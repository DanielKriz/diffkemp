#ifndef DIFFKEMP_SIMPLL_PATTERNGENERATOR_H
#define DIFFKEMP_SIMPLL_PATTERNGENERATOR_H

#include "Config.h"
#include "DebugInfo.h"
#include "DifferentialFunctionComparator.h"
#include "InstructionVariant.h"
#include "ModuleAnalysis.h"
#include "Output.h"
#include "PatternBase.h"
#include "Result.h"
#include "Utils.h"
#include "passes/StructureSizeAnalysis.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/MemorySSAUpdater.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLTraits.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace llvm;

#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>

class PatternGenerator {
  public:
    /// By default there is no pattern, hence it should be initialized to
    /// a nullptr.
    PatternGenerator() : firstCtx(), secondCtx(){};

    virtual ~PatternGenerator() = default;

    void addFileForInference(std::string patternName,
                             std::string funcName,
                             std::string fileName);

    bool addFunctionToPattern(Module *mod,
                              Function *PaternFun,
                              Function *CandidateFun,
                              std::string patterName);

    [[nodiscard]] bool addFunctionPairToPattern(
            std::pair<std::string, std::string> moduleFiles,
            std::pair<std::string, std::string> funNames,
            std::string patternName);

    PatternBase *operator[](std::string patternName) {
        return patterns[patternName].get();
    }

    void determinePatternRange(PatternBase *PatBase, Module &mod);

  private:
    class MinimalModuleAnalysis {
      public:
        DifferentialFunctionComparator *operator->() const {
            return diffComp.get();
        }
        inline void addFunPair(std::pair<Function *, Function *> fPair) {
            modComp->ComparedFuns.emplace(fPair,
                                          Result(fPair.first, fPair.second));
        }
        MinimalModuleAnalysis(Config &conf);

      private:
        std::unique_ptr<DebugInfo> dbgInfo;
        std::unique_ptr<ModuleComparator> modComp;
        std::unique_ptr<DifferentialFunctionComparator> diffComp;

        /// Objects needed for creation of minimal ModuleAnalysis, they are
        /// initialized inline, because we want them to have default values.
        std::set<const Function *> CalledFirst{};
        std::set<const Function *> CalledSecond{};
        StructureSizeAnalysis::Result StructSizeMapL{};
        StructureSizeAnalysis::Result StructSizeMapR{};
        StructureDebugInfoAnalysis::Result StructDIMapL{};
        StructureDebugInfoAnalysis::Result StructDIMapR{};
    };
    /// This has to be mutable in order to pass it to llvm::parseIRFile
    mutable LLVMContext firstCtx;
    mutable LLVMContext secondCtx;

    /// Store of base patterns.
    std::map<std::string, std::unique_ptr<PatternBase>> patterns;

    // DifferentialFunctionComparator diffFunComp;
    void attachMetadata(Instruction *instr, std::string metadataStr);

    /// Functions should be marked as new and old only if they differ, therefore
    /// this functions checks signatures of all functions and if they are the
    /// same except for name prefix, then they are going to be reduced.
    void reduceFunctions(PatternRepresentation &patRep);

    bool isValueGlobal(Value &val, Module &mod);
};

struct CandidateDifference {
    StringRef function{""};
    StringRef oldSnapshotPath;
    StringRef newSnapshotPath;
};

struct PatternInfo {
    std::string name;
    std::vector<CandidateDifference> candidates;
};

namespace llvm {
namespace yaml {
template <> struct MappingTraits<CandidateDifference> {
    static void mapping(IO &io, CandidateDifference &candidate) {
        io.mapRequired("name", candidate.function);
        io.mapRequired("old_snapshot_path", candidate.oldSnapshotPath);
        io.mapRequired("new_snapshot_path", candidate.newSnapshotPath);
    }
};
} // namespace yaml
} // namespace llvm
LLVM_YAML_IS_SEQUENCE_VECTOR(CandidateDifference);

namespace llvm {
namespace yaml {
template <> struct MappingTraits<PatternInfo> {
    static void mapping(IO &io, PatternInfo &info) {
        io.mapRequired("name", info.name);
        io.mapRequired("candidates", info.candidates);
    }
};
} // namespace yaml
} // namespace llvm
LLVM_YAML_IS_SEQUENCE_VECTOR(PatternInfo);

#endif // DIFFKEMP_SIMPLL_PATTERNGENERATOR_H
