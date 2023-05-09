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

    Config conf{FunL->getName().str(), FunR->getName().str(), &mod, &mod};

    auto semDiff = MinimalModuleAnalysis(conf);

    bool insidePatternValueRange = false;
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
                            PatRep->MDMap
                                    [PatternRepresentation::PATTERN_START]);
                    PatRep->isMetadataSet = true;
                } else {
                    InstL->setMetadata(
                            mod.getMDKindID("diffkemp.pattern"),
                            PatRep->MDMap
                                    [PatternRepresentation::PATTERN_START]);
                }
                InstR->setMetadata(
                        mod.getMDKindID("diffkemp.pattern"),
                        PatRep->MDMap[PatternRepresentation::PATTERN_START]);
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

    for (auto &BBL : *FunL) {
        for (auto &InstL : BBL) {
            if (auto RetInst = dyn_cast<ReturnInst>(&InstL)) {
                RetInst->setMetadata(
                        mod.getMDKindID("diffkemp.pattern"),
                        PatRep->MDMap[PatternRepresentation::PATTERN_END]);
            }
        }
    }
    for (auto &BBR : *FunR) {
        for (auto &InstR : BBR) {
            if (auto RetInst = dyn_cast<ReturnInst>(&InstR)) {
                RetInst->setMetadata(
                        mod.getMDKindID("diffkemp.pattern"),
                        PatRep->MDMap[PatternRepresentation::PATTERN_END]);
            }
        }
    }
}

bool isOpFunctionArg(Use &Op, Function &Fun) {
    for (auto argIter = Fun.arg_begin(); argIter != Fun.arg_end(); ++argIter) {
        if (Op.get() == argIter) {
            return true;
        }
    }
    return false;
}

void PatternGenerator::attachMetadata(Instruction *instr,
                                      std::string metadataStr) {
    // TODO: add some checking
    auto &instrContext = instr->getContext();
    MDNode *node =
            MDNode::get(instrContext, MDString::get(instrContext, metadataStr));
    instr->setMetadata("diffkemp.pattern", node);
}


void PatternGenerator::reduceFunctions(PatternBase &patRep) {
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
            if (hasSuffix(fun->getName().str())) {
                continue;
            }

            auto name = fun->getName().str();
            if (!(name.rfind("diffkemp.", 0) == 0)) {
                /// Name does not have diffkemp in the front
                continue;
            }
            auto otherFunName = name;
            if (name.rfind("diffkemp.old.", 0) == 0) {
                otherFunName.replace(9, 3, "new");
            } else {
                otherFunName.replace(9, 3, "old");
            }

            auto other = patRep.mod->getFunction(otherFunName);
            if (!other) {
                /// reduce
                continue;
            }
            Config conf{fun->getName().str(),
                        other->getName().str(),
                        patRep.mod.get(),
                        patRep.mod.get()};
            auto semDiff = MinimalModuleAnalysis(conf);
            if (!semDiff->compareSignature()) {
                auto newFunName = std::string(name.begin() + 14, name.end());
                auto ff = patRep.mod->getFunction(newFunName);
                if (!ff) {
                    ff = Function::Create(fun->getFunctionType(),
                                          fun->getLinkage(),
                                          newFunName,
                                          fun->getParent());
                }
                fun->replaceAllUsesWith(ff);
                other->replaceAllUsesWith(ff);
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
    Config conf{PatternFun->getName().str(),
                CandidateFun->getName().str(),
                this->patterns[patternName]->mod.get(),
                mod};
    auto semDiff = MinimalModuleAnalysis(conf);

    /// Create a temporary function, that is going to be augmented, if
    /// inference succeeds, then it is going to replace the PatternFun

    auto BBL = PatternFun->begin();
    auto BBR = CandidateFun->begin();
    Function *tmpFun = nullptr;
    std::set<std::pair<Instruction *, Value *>> markedValues;
    std::set<Value *> parametrizedValues;
    std::vector<Type *> futureParamTypes;
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
                    if (semDiff->cmpValues(OpL->get(), OpR->get())) {
                        // Skip if operand is a global variable
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
                        futureParamTypes.push_back(OpL->get()->getType());
                        if (tmpFun) {
                            tmpFun = cloneFunction(tmpFun->getParent(),
                                                   tmpFun,
                                                   "tmp.",
                                                   {OpL->get()->getType()});
                        } else {
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

    /// We want to insert even empty variants, as we want to make pairings
    this->patterns[patternName]->variants.push_back(variants);
    /// Mapping of new function arguments to new parameters
    if (tmpFun) {
        for (BBL = PatternFun->begin(), BBR = tmpFun->begin();
             BBL != PatternFun->end();
             ++BBL, ++BBR) {
            for (auto InL = BBL->begin(), InR = BBR->begin(); InL != BBL->end();
                 ++InL, ++InR) {
                for (auto &value : markedValues) {
                    if (value.first == &(*InL)) {
                        for (auto OpL = InL->op_begin(), OpR = InR->op_begin();
                             OpL != InL->op_end() && OpR != InR->op_end();
                             ++OpL, ++OpR) {
                            if (OpL->get() == value.second) {
                                auto ArgIter = tmpFun->arg_end();
                                --ArgIter;
                                OpR->get()->replaceAllUsesWith(ArgIter);
                            }
                        }
                    }
                }
            }
        }

        for (auto &var : this->patterns[patternName]->variants) {
            remapVariants(PatternFun, tmpFun, var);
        }

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

    llvm::SMDiagnostic err;
    std::unique_ptr<Module> ModL(parseIRFile(moduleFiles.first, err, firstCtx));
    std::unique_ptr<Module> ModR(
            parseIRFile(moduleFiles.second, err, secondCtx));
    if (!ModL || !ModR) {
        err.print("Diffkemp", llvm::errs());
        return false;
    }

    Function *FunL = ModL->getFunction(funNames.first);
    Function *FunR = ModR->getFunction(funNames.second);
    if (!FunL || !FunR) {
        // TODO: Add some kind of exception
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
        // TODO: Uncomment me for final version
        this->determinePatternRange(this->patterns[patternName].get(),
                                    *this->patterns[patternName]->mod.get());
        this->reduceFunctions(*(patterns[patternName].get()));
        return true;
    }

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
