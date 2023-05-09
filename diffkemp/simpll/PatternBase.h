//===------------- PatternBase.h - Common part of differences -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the common part between candidate
/// differences that should act as a base for pattern variance.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNBASE_H
#define DIFFKEMP_SIMPLL_PATTERNBASE_H

#include "InstructionVariant.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

#include <iostream>
#include <utility>

/// Represents a common base between changes provided to the process of
/// inference.
class PatternBase {
    /// We want to be able to use '<<' operator with standard streams
    friend std::ostream &operator<<(std::ostream &os, PatternBase &pg);
    /// PatternGenerator should be able to access private members of
    /// PatternBase, because it owns it.
    friend class PatternGenerator;

  private:
    /// This has to be predeclared, because public Module needs it for its
    /// construction. But on the other hand, there is no reason to make the
    /// context public.
    LLVMContext context;

  public:
    PatternBase(std::string name,
                std::string funFirstName,
                std::string funSecondName);

    /// Reassigns functions held by pattern base, as parametrization requires,
    /// that we clone a function, which means that the original pointer become
    /// invalidated.
    void refreshFunctions();

    /// One pattern variant is a pair of lists of instructio variants where
    /// the first one corresponds to the new side of a pattern and the other
    /// to the old side.
    using PatternVariant = std::pair<std::vector<InstructionVariant>,
                                     std::vector<InstructionVariant>>;

    /// Generates a varint from itself with provided pattern variant
    std::pair<bool, std::unique_ptr<Module>>
            generateVariant(PatternVariant var, std::string variantSuffix = "");

    /// Applies a list of instructions variants to a function, this function
    /// is supposed to reside in different module, than the base function
    /// (because we don't want to change the base pattern function).
    void applyVariant(std::vector<InstructionVariant> &var,
                      Function *VarFun,
                      bool isLeftSide);

    /// At first, the sides of a pattern have names of the functions they came
    /// from, but at the end this could mean, that they could have two
    /// different names. This functions renames those two sides (it keeps the
    /// diffkemp prefix, so provide only the new name).
    void renameSides(std::string newName);

    /// Module which contains the base pattern functions
    std::unique_ptr<Module> mod;

    /// A pair of names of the functions that create the base pattern.
    std::pair<std::string, std::string> funNames;

    /// A pair of instance of the base pattern functions.
    std::pair<Function *, Function *> functions;

    /// A list of all variants that were created for this pattern.
    ///
    /// NOTE: this is a flaw in design, because we had designed the code around
    /// the idea, that both sides should be independent from each other, but
    /// because of that we do not have any information about the variants for
    /// the other side. As a workaround we create even empty variants and in
    /// the end this vector has even count of elements and we simply take pairs
    /// from it.
    std::vector<std::vector<InstructionVariant>> variants;

    /// Tells us, if the diffkemp specific metadata are set. That is because of
    /// LLVM, as we have to treat the metadata differently for the first time
    /// they are assigned.
    bool isMetadataSet{false};

  private:
    /// Used in pattern variation, replaces the source instruction with its
    /// variant, which changes the type used by that instruction
    void replaceStructRelatedInst(Instruction &Inst, InstructionVariant &var);

    /// Used in pattern variation, replaces the source instruction with its
    /// variant, which changes the global value used by that instruction
    void replaceGlobalRelatedInst(Instruction &inst, InstructionVariant &var);

    /// Maps the function outputs. Which means that we have to redeclare the
    /// function as returning a void and wrapping the original return
    /// instruction in a call to diffkemp.output_mapping
    void mapFunctionOutput(Function &fun);

    /// Enumeration of different types of metadata supported by the diffkemp
    enum Metadata {
        PATTERN_START,
        PATTERN_END,
        DISSABLE_NAME_COMPARISON,
        GROUP_START,
        GROUP_END,
    };

    /// Map of initialized metadata for this base pattern.
    std::unordered_map<PatternBase::Metadata, MDNode *> MDMap;

    /// An instance of diffkemp.output_mapping function declaration.
    Function *OutputMappingFun;
};

#endif // DIFFKEMP_SIMPLL_PATTERNBASE_H
