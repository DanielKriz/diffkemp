"""
Building kernel module into LLVM IR.
Functions for downloading, configuring, compiling, and linkning kernel modules
into LLVM IR.
"""

import os
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from diffkemp.llvm_ir.module_analyser import *
from distutils.version import StrictVersion
from progressbar import ProgressBar, Percentage, Bar
from subprocess import CalledProcessError, call, check_call, check_output
from subprocess import Popen, PIPE
from urllib import urlretrieve


class BuildException(Exception):
    pass


# Progress bar for downloading
pbar = None


def show_progress(count, block_size, total_size):
    global pbar
    if pbar is None:
        pbar = ProgressBar(maxval=total_size, widgets=[Percentage(), Bar()])
        pbar.start()

    downloaded = count * block_size
    if downloaded < total_size:
        pbar.update(downloaded)
    else:
        pbar.finish()
        pbar = None


class LlvmKernelBuilder:
    """
    Building kernel modules into LLVM IR.
    Contains methods to automatically:
        - download kernel sources
        - configure kernel
        - build modules using GCC
        - build modules using Clang into LLVM IR
        - collect module parameters with default values
    """

    # Base path to kernel sources
    kernel_base_path = "kernel"

    def __init__(self, kernel_version, modules_dir, debug=False):
        if not os.path.isdir(self.kernel_base_path):
            os.mkdir(self.kernel_base_path)
        self.kernel_base_path = os.path.abspath(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-{}".format(self.kernel_version))
        self.modules_dir = modules_dir
        self.debug = debug

        self._prepare_kernel()

    def _extract_tar(self, tarname):
        """Extract kernel sources from .tar.xz file."""
        cwd = os.getcwd()
        os.chdir(self.kernel_base_path)
        print "Extracting"
        check_call(["tar", "-xJf", tarname])
        os.remove(tarname)
        dirname = tarname[:-7]
        # If the produced directory does not have the expected name, rename it.
        if dirname != self.kernel_path:
            os.rename(dirname, self.kernel_path)
        print "Done"
        print("Kernel sources for version {} are in directory {}".format(
              self.kernel_version, self.kernel_path))
        os.chdir(cwd)

    def _get_kernel_tar_from_upstream(self):
        """
        Download sources of the required kernel version from the upstream
        (www.kernel.org). Sources are stored as .tar.xz file.
        :returns Name of the tar file containing the sources.
        """
        url = "https://www.kernel.org/pub/linux/kernel/"

        # Version directory (different naming style for versions under and
        # over 3.0)
        if StrictVersion(self.kernel_version) < StrictVersion("3.0"):
            url += "v{}/".format(self.kernel_version[:3])
        else:
            url += "v{}.x/".format(self.kernel_version[:1])

        tarname = "linux-{}.tar.xz".format(self.kernel_version)
        url += tarname

        # Download the tarball with kernel sources
        print "Downloading kernel version {}".format(self.kernel_version)
        urlretrieve(url, os.path.join(self.kernel_base_path, tarname),
                    show_progress)

        return tarname

    def _get_kernel_tar_from_brew(self):
        """
        Download sources of the required kernel from Brew.
        Sources are part of the SRPM package and need to be extracted out of
        it.
        :returns Name of the tar file containing the sources.
        """
        url = "http://download.eng.bos.redhat.com/brewroot/packages/kernel/"
        version, release = self.kernel_version.split("-")
        url += "{}/{}.el7/src/".format(version, release)
        rpmname = "kernel-{}.el7.src.rpm".format(self.kernel_version)
        url += rpmname
        # Download the source RPM package
        print "Downloading kernel version {}".format(self.kernel_version)
        urlretrieve(url, os.path.join(self.kernel_base_path, rpmname),
                    show_progress)

        cwd = os.getcwd()
        os.chdir(self.kernel_base_path)
        # Extract files from SRPM package
        with open(os.devnull, "w") as devnull:
            rpm_cpio = Popen(["rpm2cpio", rpmname], stdout=PIPE,
                             stderr=devnull)
            check_call(["cpio", "-idmv"], stdin=rpm_cpio.stdout,
                       stderr=devnull)

        # Delete all files but the tar.xz file (keep the directories since
        # these are other kernels - RPM does not contain dirs)
        tarname = "linux-{}.el7.tar.xz".format(self.kernel_version)
        for f in os.listdir("."):
            if os.path.isfile(f) and f != tarname:
                os.remove(f)
        os.chdir(cwd)
        return tarname

    def _get_kernel_source(self):
        """Download the sources of the required kernel version."""
        # Download sources from upstream (kernel.org) or Brew
        # The choice is done based on version string, if it has release part
        # (e.g. 3.10.0-655) it must be downloaded from Brew (StrictVersion will
        # raise exception on such version string).
        try:
            StrictVersion(self.kernel_version)
            tarname = self._get_kernel_tar_from_upstream()
        except ValueError:
            tarname = self._get_kernel_tar_from_brew()

        self._extract_tar(tarname)

    def _call_and_print(self, command, stdout=None, stderr=None):
        print "  {}".format(" ".join(command))
        check_call(command, stdout=stdout, stderr=stderr)

    def _call_output_and_print(self, command):
        with open(os.devnull) as stderr:
            print "    {}".format(" ".join(command))
            output = check_output(command, stderr=stderr)
            return output

    def _check_make_target(self, make_command):
        """
        Check if make target exists.
        Runs make with -n argument which returns 2 if the target does not
        exist.
        :param make_command: Make command to run
        :return True if the command can be run (target exists)
        """
        with open(os.devnull, "w") as devnull:
            ret_code = call(make_command + ["-n"],
                            stdout=devnull, stderr=devnull)
            return ret_code != 2

    def _configure_kernel(self):
        """
        Configure kernel.
        When possible, everything should be compiled as module, otherwise it
        should be compiled as built-in.
        This can be achieved by commands:
            make allmodconfig
            make prepare
            make modules_prepare
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        print "Configuring and preparing modules"
        with open(os.devnull, 'w') as null:
            self._call_and_print(["make", "allmodconfig"], null, null)
            self._disable_kabi_size_align_checks()
            self._call_and_print(["make", "prepare"], null, null)
            self._call_and_print(["make", "modules_prepare"], null, null)
        os.chdir(cwd)

    def _prepare_kernel(self):
        """
        Download and configure kernel if kernel directory does not exist.
        """
        print "Kernel version {}".format(self.kernel_version)
        print "-------------------"
        if not os.path.isdir(self.kernel_path):
            self._get_kernel_source()
            self._symlink_gcc_header(7)
            self._configure_kernel()

    def _symlink_gcc_header(self, major_version):
        """
        Symlink include/linux/compiler-gccX.h for the current GCC version with
        the most recent header in the downloaded kernel
        :param major_version: Major version of GCC to be used for compilation
        """
        include_path = os.path.join(self.kernel_path, "include/linux")
        dest_file = os.path.join(include_path,
                                 "compiler-gcc{}.h".format(major_version))
        if not os.path.isfile(dest_file):
            # Search for the most recent version of header provided in the
            # analysed kernel and symlink the current version to it
            regex = re.compile(r"^compiler-gcc(\d+)\.h$")
            max_major = 0
            for file in os.listdir(include_path):
                match = regex.match(file)
                if match and int(match.group(1)) > max_major:
                    max_major = int(match.group(1))

            if max_major > 0:
                src_file = os.path.join(include_path,
                                        "compiler-gcc{}.h".format(max_major))
                os.symlink(src_file, dest_file)

    def _disable_kabi_size_align_checks(self):
        """
        Set CONFIG_RH_KABI_SIZE_ALIGN_CHECKS=n in .config file. Compiling with
        this option set to 'y' causes compilation failure.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        os.rename(".config", ".oldconfig")
        with open(".oldconfig", "r") as oldconfig:
            with open(".config", "w") as config:
                for line in oldconfig:
                    if "CONFIG_RH_KABI_SIZE_ALIGN_CHECKS" in line:
                        config.write("CONFIG_RH_KABI_SIZE_ALIGN_CHECKS=n\n")
                    else:
                        config.write(line)
        os.chdir(cwd)

    def _clean_kernel(self):
        """Clean whole kernel"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            self._call_and_print(["make", "clean"], stdout=stdout)
        os.chdir(cwd)

    def _clean_object(self, obj):
        """Clean an object file"""
        if os.path.isfile(obj):
            os.remove(obj)

    def _clean_module(self, mod):
        """Clean an object file"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            check_call(["make", "M={}".format(self.modules_dir),
                        "{}.ko".format(mod), "clean"],
                       stdout=stdout)
        os.chdir(cwd)

    def _clean_all_modules(self):
        """Clean all modules in the modules directory"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            check_call(["make", "M={}".format(self.modules_dir), "clean"],
                       stdout=stdout)
        os.chdir(cwd)

    def _get_sources_with_params(self, directory):
        """
        Get list of .c files in modules directory and all its subdirectories
        that contain definitions of module parameters (contain call to
        module_param macro).
        """
        result = list()
        for f in os.listdir(directory):
            file = os.path.join(directory, f)
            if os.path.isfile(file) and file.endswith(".c"):
                for line in open(file, "r"):
                    if "module_param" in line:
                        result.append(file)
                        break
            elif os.path.isdir(file):
                dir_files = self._get_sources_with_params(file)
                result.extend(dir_files)

        return result

    def _strip_bash_quotes(self, gcc_param):
        """
        Remove quotes from gcc_param that represents a part of a shell command.
        """
        if "\'" in gcc_param:
            return gcc_param.translate(None, "\'")
        else:
            return gcc_param.translate(None, "\"")

    def kbuild_object_command(self, object_file):
        """
        Build the object file (.o) using KBuild.
        The command used is `make V=1 /path/to/object.o`
        :returns GCC command used for the compilation. This is the last
                 command starting with 'gcc' that was run by make
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)

        object_file = os.path.join(self.modules_dir, object_file)
        self._clean_object(object_file)
        with open(os.devnull, "w") as stderr:
            try:
                output = check_output(["make", "V=1", object_file,
                                       "--just-print"], stderr=stderr)
            except CalledProcessError:
                raise BuildException("Error compiling {}".format(object_file))
        os.chdir(cwd)

        commands = output.splitlines()
        for cs in reversed(commands):
            for c in cs.split(";"):
                if c.lstrip().startswith("gcc"):
                    return c.lstrip()
        raise BuildException("Compiling {} did not run a gcc command".format(
                             object_file))

    def kbuild_module(self, module):
        """
        Build a kernel module using Kbuild.
        The command used is `make V=1 M=/path/to/mod module.ko`
        First, tries to build module with the given name, next tries to replace
        '_' by '-'.
        :returns Name used in the .ko file (possibly with underscores replaced
                 by dashes).
                 List of commands that were used to comiple and link files in
                 the module.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        file_name = module
        command = ["make", "V=1", "M={}".format(self.modules_dir),
                   "{}.ko".format(file_name)]

        if not self._check_make_target(command):
            # If the target does not exist, replace "_" by "-" and try again
            file_name = file_name.replace("_", "-")
            command[3] = "{}.ko".format(file_name)
        if not self._check_make_target(command):
            os.chdir(cwd)
            raise BuildException("Could not build module {}".format(module))

        try:
            output = self._call_output_and_print(command)
            ko_file = os.path.join(self.modules_dir, "{}.ko".format(file_name))
            if not os.path.exists(ko_file):
                raise BuildException("Could not build module {}"
                                     .format(module))
        except CalledProcessError:
            raise BuildException("Could not build module {}".format(module))
        finally:
            os.chdir(cwd)
        return file_name, output.splitlines()

    def get_module_name(self, gcc_command):
        """
        Extracts name of the module from a gcc command used to compile (part
        of) the module.
        The name is usually given by setting KBUILD_MODNAME variable using one
        of two ways:
            -D"KBUILD_MODNAME=KBUILD_STR(module)"
            -DKBUILD_MODNAME='"module"'
        """
        regexes = [
            re.compile(r"^-DKBUILD_MODNAME=KBUILD_STR\((.*)\)$"),
            re.compile(r"^-DKBUILD_MODNAME=\"(.*)\"$")
        ]
        for param in gcc_command.split():
            if "KBUILD_MODNAME" in param:
                param = self._strip_bash_quotes(param)
                for r in regexes:
                    m = r.match(param)
                    if m:
                        return m.group(1)
        raise BuildException("Unable to find module name")

    def get_output_file(self, command):
        """
        Extract name of the output file produced by a command.
        The name is the parameter following -o option.
        :param command: GCC, Clang, or llvm-link command. It is expected to be
                        a list of strings (parameters of the command).
        """
        index = command.index("-o")
        if len(command) == index:
            raise BuildException("Broken command: {}".format("".join(command)))
        return command[index + 1]

    def gcc_to_llvm(self, gcc_command):
        """
        Convert GCC command to corresponding Clang command for compiling source
        into LLVM IR.
        :param gcc_command: Command to convert
        """
        command = ["clang", "-S", "-emit-llvm", "-O1", "-Xclang",
                   "-disable-llvm-passes"]
        if self.debug:
            command.append("-g")
        for param in gcc_command.split():
            if (param == "gcc" or
                    (param.startswith("-W") and "-MD" not in param) or
                    param.startswith("-f") or
                    param.startswith("-m") or
                    param.startswith("-O") or
                    param == "-DCC_HAVE_ASM_GOTO" or
                    param == "-g" or
                    param == "-o" or
                    param.endswith(".o")):
                continue

            # Output name is given by replacing .c by .bc in source name
            if param.endswith(".c"):
                output_file = "{}.bc".format(param[:-2])

            command.append(self._strip_bash_quotes(param))
        command.extend(["-o", output_file])
        return command

    def ld_to_llvm(self, ld_command):
        """
        Convert ld command into llvm-link command to link multiple LLVM IR
        files into one file.
        :param ld_command: Command to convert
        """
        command = ["llvm-link", "-S"]
        for param in ld_command.split():
            if param.endswith(".o"):
                command.append("{}.bc".format(param[:-2]))
            elif param == "-o":
                command.append(param)
        return command

    def opt_llvm(self, llvm_file, command):
        """
        Optimise LLVM IR using 'opt' tool. LLVM passes are chosen based on the
        command that created the file being optimized.
        For compiled files (using clang), run basic simplification passes.
        For linked files (using llvm-link), run -constmerge to remove
        duplicate constants that might have come from linked files.
        """
        opt_command = ["opt", "-S", llvm_file, "-o", llvm_file]
        if command == "clang":
            opt_command.extend(["-lowerswitch", "-mem2reg", "-loop-simplify",
                                "-simplifycfg"])
        elif command == "llvm-link":
            opt_command.append("-constmerge")
        else:
            raise BuildException("Invalid call to {}".format(command))
        try:
            check_call(opt_command)
        except CalledProcessError:
            raise BuildException("Running opt failed")

    def kbuild_to_llvm_commands(self, kbuild_commands, file_name):
        """
        Convers a list of Kbuild commands for building a module into a list of
        corresponding llvm/clang commands to build the module into LLVM IR.
        GCC commands are transformed into clang commands.
        LD commands are transformed into llvm-link commands.
        Unnecessary commands are filtered out.
        :param kbuild_commands: List of Kbuild commands to transform
        :param file_name: File name of the kernel module to be built
        :return List of llvm/clang commands.
        """
        clang_commands = list()
        for c in kbuild_commands:
            command = c.lstrip()
            if (command.startswith("gcc") and
                    "{}.mod".format(file_name) not in command):
                clang_commands.append(self.gcc_to_llvm(command))
            elif (command.startswith("ld") and
                  "{}.ko".format(file_name) not in command):
                clang_commands.append(
                    self.ld_to_llvm(command.split(";")[0]))
        return clang_commands

    def build_llvm_file(self, file, command):
        """
        Build single source into LLVM IR
        :param file: Name of the result file
        :param command: Command to be executed
        """
        print "    [{}] {}".format(command[0], file)
        with open(os.devnull, "w") as stderr:
            try:
                check_call(command, stderr=stderr)
            except CalledProcessError:
                raise BuildException("Building {} failed".format(file))
        self.opt_llvm(file, command[0])

    def build_llvm_module(self, name, file_name, commands):
        """
        Build kernel module into LLVM IR.
        :param name: Module name
        :param file_name: Name of the kernel object file without extension (can
                          be different from the module name).
        :param commands: List of clang/llvm-link commands to be executed
        :returns Instance of LlvmKernelModule with information about files
                 containing the compiled module
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        for command in commands:
            file = self.get_output_file(command)
            self.build_llvm_file(file, command)

        if not os.path.isfile(os.path.join(self.modules_dir, "{}.bc".format(
                                                             file_name))):
            raise BuildException("Building {} did not produce LLVM IR file"
                                 .format(name))
        os.chdir(cwd)
        mod = LlvmKernelModule(name, file_name, os.path.join(self.kernel_path,
                                                             self.modules_dir))
        return mod

    def build_file(self, file_name):
        """
        Build single object file.
        First use kbuild to get the gcc command for building the file, then
        convert it into corresponding clang command and run it.
        :param file_name: Name of the file (without extension)
        :returns Instance of LlvmKernelModule where the compiled file is the
                 main module file and no kernel object file is provided.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        command = self.kbuild_object_command("{}.o".format(file_name))
        command = self.gcc_to_llvm(command)
        self.build_llvm_file(os.path.join(self.modules_dir,
                                          "{}.bc".format(file_name)),
                             command)
        os.chdir(cwd)
        return LlvmKernelModule(file_name, file_name,
                                os.path.join(self.kernel_path,
                                             self.modules_dir))

    def build_module(self, module, clean):
        """
        Build kernel module.
        First use kbuild to build kernel object file. Then, transform kbuild
        commands to the corresponding clang commands and build LLVM IR of the
        module.
        :param module: Name of the module
        :param clean: Clean module before building
        :return Built module in LLVM IR (instance of LlvmKernelModule)
        """
        if clean:
            self._clean_all_modules()
        file_name, commands = self.kbuild_module(module)
        clang_commands = self.kbuild_to_llvm_commands(commands, file_name)
        return self.build_llvm_module(module, file_name, clang_commands)

    def build_modules_with_params(self, clean):
        """
        Build all modules in the modules directory that can be configured via
        parameters.
        :return Dictionary of modules in form name -> module
        """
        print "Building all kernel modules having parameters"
        print "  Collecting modules"
        if clean:
            self._clean_all_modules()
        sources = self._get_sources_with_params(os.path.join(self.kernel_path,
                                                             self.modules_dir))
        modules = set()
        # First build objects from sources that contain definitions of
        # parameters. By building them, we can obtain names of modules they
        # belong to.
        for src in sources:
            obj = src[:-1] + "o"
            command = self.kbuild_object_command(obj)
            mod = self.get_module_name(command)
            mod_dir = os.path.relpath(os.path.dirname(src), self.kernel_path)
            if mod_dir != self.modules_dir:
                mod_dir = os.path.relpath(mod_dir, self.modules_dir)
                mod = os.path.join(mod_dir, mod)
            # Put mod into modules
            modules.add(mod)

        # Build collected modules with Kbuild and collect commands
        # Then, transform commands to use Clang/LLVM and run them to build LLVM
        # IR of modules
        llvm_modules = dict()
        for mod in modules:
            print "  {}".format(mod)
            try:
                llvm_modules[mod] = self.build_module(mod, False)
            except BuildException as e:
                print "    {}".format(str(e))
        print ""
        return llvm_modules
