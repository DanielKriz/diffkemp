#!/usr/bin/env python3
from cffi import FFI
from subprocess import check_output
import os
import sys


def get_simpll_build_dir():
    """
    Return the current SimpLL build directory as specified
    in the `SIMPLL_BUILD_DIR` environment variable.
    """
    build_dir_var = "SIMPLL_BUILD_DIR"
    if build_dir_var in os.environ:
        return os.environ[build_dir_var]
    return "build"


def get_c_declarations(header_filename):
    """
    Extracts C declarations important for the cFFI module from the SimpLL FFI
    header file.
    :param filename Name of the SimpLL header file
    """
    cdef_start = "// CFFI_DECLARATIONS_START\n"
    cdef_end = "// CFFI_DECLARATIONS_END\n"

    with open(header_filename, "r") as header_file:
        lines = header_file.readlines()
        start = lines.index(cdef_start) + 1
        end = lines.index(cdef_end)
        return "".join(lines[start:end])


def parse_cmd_output(cmd_out):
    return list(filter(None, cmd_out.decode('ascii').strip().split()))


def get_root_dir(path, is_develop_build):
    # The build script is located in CMAKE_BINARY_DIR/diffkemp/simpll, hence
    # we have to move out of it
    if is_develop_build:
        return os.path.abspath(f'{path}/../../..')
    return os.path.abspath(f'{path}/../..')


ffibuilder = FFI()
location = os.path.dirname(os.path.abspath(__file__))
root_dir = get_root_dir(location, is_develop_build=(len(sys.argv) == 1))
path_to_ffi_header = "diffkemp/simpll/library/FFI.h"

ffibuilder.cdef(get_c_declarations(f"{root_dir}/{path_to_ffi_header}"))

llvm_components = ["irreader", "passes", "support"]

llvm_cflags = parse_cmd_output(check_output(["llvm-config", "--cflags"]))
llvm_ldflags = parse_cmd_output(check_output(["llvm-config", "--ldflags"]))
llvm_libs = parse_cmd_output(check_output(["llvm-config", "--libs",
                                           *llvm_components, "--system-libs"]))

simpll_link_arg = f"-L{get_simpll_build_dir()}/diffkemp/simpll"

include_path = f'{root_dir}/diffkemp/simpll'

ffibuilder.set_source(
    "_simpll", '#include <library/FFI.h>',
    language="c++",
    libraries=['simpll'],
    extra_compile_args=[f"-I{include_path}"] + llvm_cflags,
    extra_link_args=[simpll_link_arg, "-lstdc++"] + llvm_ldflags + llvm_libs)

if __name__ == "__main__":
    ffibuilder.compile()
