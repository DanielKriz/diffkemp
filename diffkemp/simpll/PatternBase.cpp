#include "PatternBase.h"

PatternBase::PatternBase(std::string name,
                         std::string funFirstName,
                         std::string funSecondName)
        : context(), mod(new Module(name, context)),
          funNames(funFirstName, funSecondName), functions(nullptr, nullptr),
          MDMap({{PATTERN_START,
                  MDNode::get(context,
                              MDString::get(context, "pattern-start"))},
                 {PATTERN_END,
                  MDNode::get(context,
                              MDString::get(context, "pattern-end"))}}) {
    auto outputMappingFunType =
            FunctionType::get(Type::getVoidTy(context), true);
    OutputMappingFun =
            Function::Create(outputMappingFunType,
                             GlobalValue::LinkageTypes::ExternalLinkage,
                             "diffkemp.output_mapping",
                             mod.get());
};

void PatternBase::refreshFunctions() {
    functions.first = mod->getFunction(funNames.first);
    functions.second = mod->getFunction(funNames.second);
}

void PatternBase::mapFunctionOutput(Function &fun) {
    for (auto &BB : fun) {
        for (auto &Inst : BB) {
            /// Checks whether the instruction is a return instruction.
            if (auto RetInst = dyn_cast<ReturnInst>(&Inst)) {
                CallInst::Create(OutputMappingFun->getFunctionType(),
                                 OutputMappingFun,
                                 ArrayRef<Value *>(RetInst->getReturnValue()),
                                 "",
                                 RetInst);
                ReplaceInstWithInst(
                        RetInst,
                        ReturnInst::Create(fun.getContext(), nullptr, RetInst));
                break;
            }
        }
    }
}

void PatternBase::renameSides(std::string newName) {
    for (auto &Fun : {functions.first, functions.second}) {
        StringRef FunName = Fun->getName();
        /// At this point we have sides of the pattern, therefore, we can
        /// expect that the names have diffkemp.(old|new). prefix
        Fun->setName(FunName.str().replace(
                FunName.rfind('.') + 1, FunName.size() - 1, newName));
    }
    funNames.first = functions.first->getName().str();
    funNames.second = functions.second->getName().str();
}

std::pair<bool, std::unique_ptr<Module>>
        PatternBase::generateVariant(PatternBase::PatternVariant var,
                                     std::string variantSuffix) {

    auto varMod = std::make_unique<Module>(mod->getName().str() + variantSuffix,
                                           this->context);

    /// It is impossible to have an odd number of variants, as we are inserting
    /// even empty ones.
    assert(variants.size() % 2 == 0);

    /// Perform no variation, if both variation vectors are empty.
    if (var.first.empty() && var.second.empty()) {
        return std::make_pair(false, std::move(varMod));
    }
    /// Clone functions from the base pattern to the module that represents
    /// the pattern variant instance.
    auto VarFunL = cloneFunction(varMod.get(), functions.first, "", {});
    // Type::getVoidTy(varMod->getContext()));
    auto VarFunR = cloneFunction(varMod.get(), functions.second, "", {});
    // Type::getVoidTy(varMod->getContext()));

    applyVariant(var.first, VarFunL, true);
    applyVariant(var.second, VarFunR, false);

    /// Apply output mapping
    // mapFunctionOutput(*VarFunL);
    // mapFunctionOutput(*VarFunR);

    return std::make_pair(true, std::move(varMod));
}

void PatternBase::replaceStructRelatedInst(Instruction &Inst,
                                           InstructionVariant &var) {
    /// Structure types are located in alloca or GEP instructions. We are
    /// cheking for them and then copying them to a new instruction, that is
    /// using the new type. Then we are deleting the orignal instruction
    /// (which means that we should be working with instruction from the
    /// pattern variant module)
    if (auto InstAlloca = dyn_cast<AllocaInst>(&Inst)) {
        auto NewInstAlloca = new AllocaInst(var.newType, 0, "", &Inst);
        NewInstAlloca->setAlignment(InstAlloca->getAlign());
        InstAlloca->replaceAllUsesWith(NewInstAlloca);
        InstAlloca->eraseFromParent();
    } else if (auto InstGep = dyn_cast<GetElementPtrInst>(&Inst)) {
        std::vector<Value *> operands;
        for (auto &op : InstGep->operands()) {
            operands.push_back(op);
        }
        /// Structure type is always the first operand, the other operands are
        /// just offsets related to the structure type.
        auto ptr = operands[0];
        operands.erase(operands.begin());
        auto NewInstGep = GetElementPtrInst::Create(
                var.newType, ptr, operands, "", InstGep);
        InstGep->replaceAllUsesWith(NewInstGep);
        InstGep->eraseFromParent();
    }
}

void PatternBase::replaceGlobalRelatedInst(Instruction &Inst,
                                           InstructionVariant &var) {
    /// Find the global value in the instruction and replace that value with
    /// a new global value specified by the pattern variant.
    /// (which means that we should be working with instruction from the
    /// pattern variant module)
    GlobalVariable *newGlobal = nullptr;
    auto varMod = Inst.getParent()->getParent()->getParent();
    /// Search for the new global in the original module, to see whether we
    /// have copied it before, or not.
    for (auto &global : mod->globals()) {
        if (var.newGlobalInfo.name == global.getName()) {
            newGlobal = &global;
            break;
        }
    }
    /// Create a new global variable in the variant module.
    if (!newGlobal) {
        newGlobal = new GlobalVariable(*varMod,
                                       var.newGlobalInfo.type,
                                       false,
                                       var.newGlobalInfo.linkage,
                                       0,
                                       var.newGlobalInfo.name);
        newGlobal->setAlignment(var.newGlobalInfo.align);
    }
    Inst.getOperandUse(var.opPos).set(newGlobal);
}

void PatternBase::applyVariant(std::vector<InstructionVariant> &vars,
                               Function *VarFun,
                               bool isLeftSide) {
    /// Assign the function in dependence to the side we are currently
    /// augmenting.
    Function *Fun = isLeftSide ? functions.first : functions.second;

    /// Find corresponding instructions and mutate them
    for (auto &var : vars) {
        bool found = false;
        for (auto BBL = Fun->begin(), BBR = VarFun->begin(); BBL != Fun->end();
             ++BBL, ++BBR) {
            for (auto InstL = BBL->begin(), InstR = BBR->begin();
                 InstL != BBL->end();
                 ++InstL, ++InstR) {
                if (&(*InstL) == var.inst) {
                    switch (var.kind) {
                    case InstructionVariant::TYPE:
                        replaceStructRelatedInst(*InstR, var);
                        break;
                    case InstructionVariant::GLOBAL:
                        replaceGlobalRelatedInst(*InstR, var);
                        break;
                    }
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
}
