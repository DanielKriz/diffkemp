"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from llvmcpy.llvm import *
from diffkemp.function_comparator import compare_functions, Result
from diffkemp.function_coupling import FunctionCouplings


class Statistics():
    """
    Statistics of the analysis.
    Captures numbers of equal and not equal functions, as well as number of
    uncertain or error results.
    """
    def __init__(self):
        self.equal = list()
        self.not_equal = list()
        self.unknown = list()
        self.errors = list()

    def log_result(self, result, function):
        """Add result of analysis (comparison) of some function."""
        if result == Result.EQUAL or result == Result.EQUAL_UNDER_ASSUMPTIONS:
            self.equal.append(function)
        elif result == Result.NOT_EQUAL:
            self.not_equal.append(function)
        elif result == Result.UNKNOWN:
            self.unknown.append(function)
        else:
            self.errors.append(function)

    def report(self):
        """Report results."""
        print "Equal:     ", str(len(self.equal))
        print "Not equal: ", str(len(self.not_equal))
        print "Unknown:   ", str(len(self.unknown))
        print "Errors:    ", str(len(self.errors))

    def overall_result(self):
        """Aggregate results for individual functions into one result."""
        if len(self.errors) > 0:
            return Result.ERROR
        if len(self.not_equal) > 0:
            return Result.NOT_EQUAL
        if len(self.unknown) > 0:
            return Result.UNKNOWN
        return Result.EQUAL


def compare_modules(first, second, parameter, verbose=False):
    """
    Analyse semantic difference of two LLVM IR modules w.r.t. some parameter
    :param first: File with LLVM IR of the first module
    :param second: File with LLVM IR of the second module
    :param parameter: Parameter (global variable) to compare (only functions
                      using this variable are compared)
    """
    stat = Statistics()
    couplings = FunctionCouplings(first, second)
    couplings.infer_for_param(parameter)
    for c in couplings.main:
        result = compare_functions(first, second, c.first, c.second,
                                   couplings.called, verbose)
        stat.log_result(result, c.first)
    return stat

