"""
LLVM source finder for projects that are entirely compiled in build phase
of DiffKemp using the compiler wrapper.
"""
from diffkemp.llvm_ir import LlvmSourceFinder
from diffkemp.utils import get_functions_from_llvm
import os


class WrapperBuildFinder(LlvmSourceFinder):
    """
    LLVM source finder for projects that are entirely compiled in build phase
    of DiffKemp using the compiler wrapper.
    """
    def __init__(self, source_dir, wrapper_db_filename):
        LlvmSourceFinder.__init__(self, source_dir)
        self.wrapper_db_filename = wrapper_db_filename
        self.functions = {}
        # Prepare for snapshot generation
        self.initialize()

    def str(self):
        return "wrapper_build_tree"

    def clone_to_dir(self, new_source_dir):
        return WrapperBuildFinder(new_source_dir, self.wrapper_db_filename)

    def initialize(self):
        # Read LLVM IR file list from database generated by wrapper
        # Note: only o: files are added, f: files contain code that is already
        # present in the o: files
        llvm_files = []
        with open(self.wrapper_db_filename, 'r') as db_file:
            llvm_files = [line.split(':')[1].rstrip()
                          for line in db_file.readlines()
                          if line.startswith("o:")]

        self.functions = get_functions_from_llvm(llvm_files)

        # Write out functions list into a file in the snapshot directory
        with open(os.path.join(self.source_dir,
                               'function_list'), 'w') as function_list_file:
            for function_name in self.functions.keys():
                function_list_file.write(function_name + '\n')

    def finalize(self):
        pass

    def find_llvm_with_symbol_def(self, symbol):
        return self.functions.get(symbol)

    def find_llvm_with_symbol_use(self, symbol):
        return self.find_llvm_with_symbol_def(symbol)
