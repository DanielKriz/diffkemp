#ifndef DIFFKEMP_SIMPLL_PATTERNGENERATOR_H
#define DIFFKEMP_SIMPLL_PATTERNGENERATOR_H

#include "Config.h"

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
#include <llvm/Transforms/Utils/Cloning.h>

#include <iostream>
#include <unordered_map>

class PatternGenerator {
  public:
    /// By default there is no pattern, hence it should be initialized to
    /// a nullptr.
    PatternGenerator() : ctx(), isFreshRun(true){};

    virtual ~PatternGenerator() = default;

    void addFileForInference(std::string patternName,
                             std::string funcName,
                             std::string fileName);

    friend std::ostream &operator<<(std::ostream &os, PatternGenerator &pg);

  private:
    /// Config is needed for invocation of DifferentialFunctionComparator class
    /// But it is going to be initialized everytime new function in the pattern
    /// config is found.
    // const Config &config;
    /// This has to be mutable in order to pass it to llvm::parseIRFile
    mutable llvm::LLVMContext ctx;
    mutable bool isFreshRun;
    mutable std::unique_ptr<llvm::Module> pattern;
    mutable std::unordered_map<std::string, std::unique_ptr<llvm::Module>>
            patterns;

    void cloneFunction(llvm::Function &, llvm::Function &);
};

/// TODO: Finish this, I have to add this, because I have to be able to start
/// the progrm as binary.
///
/// YAML mappings
/// I am starting to think, that those are going to be not needed as I could
/// invoke them in python, because it does not make a sense in C++ code.

// class PatternSnapshot {
//   public:
//     std::string name;
// };
// using PatternSnapshotSequence = std::vector<PatternSnapshot>;

struct PatternCandidate {
    std::string function{""};
    std::vector<llvm::StringRef> snapshots;
    PatternCandidate(){};
};

// using PatternCandidateSequence = std::vector<PatternCandidate>;
// LLVM_YAML_IS_SEQUENCE_VECTOR(PatternCandidateSequence);
// TODO: not sure about this naming
class PatternGeneratorConfig {
  public:
    std::vector<PatternCandidate> candidates;
};

#endif // DIFFKEMP_SIMPLL_PATTERNGENERATOR_H
