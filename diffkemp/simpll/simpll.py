"""
Simplifying LLVM modules with the SimpLL tool.
"""
from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.simpll._simpll import ffi, lib
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
import os
from subprocess import check_call, check_output, CalledProcessError
import yaml


class SimpLLModule:
    """Represents a Module class in LLVM."""
    def __init__(self, path):
        self.pointer = lib.loadModule(ffi.new("char []", path.encode("ascii")))

    def __del__(self):
        lib.freeModule(self.pointer)


class SimpLLException(Exception):
    pass


def add_suffix(file, suffix):
    """Add suffix to the file name."""
    name, ext = os.path.splitext(file)
    return "{}-{}{}".format(name, suffix, ext)


def run_simpll(first, second, fun_first, fun_second, var, suffix=None,
               cache_dir=None, control_flow_only=False, output_llvm_ir=False,
               print_asm_diffs=False, verbose=False, use_ffi=False):
    """
    Simplify modules to ease their semantic difference. Uses the SimpLL tool.
    :return A tuple containing the two LLVM IR files generated by SimpLL
            followed by the result of the comparison in the form of a graph and
            a list of missing function definitions.
    """
    stderr = None
    if not verbose:
        stderr = open(os.devnull, "w")

    first_out_name = add_suffix(first, suffix) if suffix else first
    second_out_name = add_suffix(second, suffix) if suffix else second

    if use_ffi:
        output = ffi.new("char [1000000]")
        conf_struct = ffi.new("struct config *")

        cache_dir = ffi.new("char []", cache_dir.encode("ascii") if cache_dir
                            else b"")
        variable = ffi.new("char []", var.encode("ascii") if var else b"")
        conf_struct = ffi.new("struct config *")
        conf_struct.CacheDir = cache_dir
        conf_struct.ControlFlowOnly = control_flow_only
        conf_struct.OutputLlvmIR = output_llvm_ir
        conf_struct.PrintAsmDiffs = print_asm_diffs
        conf_struct.PrintCallStacks = True
        conf_struct.Variable = variable
        conf_struct.Verbose = verbose
        conf_struct.VerboseMacros = False

        module_left = ffi.new("char []", first.encode("ascii"))
        module_right = ffi.new("char []", second.encode("ascii"))
        module_left_out = ffi.new("char []", first_out_name.encode("ascii"))
        module_right_out = ffi.new("char []", second_out_name.encode("ascii"))
        fun_left = ffi.new("char []", fun_first.encode("ascii"))
        fun_right = ffi.new("char []", fun_second.encode("ascii"))

        try:
            lib.parseAndRunSimpLL(module_left, module_right,
                                  module_left_out, module_right_out, fun_left,
                                  fun_right, conf_struct[0], output)
            simpll_out = ffi.string(output)
            lib.shutdownSimpLL()
        except ffi.error:
            raise SimpLLException("Simplifying files failed")
    else:
        try:
            # Determine the SimpLL binary to use.
            # The manually built one has priority over the installed one.
            if os.path.isfile("build/diffkemp/simpll/diffkemp-simpll"):
                simpll_bin = "build/diffkemp/simpll/diffkemp-simpll"
            else:
                simpll_bin = "diffkemp-simpll"
            # SimpLL command
            simpll_command = list([simpll_bin, first, second,
                                   "--print-callstacks"])
            # Main (analysed) functions
            simpll_command.append("--fun")
            if fun_first != fun_second:
                simpll_command.append("{},{}".format(fun_first, fun_second))
            else:
                simpll_command.append(fun_first)
            # Analysed variable
            if var:
                simpll_command.extend(["--var", var])
            # Suffix for output files
            if suffix and output_llvm_ir:
                simpll_command.extend(["--suffix", suffix])
            # Cache directory with equal function pairs
            if cache_dir:
                simpll_command.extend(["--cache-dir", cache_dir])

            if control_flow_only:
                simpll_command.append("--control-flow")

            if output_llvm_ir:
                simpll_command.append("--output-llvm-ir")

            if print_asm_diffs:
                simpll_command.append("--print-asm-diffs")

            if verbose:
                simpll_command.append("--verbose")
                print(" ".join(simpll_command))

            simpll_out = check_output(simpll_command)
        except CalledProcessError:
            raise SimpLLException("Simplifying files failed")

    if output_llvm_ir:
        check_call(["opt", "-S", "-deadargelim", "-o", first_out_name,
                    first_out_name],
                   stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", second_out_name,
                    second_out_name],
                   stderr=stderr)

    first_out = LlvmKernelModule(first_out_name)
    second_out = LlvmKernelModule(second_out_name)

    missing_defs = None
    try:
        result_graph = ComparisonGraph()
        simpll_result = yaml.safe_load(simpll_out)
        if simpll_result is not None:
            if "function-results" in simpll_result:
                for fun_result in simpll_result["function-results"]:
                    # Create the vertex from the result and insert it into
                    # the graph.
                    vertex = ComparisonGraph.Vertex.from_yaml(
                        fun_result, result_graph)
                    # Prefer pointed name to ensure that a difference
                    # contaning the variant function as either the left or
                    # the right side has its name in the key.
                    # This is useful because one can tell this is a weak
                    # vertex from its name.
                    if "." in vertex.names[ComparisonGraph.Side.LEFT]:
                        result_graph[vertex.names[
                            ComparisonGraph.Side.LEFT]] = vertex
                    else:
                        result_graph[vertex.names[
                            ComparisonGraph.Side.RIGHT]] = vertex
            result_graph.normalize()
            result_graph.populate_predecessor_lists()
            result_graph.mark_uncachable_from_assumed_equal()
            missing_defs = simpll_result["missing-defs"] \
                if "missing-defs" in simpll_result else None
    except yaml.YAMLError:
        pass

    return first_out, second_out, result_graph, missing_defs
