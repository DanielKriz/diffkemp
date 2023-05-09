//===------------- InstructionVariant.h - Code pattern finder -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of instruction variant class.
///
/// It also contains declarations of classes and functions required for cloning
/// of functions.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_INSTRUCTIONVARIANT_H
#define DIFFKEMP_SIMPLL_INSTRUCTIONVARIANT_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

using namespace llvm;

#include <iostream>
#include <map>

#include "Utils.h"

/// Implements ValueMapTypeRemapper which enables us to remap structure types
/// while cloning functions.
class StructTypeRemapper : public ValueMapTypeRemapper {
  private:
    /// Mapping between the source type and the destination type
    std::map<Type *, Type *> remapperMap;

  public:
    StructTypeRemapper() : remapperMap(){};
    StructTypeRemapper(std::map<Type *, Type *> newMap) : remapperMap(newMap){};

    /// Return the type to which the source type should map. In a case that
    /// there is no mapping between the source type and some destination type,
    /// then return the source type (as it should not be remapped).
    virtual inline Type *remapType(Type *srcType) override {
        for (const auto &remapping : remapperMap) {
            if (srcType == remapping.first) {
                return remapping.second;
            }
        }
        return srcType;
    }

    /// Provides ability to add new mappings.
    inline void addNewMapping(StructType *from, StructType *to) {
        remapperMap.insert({from, to});
    }

    /// Checks whether the remapper map is empty.
    inline bool empty() { return remapperMap.empty(); }
};

/// Holds information about variant of some instruction.
struct InstructionVariant {
  private:
    /// Embedded structure that should not be used outside of its parent
    /// structure. It holds information required for reproduction of global
    /// variable. As holding a pointer directly to an already initialized value
    /// could become hangling and complicate further analysis.
    struct GlobalVariableInfo {
        /// Name of the global variable.
        std::string name;
        /// Type of the global variable.
        Type *type;
        /// Linkage type tells us how the global should be linked.
        GlobalVariable::LinkageTypes linkage;
        /// Alignement of the global variable in the memory.
        MaybeAlign align;
    };

  public:
    /// The kind of the variant, we are currently distinguishing between
    /// variant based in type or global variable usage.
    /// Currently we are using C++14 which does not contain implementation of
    /// std::variant which could in the future replace this structure.
    enum Kind {
        TYPE,
        GLOBAL,
    };

    /// Ctor used for initialization of TYPE kind of InstructionVariant.
    InstructionVariant(Instruction *encounterInst,
                       Type *type,
                       unsigned opPos = 0);
    /// Ctor used for initialization of GLOBAL kind of InstructionVariant, all
    /// information required to create the GlobalVariableInfo structure are
    /// then aquired from the provided global value.
    InstructionVariant(Instruction *encounterInst,
                       GlobalVariable *global,
                       unsigned opPos = 0);

    /// Instruction from which this variant comes. We are using it for
    /// searching where to apply this variant.
    Instruction *inst;
    /// kind of the variant.
    Kind kind;
    /// If this variant is TYPE kind, then it contains the type which should
    /// replace the original type.
    Type *newType{nullptr};
    /// If this variant is GLOBAL kind, then it contains the global variable
    /// which should replace the original global variable.
    GlobalVariableInfo newGlobalInfo;
    /// Position of the to-be-replaced operand in the source instruction
    unsigned opPos;
};

/// Wrapper around llvm::cloneFunction which provides all the necessary
/// operations that are required to succesfully clone a function between
/// modules, which includes:
///     - Called function declarations
///     - Structure type remapping (otherwise they would be cloned and receive
///       a suffix)
///     - Definition of global variables (otherwise they would be undefined
///       values)
///
/// It add new arguments to the destination function or change its return type.
Function *cloneFunction(Module *dstMod,
                        Function *src,
                        std::string prefix = "",
                        std::vector<Type *> newArgs = {},
                        Type *newReturnType = nullptr,
                        StructTypeRemapper *remapper = nullptr);

/// During cloning we would lose the original pointers to instruction,
/// therefore, we have to remap them during cloning.
void remapVariants(Function *src,
                   Function *dst,
                   std::vector<InstructionVariant> &vars);

/// Remove all calls to llvm.dbg functions, because we do not want them
/// in our inference, because they should not be a part of a pattern
void removeCallsToDbgFuns(Function *Fun);

#endif // DIFFKEMP_SIMPLL_INSTRUCTIONVARIANT_H
