import jinja2
import os
import textwrap
from collections import OrderedDict

from thirdparty._internal.output import Output
from thirdparty._internal.util.files import save
from thirdparty.build import use_win_mingw

from thirdparty.cmake.presets import write_cmake_presets
from thirdparty.cmake.toolchain import RECIPE_TOOLCHAIN_FILENAME
from thirdparty.cmake.toolchain.blocks import (
    ExtraVariablesBlock,
    ToolchainBlocks,
    UserToolchain,
    GenericSystemBlock,
    AndroidSystemBlock,
    AppleSystemBlock,
    FPicBlock,
    ArchitectureBlock,
    GLibCXXBlock,
    VSRuntimeBlock,
    CppStdBlock,
    ParallelBlock,
    CMakeFlagsInitBlock,
    TryCompileBlock,
    FindFiles,
    PkgConfigBlock,
    SkipRPath,
    SharedLibBock,
    OutputDirsBlock,
    ExtraFlagsBlock,
    CompilersBlock,
    LinkerScriptsBlock,
    VSDebuggerEnvironment,
    VariablesBlock,
    PreprocessorBlock,
    RpathLinkFlagsBlock,
)
from thirdparty.cmake.utils import is_multi_configuration
from thirdparty.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.errors import RecipeException
from thirdparty.microsoft import VCVars
from thirdparty.microsoft.visual import vs_ide_version
from thirdparty.recipe import RecipeBase


class Variables(OrderedDict):
    _configuration_types = None  # Needed for py27 to avoid infinite recursion

    def __init__(self):
        super(Variables, self).__init__()
        self._configuration_types = {}

    def __getattribute__(self, config):
        try:
            return super(Variables, self).__getattribute__(config)
        except AttributeError:
            return self._configuration_types.setdefault(config, OrderedDict())

    @property
    def configuration_types(self):
        # Reverse index for the configuration_types variables
        ret = OrderedDict()
        for conf, definitions in self._configuration_types.items():
            for k, v in definitions.items():
                ret.setdefault(k, []).append((conf, v))
        return ret

    def quote_preprocessor_strings(self):
        for key, var in self.items():
            if isinstance(var, str):
                self[key] = str(var).replace('"', '\\"')
        for config, data in self._configuration_types.items():
            for key, var in data.items():
                if isinstance(var, str):
                    data[key] = str(var).replace('"', '\\"')


class CMakeToolchain:
    filename = RECIPE_TOOLCHAIN_FILENAME

    _template = textwrap.dedent(
        """
        # Recipe automatically generated toolchain file
        # DO NOT EDIT MANUALLY, it will be overwritten

        # Avoid including toolchain file several times (bad if appending to variables like
        #   CMAKE_CXX_FLAGS. See https://github.com/android/ndk/issues/323
        include_guard()
        message(STATUS "Using Recipe toolchain: ${CMAKE_CURRENT_LIST_FILE}")
        if(${CMAKE_VERSION} VERSION_LESS "3.15")
            message(FATAL_ERROR "The 'CMakeToolchain' generator only works with CMake >= 3.15")
        endif()

        {% for recipe_block in recipe_blocks %}
        {{ recipe_block }}
        {% endfor %}

        if(CMAKE_POLICY_DEFAULT_CMP0091)  # Avoid unused and not-initialized warnings
        endif()
        """)

    variables: Variables
    cache_variables: Variables

    _recipe: RecipeBase
    preprocessor_definitions: Variables
    extra_cxxflags: list[str]
    extra_cflags: list[str]
    extra_sharedlinkflags: list[str]
    extra_exelinkflags: list[str]
    add_rpath_link: bool
    find_builddirs: bool
    user_presets_path: str
    presets_prefix: str
    absolute_paths: bool
    configure_args: list[str]

    def __init__(self, recipe: RecipeBase, generator: str | None = None):
        self._recipe = recipe
        self.generator = "Ninja"
        self.variables = Variables()
        # This doesn't support multi-config, they go to the same configPreset common in multi-config
        self.cache_variables = Variables()
        self.preprocessor_definitions = Variables()

        self.extra_cxxflags = []
        self.extra_cflags = []
        self.extra_sharedlinkflags = []
        self.extra_exelinkflags = []
        self.add_rpath_link = False

        self.blocks = ToolchainBlocks(
            self._recipe, self, [
                ("user_toolchain", UserToolchain),
                ("generic_system", GenericSystemBlock),
                ("compilers", CompilersBlock),
                ("android_system", AndroidSystemBlock),
                ("apple_system", AppleSystemBlock),
                ("pic", FPicBlock),
                ("arch_flags", ArchitectureBlock),
                ("linker_scripts", LinkerScriptsBlock),
                ("rpath_link_flags", RpathLinkFlagsBlock),
                ("libcxx", GLibCXXBlock),
                ("vs_runtime", VSRuntimeBlock),
                ("vs_debugger_environment", VSDebuggerEnvironment),
                ("cppstd", CppStdBlock),
                ("parallel", ParallelBlock),
                ("extra_flags", ExtraFlagsBlock),
                ("cmake_flags_init", CMakeFlagsInitBlock),
                ("extra_variables", ExtraVariablesBlock),
                ("try_compile", TryCompileBlock),
                ("find_paths", FindFiles),
                ("pkg_config", PkgConfigBlock),
                ("rpath", SkipRPath),
                ("shared", SharedLibBock),
                ("output_dirs", OutputDirsBlock),
                ("variables", VariablesBlock),
                ("preprocessor", PreprocessorBlock),
            ])

        # Set the CMAKE_MODULE_PATH and CMAKE_PREFIX_PATH to the deps .builddirs
        self.find_builddirs = True
        self.user_presets_path = "CMakeUserPresets.json"
        self.presets_prefix = "recipe"
        self.presets_build_environment = None
        self.presets_run_environment = None
        self.absolute_paths = False  # By default use relative paths to toolchain and presets

    def _context(self):
        """ Returns dict, the context for the template
        """
        self.preprocessor_definitions.quote_preprocessor_strings()

        blocks = self.blocks.process_blocks()
        ctxt_toolchain = {

            "recipe_blocks": blocks,
        }

        return ctxt_toolchain

    @property
    def content(self):
        context = self._context()
        content = jinja2.Template(
            self._template, trim_blocks=True, lstrip_blocks=True, keep_trailing_newline=True).render(**context)
        return content

    @property
    def is_multi_configuration(self):
        return is_multi_configuration(self.generator)

    def _find_cmake_exe(self):
        for req in self._recipe.dependencies.direct_build.values():
            if req.name == "cmake":
                for bindir in req.info.bindirs:
                    cmake_path = os.path.join(bindir, "cmake")
                    cmake_exe_path = os.path.join(bindir, "cmake.exe")

                    if os.path.exists(cmake_path):
                        return cmake_path
                    elif os.path.exists(cmake_exe_path):
                        return cmake_exe_path

    def generate(self):
        """
          This method will save the generated files to the recipe.folders.generators
        """
        toolchain_file = self._recipe.conf.tools.cmake.toolchain.toolchain_file
        if toolchain_file is None:  # The main toolchain file generated only if user dont define
            toolchain_file = self.filename
            save(os.path.join(self._recipe.folders.generators, toolchain_file), self.content)
            Output(str(self._recipe)).info(f"CMakeToolchain generated: {toolchain_file}")
        # Generators like Ninja or NMake requires an active vcvars
        if self.generator is not None and "Visual" not in self.generator:
            VCVars(self._recipe).generate()

        cache_variables = {}
        for name, value in self.cache_variables.items():
            if isinstance(value, bool):
                cache_variables[name] = "ON" if value else "OFF"
            else:
                cache_variables[name] = value

        buildenv, runenv, cmake_executable = None, None, None

        if self._recipe.conf.tools.cmake.toolchain.presets_environment != "disabled":
            build_env = self.presets_build_environment.vars(self._recipe) if self.presets_build_environment else VirtualBuildEnv(self._recipe).vars()
            run_env = self.presets_run_environment.vars(self._recipe) if self.presets_run_environment else VirtualRunEnv(self._recipe).vars()

            buildenv = {name: value for name, value in build_env.items(variable_reference="$penv{{{name}}}")}
            runenv = {name: value for name, value in run_env.items(variable_reference="$penv{{{name}}}")}

            cmake_executable = self._recipe.conf.tools.cmake.cmake_program
            cmake_executable = cmake_executable or self._find_cmake_exe()

        write_cmake_presets(self._recipe, toolchain_file, self.generator, cache_variables, self.user_presets_path, self.presets_prefix, buildenv, runenv, cmake_executable, self.absolute_paths)
