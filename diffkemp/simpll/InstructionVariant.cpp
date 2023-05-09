//===------------ InstructionVariant.cpp - Code pattern finder ------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definitions of instruction variant class.
///
//===----------------------------------------------------------------------===//

#include "InstructionVariant.h"

InstructionVariant::InstructionVariant(Instruction *encounterInst,
                                       Type *type,
                                       unsigned opPos)
        : inst(encounterInst), kind(InstructionVariant::TYPE), newType(type),
          opPos(opPos){};

InstructionVariant::InstructionVariant(Instruction *encounterInst,
                                       GlobalVariable *global,
                                       unsigned opPos)
        : inst(encounterInst), kind(InstructionVariant::GLOBAL), opPos(opPos) {
    newGlobalInfo = GlobalVariableInfo{};
    newGlobalInfo.type = global->getType();
    newGlobalInfo.name = global->getName().str();
    newGlobalInfo.linkage = global->getLinkage();
    newGlobalInfo.align = global->getAlign();
};

void remapVariants(Function *src,
                   Function *dst,
                   std::vector<InstructionVariant> &vars) {
    /// Find a instruction found in vars in the source function and remap it
    /// to the instruction in the destination functions. As the destination
    /// function is the clone of source function, we can expect them to have
    /// the same count of instruction at the same places.
    for (auto BBL = src->begin(), BBR = dst->begin(); BBL != src->end();
         ++BBL, ++BBR) {
        for (auto InstL = BBL->begin(), InstR = BBR->begin();
             InstL != BBL->end();
             ++InstL, ++InstR) {
            for (auto &var : vars) {
                if (var.inst == &(*InstL)) {
                    var.inst = &(*InstR);
                }
            }
        }
    }
}

/// Removes all metadata from function, because we do not want any debug
/// metadata in the analyzed function.
static void removeMetadataFromFun(Function *Fun) {
    auto &srcMod = *Fun->getParent();
    std::vector<NamedMDNode *> NMDs;
    for (NamedMDNode &NMD : srcMod.named_metadata()) {
        NMDs.push_back(&NMD);
    }
    for (auto &NMD : NMDs) {
        srcMod.eraseNamedMetadata(NMD);
    }
    SmallVector<std::pair<unsigned, MDNode *>> FunMD;
    Fun->getAllMetadata(FunMD);
    for (auto &MD : FunMD) {
        Fun->setMetadata(MD.first, nullptr);
    }
    for (auto &BB : *Fun) {
        for (auto &Inst : BB) {
            if (Inst.hasMetadata()) {
                SmallVector<std::pair<unsigned, MDNode *>> InstMD;
                Inst.getAllMetadata(InstMD);
                for (auto &MD : InstMD) {
                    Inst.setMetadata(MD.first, nullptr);
                }
            }
        }
    }
}

void removeCallsToDbgFuns(Function *Fun) {
    std::vector<CallInst *> callInstsToRemove = {};
    for (auto &BB : *Fun) {
        for (auto &Inst : BB) {
            if (auto InstCall = dyn_cast<CallInst>(&Inst)) {
                if (InstCall->getCalledFunction()->getName().contains(
                            "llvm.dbg")) {
                    callInstsToRemove.push_back(InstCall);
                }
            }
        }
    }
    for (auto &Inst : callInstsToRemove) {
        Inst->eraseFromParent();
    }
}

Function *cloneFunction(Module *dstMod,
                        Function *src,
                        std::string prefix,
                        std::vector<Type *> newArgTypes,
                        Type *newReturnType,
                        StructTypeRemapper *remapper) {
    removeMetadataFromFun(src);

    /// Merge new function argument types with new types and popule
    /// one from the source with value mapping between source and
    /// destination functions.
    auto newFunTypeParams = src->getFunctionType()->params().vec();
    for (auto &newArgType : newArgTypes) {
        newFunTypeParams.push_back(newArgType);
    }
    FunctionType *newFunType;
    if (!newReturnType) {
        newFunType = FunctionType::get(
                src->getReturnType(), newFunTypeParams, false);
    } else {
        newFunType = FunctionType::get(newReturnType, newFunTypeParams, false);
    }
    auto dst = Function::Create(
            newFunType, src->getLinkage(), prefix + src->getName(), dstMod);
    llvm::ValueToValueMapTy tmpValueMap;
    auto patternFuncArgIter = dst->arg_begin();
    for (auto &arg : src->args()) {
        patternFuncArgIter->setName(arg.getName());
        tmpValueMap[&arg] = &(*patternFuncArgIter++);
    }

    removeCallsToDbgFuns(src);

    auto tmpRemapper = std::make_unique<StructTypeRemapper>();
    if (!remapper) {
        /// If function contains any structure types, that are already present
        /// in the destination context (comparison is done by name only), then
        /// remap instances of the source structure type to destination contexts
        /// structure type.
        std::vector<StructType *> foundStructTypes;
        for (auto &BB : *src) {
            for (auto &Inst : BB) {
                if (auto InstAlloca = dyn_cast<AllocaInst>(&Inst)) {
                    if (auto StType = dyn_cast<StructType>(
                                InstAlloca->getAllocatedType())) {
                        foundStructTypes.push_back(StType);
                    }
                } else if (auto InstGEP = dyn_cast<GetElementPtrInst>(&Inst)) {
                    if (auto StType = dyn_cast<StructType>(
                                InstGEP->getSourceElementType())) {
                        foundStructTypes.push_back(StType);
                    }
                }
            }
        }
        for (auto &StructL : dstMod->getIdentifiedStructTypes()) {
            /// We want to prevent double mapping
            auto structIter = foundStructTypes.begin();
            while (structIter != foundStructTypes.end()) {
                if ((*structIter)->getStructName()
                    == StructL->getStructName()) {
                    tmpRemapper->addNewMapping(*structIter, StructL);
                    foundStructTypes.erase(structIter);
                } else {
                    ++structIter;
                }
            }
        }

        /// Check if source module has any structure types that have suffix,
        /// that is because of the shared context of read IR. Try to remap the
        /// structure to it's variant without suffix in the destination module.
        ///
        /// If there is non, then create it and then remap source to it.
        for (auto &sTypeL : src->getParent()->getIdentifiedStructTypes()) {
            if (hasSuffix(sTypeL->getStructName().str())) {
                auto nameWithoutSuffix =
                        dropSuffix(sTypeL->getStructName().str());
                bool found = false;
                for (auto &sTypeR : dstMod->getIdentifiedStructTypes()) {
                    if (nameWithoutSuffix == sTypeR->getStructName().str()) {
                        tmpRemapper->addNewMapping(sTypeL, sTypeR);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    auto sTypeR = StructType::create(dstMod->getContext(),
                                                     sTypeL->elements(),
                                                     nameWithoutSuffix);
                    tmpRemapper->addNewMapping(sTypeL, sTypeR);
                }
            }
        }
        if (!tmpRemapper->empty()) {
            remapper = tmpRemapper.get();
        }
    }

    /// Builtin clone functions needs to know whether cloning appears in the
    /// same module.
    CloneFunctionChangeType changeType =
            (dstMod == src->getParent()
                     ? CloneFunctionChangeType::LocalChangesOnly
                     : CloneFunctionChangeType::DifferentModule);

    SmallVector<llvm::ReturnInst *, 8> returns;
    CloneFunctionInto(
            dst, src, tmpValueMap, changeType, returns, "", nullptr, remapper);
    /// Provide declaration of used functions
    for (auto &BB : *dst) {
        for (auto &Inst : BB) {
            if (auto call = dyn_cast<CallInst>(&Inst)) {
                auto calledFun = call->getCalledFunction();
                bool found = false;
                for (auto &func : dstMod->functions()) {
                    if (prefix + calledFun->getName().str()
                        == func.getName().str()) {
                        call->setCalledFunction(&func);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    call->setCalledFunction(
                            Function::Create(calledFun->getFunctionType(),
                                             calledFun->getLinkage(),
                                             prefix + calledFun->getName(),
                                             dst->getParent()));
                }
            }
        }
    }

    for (auto &globalL : src->getParent()->globals()) {
        bool found = false;
        for (auto &globalR : dstMod->globals()) {
            if (globalL.getName() == globalR.getName()) {
                found = true;
            }
        }
        if (!found) {
            auto newGlobal = new GlobalVariable(*dstMod,
                                                globalL.getType(),
                                                globalL.isConstant(),
                                                globalL.getLinkage(),
                                                nullptr,
                                                globalL.getName());
            newGlobal->setAlignment(globalL.getAlign());
            globalL.replaceAllUsesWith(newGlobal);
        }
    }

    return dst;
}
