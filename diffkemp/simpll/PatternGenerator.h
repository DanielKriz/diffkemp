//===------------ PatternGenerator.h - Code Pattern Generator -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the custom code change pattern
/// generator.
///
//===----------------------------------------------------------------------===//

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

/// Generates code change patterns from provided changes.
class PatternGenerator {
  public:
    PatternGenerator() : firstCtx(), secondCtx(){};
    virtual ~PatternGenerator() = default;

    /// Adds a function to an inference, which means that it compares the
    /// candidate function with the pattern functions and tries to find
    /// differences and eventually parametrized them.
    bool addFunctionToPattern(Module *mod,
                              Function *PaternFun,
                              Function *CandidateFun,
                              std::string patterName);

    /// Adds a pair of functions to the base pattern identified by name.
    [[nodiscard]] bool addFunctionPairToPattern(
            std::pair<std::string, std::string> moduleFiles,
            std::pair<std::string, std::string> funNames,
            std::string patternName);

    PatternBase *operator[](std::string patternName) {
        return patterns[patternName].get();
    }

    /// Adds diffkemp specific metadata to the base pattern.
    void determinePatternRange(PatternBase *PatBase, Module &mod);

  private:
    /// Embedded class which encapsulates functionality of semantic comparison
    /// with DifferentialFunctionComparator. DFC was not designed to be used
    /// in a different parts of the codebase. This is a simple workaround that
    /// passes a few empty structures, because we do not need all the
    /// information it provides.
    class MinimalModuleAnalysis {
      public:
        /// Simple accessor to the DFC. Defining a '->' operator is safe in
        /// this case, because guidelines say that it has to return an object
        /// which also has '->' defined (and pointers adhere to that).
        DifferentialFunctionComparator *operator->() const {
            return diffComp.get();
        }
        /// We have to add function pair to the analysis, otherwise DFC cannot
        /// find it and program crashes.
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
    /// We have to keep contexts for parsing LLVM IR files and then copying
    /// functions from them
    LLVMContext firstCtx;
    LLVMContext secondCtx;

    /// Store of base patterns.
    std::map<std::string, std::unique_ptr<PatternBase>> patterns;

    // DifferentialFunctionComparator diffFunComp;
    void attachMetadata(Instruction *instr, std::string metadataStr);

    /// Functions should be marked as new and old only if they differ, therefore
    /// this functions checks signatures of all functions and if they are the
    /// same except for name prefix, then they are going to be reduced.
    void reduceFunctions(PatternBase &patRep);

    /// Checks whether a value is a global value in a module.
    bool isValueGlobal(Value &val, Module &mod);
};

/// Holds information about candidate difference.
struct CandidateDifference {
    /// Name of the function that should be added during pattern inference.
    StringRef function{""};
    /// Path to the old snapshot, which should be generated by the diffkemp
    /// compare command.
    StringRef oldSnapshotPath;
    /// Path to the new snapshot, which should be generated by the diffkemp
    /// compare command.
    StringRef newSnapshotPath;
};

/// Holds information about a pattern that user wants to generate.
struct PatternInfo {
    /// Name of the desired pattern.
    std::string name;
    /// A sequence of candidate differences associated with this pattern.
    std::vector<CandidateDifference> candidates;
};

namespace llvm {
namespace yaml {
/// Mapping of the candidate difference to and from a YAML.
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
/// Mapping of the pattern information to and from a YAML.
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
