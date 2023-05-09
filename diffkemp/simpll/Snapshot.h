//===-------------- Snapshot.h - Representation of snapshot ---------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the representation of a snapshot,
/// which is the output of compare phase of DiffKemp.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_SNAPSHOT_H
#define DIFFKEMP_SIMPLL_SNAPSHOT_H

#include <llvm/Support/YAMLTraits.h>

using namespace llvm;

#include <iostream>
#include <vector>

/// Represents an information about a single function found in snapshot.
struct SnapshotFunction {
    /// Name of any global variable
    std::string GlobalVar;
    /// Path to the LLVM file containing the function
    std::string PathToLLVM;
    /// Name of the function.
    std::string Name;
    /// Tag associated with the function.
    std::string Tag;
};
LLVM_YAML_IS_SEQUENCE_VECTOR(SnapshotFunction);

/// Represents a source finder assigned to the snapshot.
struct SourceFinder {
    /// Which kind of source finder is it.
    std::string Kind;
};

/// Top-level representation of the snapshot.
struct Snapshot {
    /// Time of creation.
    std::string CreationTime;
    /// Version of the DiffKemp that created this snapshot.
    std::string Version;
    /// List of functions contained in the snapshot.
    std::vector<SnapshotFunction> Functions;
    /// Which kind of list is it.
    std::string ListKind;
    /// Version of the LLVM that provided LLVM IR to this snapshot.
    unsigned LLVMVersion;
    /// Source finder associated with this snapshot
    SourceFinder SrcFinder;
    /// Location of the source directory, all paths to the LLVM IR files in
    /// functions are relative to this directory.
    std::string SrcDir;
};
LLVM_YAML_IS_SEQUENCE_VECTOR(Snapshot);

namespace llvm {
namespace yaml {
/// Mapping of the source finder to and from a YAML.
template <> struct MappingTraits<SourceFinder> {
    static void mapping(IO &io, SourceFinder &srcFinder) {
        io.mapRequired("kind", srcFinder.Kind);
    }
};
} // namespace yaml
} // namespace llvm

namespace llvm {
namespace yaml {
/// Mapping of the snapshot function to and from a YAML.
template <> struct MappingTraits<SnapshotFunction> {
    static void mapping(IO &io, SnapshotFunction &snapFun) {
        io.mapRequired("glob_var", snapFun.GlobalVar);
        io.mapRequired("llvm", snapFun.PathToLLVM);
        io.mapRequired("name", snapFun.Name);
        io.mapRequired("tag", snapFun.Tag);
    }
};
} // namespace yaml
} // namespace llvm

namespace llvm {
namespace yaml {
/// Mapping of the snapshot to and from a YAML.
template <> struct MappingTraits<Snapshot> {
    static void mapping(IO &io, Snapshot &snapshot) {
        io.mapRequired("created_time", snapshot.CreationTime);
        io.mapRequired("diffkemp_version", snapshot.Version);
        io.mapRequired("list", snapshot.Functions);
        io.mapRequired("list_kind", snapshot.ListKind);
        io.mapRequired("llvm_source_finder", snapshot.SrcFinder);
        io.mapRequired("llvm_version", snapshot.LLVMVersion);
        io.mapRequired("source_dir", snapshot.SrcDir);
    }
};
} // namespace yaml
} // namespace llvm

#endif // DIFFKEMP_SIMPLL_SNAPSHOT_H
