#ifndef DIFFKEMP_SNAPSHOT_HPP
#define DIFFKEMP_SNAPSHOT_HPP

#include <iostream>
#include <vector>

#include <llvm/Support/YAMLTraits.h>

struct SnapshotFunction {
    std::string GlobalVar;
    std::string PathToLLVM;
    std::string Name;
    std::string Tag;
};
LLVM_YAML_IS_SEQUENCE_VECTOR(SnapshotFunction);

struct SourceFinder {
    std::string Kind;
};

struct Snapshot {
    std::string CreationTime;
    std::string Version;
    std::vector<SnapshotFunction> Functions;
    std::string ListKind;
    unsigned LLVMVersion;
    SourceFinder SrcFinder;
    std::string SrcDir;
};
LLVM_YAML_IS_SEQUENCE_VECTOR(Snapshot);

namespace llvm {
namespace yaml {
template <> struct MappingTraits<SourceFinder> {
    static void mapping(IO &io, SourceFinder &srcFinder) {
        io.mapRequired("kind", srcFinder.Kind);
    }
};
} // namespace yaml
} // namespace llvm

namespace llvm {
namespace yaml {
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

#endif // DIFFKEMP_SNAPSHOT_HPP
