//===----------- PatternGenerator.cpp - Code Pattern Generator ------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definitions of the custom code change pattern
/// generator.
///
//===----------------------------------------------------------------------===//

#include "PatternGenerator.h"

PatternGenerator::MinimalModuleAnalysis::MinimalModuleAnalysis(Config &conf) {
    conf.refreshFunctions();
    /// All of these are not inside of the member initializer list,
    /// because we want to be sure, that functions are refreshed in
    /// conf.
    dbgInfo = std::make_unique<DebugInfo>(*conf.First,
                                          *conf.Second,
                                          conf.FirstFun,
                                          conf.SecondFun,
                                          CalledFirst,
                                          CalledSecond);
    modComp = std::make_unique<ModuleComparator>(*conf.First,
                                                 *conf.Second,
                                                 conf,
                                                 dbgInfo.get(),
                                                 StructSizeMapL,
                                                 StructSizeMapR,
                                                 StructDIMapL,
                                                 StructDIMapR);
    diffComp = std::make_unique<DifferentialFunctionComparator>(
            conf.FirstFun,
            conf.SecondFun,
            conf,
            dbgInfo.get(),
            &modComp.get()->Patterns,
            modComp.get());
    addFunPair(std::make_pair(conf.FirstFun, conf.SecondFun));
}

void PatternGenerator::determinePatternRange(PatternBase *PatRep, Module &mod) {
    auto FunL = mod.getFunction(PatRep->functions.first->getName());
    auto FunR = mod.getFunction(PatRep->functions.second->getName());
    if (!FunL && !FunR) {
        return;
    }

    /// Initialize minimal module analysis
    Config conf{FunL->getName().str(), FunR->getName().str(), &mod, &mod};
    auto semDiff = MinimalModuleAnalysis(conf);

    /// Indicates whether we have found the first differing instruction =>
    /// we are inside of the pattern range
    bool insidePatternValueRange = false;
    /// Search for the first differing instruction
    for (auto BBL = FunL->begin(), BBR = FunR->begin();
         BBL != FunL->end() || BBR != FunR->end();
         ++BBL, ++BBR) {
        for (auto InstL = BBL->begin(), InstR = BBR->begin();
             InstL != BBL->end() || InstR != BBR->end();
             ++InstL, ++InstR) {
            if (semDiff->cmpOperationsWithOperands(&(*InstL), &(*InstR)) != 0) {
                if (!PatRep->isMetadataSet || &mod != PatRep->mod.get()) {
                    /// Metadata needs to be registered in the module, but
                    /// for some reason they are not accessible the first
                    /// time that they are used.
                    InstL->setMetadata(
                            "diffkemp.pattern",
                            PatRep->MDMap[PatternBase::PATTERN_START]);
                    PatRep->isMetadataSet = true;
                } else {
                    InstL->setMetadata(
                            mod.getMDKindID("diffkemp.pattern"),
                            PatRep->MDMap[PatternBase::PATTERN_START]);
                }
                InstR->setMetadata(mod.getMDKindID("diffkemp.pattern"),
                                   PatRep->MDMap[PatternBase::PATTERN_START]);
                insidePatternValueRange = true;
                break;
            }
        }
        if (insidePatternValueRange) {
            break;
        }
    }

    if (!insidePatternValueRange) {
        return;
    }

    /// Find all return instruction in both functions and attach pattern-end
    /// metadata to them.
    for (auto &BBL : *FunL) {
        for (auto &InstL : BBL) {
            if (auto RetInst = dyn_cast<ReturnInst>(&InstL)) {
                RetInst->setMetadata(mod.getMDKindID("diffkemp.pattern"),
                                     PatRep->MDMap[PatternBase::PATTERN_END]);
            }
        }
    }
    for (auto &BBR : *FunR) {
        for (auto &InstR : BBR) {
            if (auto RetInst = dyn_cast<ReturnInst>(&InstR)) {
                RetInst->setMetadata(mod.getMDKindID("diffkemp.pattern"),
                                     PatRep->MDMap[PatternBase::PATTERN_END]);
            }
        }
    }
}

/// Checks whether operand is a function arguemnt
static bool isOpFunctionArg(Use &Op, Function &Fun) {
    for (auto argIter = Fun.arg_begin(); argIter != Fun.arg_end(); ++argIter) {
        if (Op.get() == argIter) {
            return true;
        }
    }
    return false;
}

void PatternGenerator::attachMetadata(Instruction *instr,
                                      std::string metadataStr) {
    auto &instrContext = instr->getContext();
    MDNode *node =
            MDNode::get(instrContext, MDString::get(instrContext, metadataStr));
    instr->setMetadata("diffkemp.pattern", node);
}

void PatternGenerator::reduceFunctions(PatternBase &patRep) {
    /// get all names of function in a modlue
    std::vector<std::string> funNames;
    for (auto &fun : patRep.mod->getFunctionList()) {
        funNames.push_back(fun.getName().str());
    }
    for (auto &funName : funNames) {
        auto fun = patRep.mod->getFunction(funName);
        if (fun) {
            /// We don't want to reduce the pattern functions, only the
            /// supporting ones.
            if (fun == patRep.functions.first
                || fun == patRep.functions.second) {
                continue;
            }

            /// Skip functions that have suffix
            if (hasSuffix(fun->getName().str())) {
                continue;
            }

            auto FunName = fun->getName();
            /// Check whether function contains the old or new prefix
            if (!(FunName.contains("diffkemp.old")
                  || FunName.contains("diffkemp.new"))) {
                continue;
            }

            /// We need to use the replace function, which is only available
            /// to the std::string, unfortunately StringRef::str() only
            /// returns copy fo the kept string
            auto OtherFunName = FunName.str();
            if (FunName.contains("diffkemp.old")) {
                OtherFunName.replace(FunName.find("old"), 3, "new");
            } else {
                OtherFunName.replace(FunName.find("new"), 3, "old");
            }

            auto other = patRep.mod->getFunction(OtherFunName);
            /// There is not other function, therefore the other side does not
            /// contain a function of the same name and we don't have to reduce
            /// it.
            if (!other) {
                continue;
            }
            Config conf{fun->getName().str(),
                        other->getName().str(),
                        patRep.mod.get(),
                        patRep.mod.get()};

            /// Compare whether the signature are same for the both sides
            auto semDiff = MinimalModuleAnalysis(conf);
            if (!semDiff->compareSignature()) {
                auto newFunName = std::string(
                        FunName.begin() + StringRef("diffkemp.old.").size(),
                        FunName.end());
                auto NewFun = patRep.mod->getFunction(newFunName);
                if (!NewFun) {
                    NewFun = Function::Create(fun->getFunctionType(),
                                              fun->getLinkage(),
                                              newFunName,
                                              fun->getParent());
                }
                /// replace the old function with the new one
                fun->replaceAllUsesWith(NewFun);
                other->replaceAllUsesWith(NewFun);
                fun->eraseFromParent();
                other->eraseFromParent();
            }
        }
    }
}

bool PatternGenerator::addFunctionToPattern(Module *mod,
                                            Function *PatternFun,
                                            Function *CandidateFun,
                                            std::string patternName) {
    /// Initialize minimal module analysis
    Config conf{PatternFun->getName().str(),
                CandidateFun->getName().str(),
                this->patterns[patternName]->mod.get(),
                mod};
    auto semDiff = MinimalModuleAnalysis(conf);

    /// Remove all calls to debug function in the candidate difference function.
    removeCallsToDbgFuns(CandidateFun);

    auto BBL = PatternFun->begin();
    auto BBR = CandidateFun->begin();

    /// Create a temporary function, that is going to be augmented, if
    /// inference succeeds, then it is going to replace the PatternFun
    Function *tmpFun = nullptr;

    /// Information about the state of the analysis. We keep track about the
    /// values marked for parametrization, already encountered parameteres and
    /// parameterized values together with future parameter type.
    std::set<std::pair<Instruction *, Value *>> markedValues;
    std::set<std::pair<Value *, Value *>> encounteredParameters;
    std::set<Value *> parametrizedValues;
    std::vector<Type *> futureParamTypes;

    /// List of instruction variants found for this candidate function.
    std::vector<InstructionVariant> variants;
    while (BBL != PatternFun->end() && BBR != CandidateFun->end()) {
        auto InL = BBL->begin();
        auto InR = BBR->begin();
        while (InL != BBL->end() || InR != BBR->end()) {
            if (InL->getOpcode() != InR->getOpcode()) {
                /// We cannot create a pattern from code snippets with
                /// differing operations.
                return false;
            }

            /// Alloca and GEP instruction could contain structure types,
            /// therefore, we are analysing them first.
            auto AllocaL = dyn_cast<AllocaInst>(InL);
            auto AllocaR = dyn_cast<AllocaInst>(InR);
            if (AllocaL && AllocaR) {
                auto StructL =
                        dyn_cast<StructType>(AllocaL->getAllocatedType());
                auto StructR =
                        dyn_cast<StructType>(AllocaR->getAllocatedType());
                if (StructL != nullptr && StructR != nullptr) {
                    if (StructL != StructR) {
                        variants.emplace_back(&(*InL), StructR, 0);
                    }
                }
            }
            auto GepL = dyn_cast<GetElementPtrInst>(InL);
            auto GepR = dyn_cast<GetElementPtrInst>(InR);
            if (GepL && GepR) {
                auto StructL =
                        dyn_cast<StructType>(GepL->getSourceElementType());
                auto StructR =
                        dyn_cast<StructType>(GepR->getSourceElementType());
                if (StructL != nullptr && StructR != nullptr) {
                    if (StructL != StructR) {
                        variants.emplace_back(&(*InL), StructR, 0);
                    }
                }
            }
            if (!semDiff->cmpOperationsWithOperands(&(*InL), &(*InR))) {
                ++InL, ++InR;
                continue;
            }
            /// Ignore call instructions, as they usually call to different
            /// functions
            if (dyn_cast<CallInst>(InL)) {
                ++InL, ++InR;
                continue;
            }
            /// Same operations shouldn't have different operands count
            unsigned i = 0;
            for (auto OpL = InL->op_begin(), OpR = InR->op_begin();
                 OpL != InL->op_end() && OpR != InR->op_end();
                 ++OpL, ++OpR) {
                /// Elementar types usually use different instructions,
                /// hence at this point they are already out of the game.
                auto OpTypeL = OpL->get()->getType();
                auto OpTypeR = OpR->get()->getType();
                if (OpTypeL != OpTypeR) {
                    std::cout << "Not Implemented" << std::endl;
                } else {
                    /// Check whether the difference is in the value
                    if (semDiff->cmpValues(OpL->get(), OpR->get())) {
                        /// If the value in the base pattern is a global value,
                        /// then we create an instruction variant.
                        if (isValueGlobal(*OpL->get(),
                                          *PatternFun->getParent())) {
                            GlobalVariable *gvar =
                                    dyn_cast<GlobalVariable>(OpR->get());
                            variants.emplace_back(&(*InL), gvar, i);
                            ++i;
                            continue;
                        }
                        /// Value is already parametrized
                        if (parametrizedValues.find(OpL->get())
                            != parametrizedValues.end()) {
                            ++i;
                            continue;
                        }
                        /// difference is in the value
                        /// Marked for future copy
                        auto pp = std::make_pair(&(*InL), OpL->get());
                        if (isOpFunctionArg(*OpL, *PatternFun)
                            || std::find(markedValues.begin(),
                                         markedValues.end(),
                                         pp)
                                       != markedValues.end()) {
                            ++i;
                            continue;
                        }
                        /// Collect information about parametrization
                        futureParamTypes.push_back(OpL->get()->getType());
                        if (tmpFun) {
                            /// Temporary function is already initialzed and
                            /// we can mutate it in place.
                            tmpFun = cloneFunction(tmpFun->getParent(),
                                                   tmpFun,
                                                   "tmp.",
                                                   {OpL->get()->getType()});
                        } else {
                            /// Temporary function is not yet initialized and
                            /// because of that we have to initialize it and
                            /// clone the pattern function into it.
                            tmpFun = cloneFunction(PatternFun->getParent(),
                                                   PatternFun,
                                                   "tmp.",
                                                   {OpL->get()->getType()});
                        }
                        markedValues.insert(
                                std::make_pair(&(*InL), OpL->get()));
                        parametrizedValues.insert(OpL->get());
                    }
                }
                ++i;
            }
            InR++;
            InL++;
        }
        BBR++;
        BBL++;
    }

    /// We want to insert even empty variants, as we want to make pairs
    this->patterns[patternName]->variants.push_back(variants);
    /// Mapping of new function arguments to new parameters
    if (tmpFun) {
        for (BBL = PatternFun->begin(), BBR = tmpFun->begin();
             BBL != PatternFun->end();
             ++BBL, ++BBR) {
            for (auto InL = BBL->begin(), InR = BBR->begin(); InL != BBL->end();
                 ++InL, ++InR) {
                /// We have to check for each value that is marked for
                /// parametrization
                for (auto &value : markedValues) {
                    /// Check if we  have found the source instruction of the
                    /// to-be-parametrized value
                    if (value.first == &(*InL)) {
                        for (auto OpL = InL->op_begin(), OpR = InR->op_begin();
                             OpL != InL->op_end() && OpR != InR->op_end();
                             ++OpL, ++OpR) {
                            if (OpL->get() == value.second) {
                                /// We have to add the new parameter at the end
                                /// of argument list.
                                auto ArgIter = tmpFun->arg_end();
                                --ArgIter;
                                OpR->get()->replaceAllUsesWith(ArgIter);
                            }
                        }
                    }
                }
            }
        }

        /// Remap instruction variants to the new function
        for (auto &var : this->patterns[patternName]->variants) {
            remapVariants(PatternFun, tmpFun, var);
        }

        /// Rename the tmp function and replace all usages of the source one.
        auto pastFunName = PatternFun->getName();
        PatternFun->replaceAllUsesWith(tmpFun);
        PatternFun->eraseFromParent();
        tmpFun->setName(pastFunName);
    }
    return true;
}

bool PatternGenerator::addFunctionPairToPattern(
        std::pair<std::string, std::string> moduleFiles,
        std::pair<std::string, std::string> funNames,
        std::string patternName) {

    /// Get the modules of the analyzed functions
    llvm::SMDiagnostic err;
    std::unique_ptr<Module> ModL(parseIRFile(moduleFiles.first, err, firstCtx));
    std::unique_ptr<Module> ModR(
            parseIRFile(moduleFiles.second, err, secondCtx));
    if (!ModL || !ModR) {
        err.print("Diffkemp", llvm::errs());
        return false;
    }

    /// Get the analyzed functions
    Function *FunL = ModL->getFunction(funNames.first);
    Function *FunR = ModR->getFunction(funNames.second);
    if (!FunL || !FunR) {
        errs() << "DiffKemp: Could not read functions"
               << "\n";
        err.print("Diffkemp", llvm::errs());
        return false;
    }

    /// This pattern is not yet initialized, and we have to initialize it by
    /// copying module functions to it.
    if (this->patterns.find(patternName) == this->patterns.end()) {
        const std::string oldPrefix = "diffkemp.old.";
        const std::string newPrefix = "diffkemp.new.";
        this->patterns[patternName] = std::make_unique<PatternBase>(
                patternName,
                (oldPrefix + FunL->getName()).str(),
                (newPrefix + FunR->getName()).str());
        auto PatternRepr = this->patterns[patternName].get();
        PatternRepr->functions.first =
                cloneFunction(PatternRepr->mod.get(), FunL, oldPrefix);
        PatternRepr->functions.second =
                cloneFunction(PatternRepr->mod.get(), FunR, newPrefix);

        auto attrs = AttributeList();
        PatternRepr->functions.first->setAttributes(attrs);
        PatternRepr->functions.second->setAttributes(attrs);

        /// Pattern was empty, hence provided functions are the most
        /// specific pattern that we can infere, thus generation have
        /// been successful and we are returning true.
        this->determinePatternRange(this->patterns[patternName].get(),
                                    *this->patterns[patternName]->mod.get());
        this->reduceFunctions(*(patterns[patternName].get()));
        return true;
    }

    /// Pattern is already initialized and we can add function to inference.
    auto resultL = this->addFunctionToPattern(
            ModL.get(),
            this->patterns[patternName]->functions.first,
            FunL,
            patternName);
    auto resultR = this->addFunctionToPattern(
            ModR.get(),
            this->patterns[patternName]->functions.second,
            FunR,
            patternName);

    this->patterns[patternName]->refreshFunctions();
    this->determinePatternRange(this->patterns[patternName].get(),
                                *this->patterns[patternName]->mod.get());
    this->reduceFunctions(*(patterns[patternName].get()));

    if (!resultL || !resultR) {
        this->patterns.erase(patternName);
        return false;
    }
    return true;
}

bool PatternGenerator::isValueGlobal(Value &val, Module &mod) {
    for (auto &global : mod.globals()) {
        if (val.getValueID() == global.getValueID()) {
            return true;
        }
    }
    return false;
}

std::ostream &operator<<(std::ostream &os, PatternBase &pat) {
    std::string tmpStr;
    raw_string_ostream tmp(tmpStr);
    tmp << *(pat.mod);
    os << tmpStr;
    return os;
}
