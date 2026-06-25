import os
from typing import Any

from thirdparty._internal.internal_tools import is_universal_arch
from thirdparty.apple.utils import is_apple_os, resolve_apple_flags, apple_extra_flags
from thirdparty.build import cmd_args_to_string, save_toolchain_args
from thirdparty.build.cross_building import cross_building
from thirdparty.build.flags import architecture_flag, architecture_link_flag, build_type_flags, cppstd_flag, build_type_link_flags, libcxx_flags, cstd_flag, llvm_clang_front, threads_flags
from thirdparty.env import Environment, VirtualBuildEnv
from thirdparty.errors import RecipeException
from thirdparty.autotools.get_gnu_triplet import _get_gnu_triplet
from thirdparty.microsoft import VCVars, msvc_runtime_flag, unix_path, check_min_vs, is_msvc
from thirdparty.recipe import RecipeBase


class AutotoolsToolchain:

    def __init__(self, recipe: RecipeBase, namespace: str | None = None, prefix: str = "/"):
        """
        :param recipe: The current recipe object. Always use ``self``.
        :param namespace: This argument avoids collisions when you have multiple toolchain calls in
               the same recipe. By setting this argument, the *buildenv.conf* file used to pass
               information to the build helper will be named as *<namespace>_buildenv.conf*. The default
               value is ``None`` meaning that the name of the generated file is *buildenv.conf*. This
               namespace must be also set with the same value in the constructor of the Autotools build
               helper so that it reads the information from the proper file.
        :param prefix: Folder to use for ``--prefix`` argument ("/" by default).
        """

        self._recipe = recipe
        self._namespace = namespace
        self._prefix = prefix

        # Flags
        self.extra_cxxflags = []
        self.extra_cflags = []
        self.extra_ldflags = []
        self.extra_defines = []

        # Defines
        self.ndebug = None
        build_type = self._recipe.settings.get_safe("build_type")
        if build_type in ['Release', 'RelWithDebInfo', 'MinSizeRel']:
            self.ndebug = "NDEBUG"

        # TODO: This is also covering compilers like Visual Studio, necessary to test it (&remove?)
        self.build_type_flags = build_type_flags(self._recipe)
        self.build_type_link_flags = build_type_link_flags(self._recipe.settings)

        self.cppstd = cppstd_flag(self._recipe)
        self.cstd = cstd_flag(self._recipe)
        self.arch_flag = architecture_flag(self._recipe)
        self.arch_ld_flag = architecture_link_flag(self._recipe)
        self.threads_flags = threads_flags(self._recipe)
        self.libcxx, self.gcc_cxx11_abi = libcxx_flags(self._recipe)
        self.fpic = self._recipe.options.get_safe("fPIC")
        self.msvc_runtime_flag = self._get_msvc_runtime_flag()
        self.msvc_extra_flags = self._msvc_extra_flags()
        self.msvc_runtime_link_flags = []
        if llvm_clang_front(self._recipe) == "clang":
            self.msvc_runtime_link_flags = ["-fuse-ld=lld-link"]

        self._is_universal_arch = is_universal_arch(
            recipe.settings.get_safe("arch"), recipe.settings.possible_values().get("arch"))
        if self._is_universal_arch and not is_apple_os(self._recipe):
            arch_str = recipe.settings.get_safe('arch')
            raise RecipeException(f"Universal arch '{arch_str}' is only supported in Apple OSes")

        # Cross build triplets
        self._host = self._recipe.conf.get("tools.gnu:host_triplet")
        self._build = self._recipe.conf.get("tools.gnu:build_triplet")
        self._target = None

        self.android_cross_flags = {}
        self._is_cross_building = not self._is_universal_arch and cross_building(self._recipe)
        if self._is_cross_building:
            compiler = self._recipe.settings.get_safe("compiler")
            # If cross-building and tools.android:ndk_path is defined, let's try to guess the Android
            # cross-building flags
            self.android_cross_flags = self._resolve_android_cross_compilation()
            # If it's not defined the triplet
            if not self._host:
                os_host = recipe.settings.get_safe("os")
                arch_host = recipe.settings.get_safe("arch")
                self._host = _get_gnu_triplet(os_host, arch_host, compiler=compiler)["triplet"]
            # Build triplet
            if not self._build:
                os_build = recipe.settings_build.get_safe('os')
                arch_build = recipe.settings_build.get_safe('arch')
                self._build = _get_gnu_triplet(os_build, arch_build, compiler=compiler)["triplet"]

        sysroot = self._recipe.conf.get("tools.build:sysroot")
        if sysroot:
            root = sysroot.replace("\\", "/")
            compiler = self._recipe.settings.get_safe("compiler")
            self.sysroot_flag = f"--sysroot {root}" if compiler != "qcc" else f"-Wc,-isysroot,{root}"
        else:
            self.sysroot_flag = None

        extra_configure_args = self._recipe.conf.get(
            "tools.gnu:extra_configure_args", check_type=list, default=[])

        self.configure_args = (self._default_configure_shared_flags() + self._default_configure_install_flags() + self._get_triplets() + extra_configure_args)
        self.autoreconf_args = self._default_autoreconf_flags()
        self.make_args = []
        # Apple stuff
        is_cross_building_osx = (self._is_cross_building and recipe.settings_build.get_safe('os') == "Mac" and is_apple_os(recipe) and not self._is_universal_arch)

        min_flag, arch_flags, isysroot_flag = (resolve_apple_flags(
            recipe, is_cross_building=is_cross_building_osx, is_universal=self._is_universal_arch))
        # https://man.archlinux.org/man/clang.1.en#Target_Selection_Options
        self.apple_arch_flag = arch_flags
        # -isysroot makes all includes for your library relative to the build directory
        self.apple_isysroot_flag = isysroot_flag
        self.apple_min_version_flag = min_flag
        self.apple_extra_flags = apple_extra_flags(self._recipe)

    def yes_no(self, option_name: str, default: Any = None, negated: bool = False) -> str:
        """
        Simple wrapper to return "yes" or "no" depending on whether ``option_name`` evaluates
        as True or False.  Convenient for autotools ``--enable-x=yes/no`` configure arguments.

        :param option_name: option name.
        :param default: Default value to return if the option is not defined.
        :param negated: Negates the option value if True.
        :return: "yes" or "no" depending on whether option_name is True or False.
        """
        option_value = bool(self._recipe.options.get_safe(option_name, default=default))
        option_value = not option_value if negated else option_value
        return "yes" if option_value else "no"

    def _resolve_android_cross_compilation(self) -> dict[str, str]:
        # Issue related: upstream issue 13443
        ret = {}
        if not self._is_cross_building or not self._recipe.settings.get_safe("os") == "Android":
            return ret
        # Setting host if it was not already defined yet
        arch = self._recipe.settings.get_safe("arch")
        android_target = {
            'ARM': 'aarch64-linux-android', 'X64': 'x86_64-linux-android',
        }.get(arch)
        self._host = self._host or android_target
        # Automatic guessing made by Recipe (need the NDK path variable defined)
        recipe_vars = {}
        ndk_path = self._recipe.conf.get("tools.android:ndk_path", check_type=str)
        if ndk_path:
            if self._recipe.conf.get("tools.build:compiler_executables"):
                self._recipe.output.warning(
                    "tools.build:compiler_executables conf has no effect"
                    " when tools.android:ndk_path is defined too.")
            os_build = self._recipe.settings_build.get_safe("os")
            ndk_os_folder = {
                'Mac': 'darwin', 'iOS': 'darwin', 'tvOS': 'darwin', 'visionOS': 'darwin', 'Linux': 'linux', 'Windows': 'windows', 'WindowsCE': 'windows', 'WindowsStore': 'windows',
            }.get(os_build, "linux")
            ext = ".cmd" if os_build == "Windows" else ""
            ndk_bin = os.path.join(
                ndk_path, "toolchains", "llvm", "prebuilt", f"{ndk_os_folder}-x86_64", "bin")
            android_api_level = self._recipe.settings.get_safe("os.api_level")
            recipe_vars = {
                "CC": os.path.join(ndk_bin, f"{android_target}{android_api_level}-clang{ext}"),
                "CXX": os.path.join(ndk_bin, f"{android_target}{android_api_level}-clang++{ext}"),
                "LD": os.path.join(ndk_bin, "ld"),
                "STRIP": os.path.join(ndk_bin, "llvm-strip"),
                "RANLIB": os.path.join(ndk_bin, "llvm-ranlib"),
                "AS": os.path.join(ndk_bin, f"{android_target}{android_api_level}-clang{ext}"),
                "AR": os.path.join(ndk_bin, "llvm-ar"),
                "ADDR2LINE": os.path.join(ndk_bin, "llvm-addr2line"),
                "NM": os.path.join(ndk_bin, "llvm-nm"),
                "OBJCOPY": os.path.join(ndk_bin, "llvm-objcopy"),
                "OBJDUMP": os.path.join(ndk_bin, "llvm-objdump"),
                "READELF": os.path.join(ndk_bin, "llvm-readelf"),
                "ELFEDIT": os.path.join(ndk_bin, "llvm-elfedit"),
            }
        build_env = VirtualBuildEnv(self._recipe, auto_generate=True).vars()
        for var_name, var_path in recipe_vars.items():
            # User variables have more priority than Recipe ones, so if it was defined within
            # the build env then do nothing
            if build_env.get(var_name) is None:
                ret[var_name] = var_path
        return ret

    def _get_msvc_runtime_flag(self) -> str:
        if llvm_clang_front(self._recipe) == "clang":
            if self._recipe.settings.compiler.runtime == "dynamic":
                runtime_type = self._recipe.settings.get_safe("compiler.runtime_type")
                library = "msvcrtd" if runtime_type == "Debug" else "msvcrt"
                # The -D_DEBUG is important to link with the Debug MSVCP140D.dll
                debug = "-D_DEBUG " if runtime_type == "Debug" else ""
                return f"{debug}-D_DLL -D_MT -Xclang --dependent-lib={library}"
            return ""  # By default it already link statically

        flag = msvc_runtime_flag(self._recipe)
        if flag:
            flag = f"-{flag}"
        return flag

    def _msvc_extra_flags(self) -> list[str]:
        if is_msvc(self._recipe) and check_min_vs(
            self._recipe, "180", raise_invalid=False):
            return ["-FS"]
        return []

    def _add_msvc_flags(self, flags: list[str]) -> list[str]:
        # This is to avoid potential duplicate with users recipes -FS (already some in RecipeCenter)
        return [f for f in self.msvc_extra_flags if f not in flags]

    @staticmethod
    def _filter_list_empty_fields(v: list[str | None]) -> list[str]:
        return list(filter(bool, v))

    @property
    def cxxflags(self) -> list[str]:
        fpic = "-fPIC" if self.fpic else None
        ret = [
                  self.libcxx, self.cppstd, self.arch_flag, fpic, self.msvc_runtime_flag, self.sysroot_flag,
              ] + self.threads_flags
        apple_flags = [self.apple_isysroot_flag, self.apple_arch_flag, self.apple_min_version_flag]
        apple_flags += self.apple_extra_flags
        conf_flags = self._recipe.conf.get("tools.build:cxxflags", default=[], check_type=list)
        vs_flag = self._add_msvc_flags(self.extra_cxxflags)
        ret = ret + self.build_type_flags + apple_flags + self.extra_cxxflags + vs_flag + conf_flags
        return self._filter_list_empty_fields(ret)

    @property
    def cflags(self) -> list[str]:
        fpic = "-fPIC" if self.fpic else None
        ret = [self.cstd, self.arch_flag, fpic, self.msvc_runtime_flag, self.sysroot_flag] + self.threads_flags
        apple_flags = [self.apple_isysroot_flag, self.apple_arch_flag, self.apple_min_version_flag]
        apple_flags += self.apple_extra_flags
        conf_flags = self._recipe.conf.get("tools.build:cflags", default=[], check_type=list)
        vs_flag = self._add_msvc_flags(self.extra_cflags)
        ret = ret + self.build_type_flags + apple_flags + self.extra_cflags + vs_flag + conf_flags
        return self._filter_list_empty_fields(ret)

    @property
    def ldflags(self) -> list[str]:
        ret = [self.arch_flag, self.sysroot_flag, self.arch_ld_flag] + self.threads_flags
        apple_flags = [self.apple_isysroot_flag, self.apple_arch_flag, self.apple_min_version_flag]
        apple_flags += self.apple_extra_flags
        conf_flags = self._recipe.conf.get(
            "tools.build:sharedlinkflags", default=[], check_type=list)
        conf_flags.extend(
            self._recipe.conf.get(
                "tools.build:exelinkflags", default=[], check_type=list))
        linker_scripts = self._recipe.conf.get(
            "tools.build:linker_scripts", default=[], check_type=list)
        conf_flags.extend(["-T'" + linker_script + "'" for linker_script in linker_scripts])
        ret = ret + self.build_type_link_flags + apple_flags + self.extra_ldflags + conf_flags
        ret = ret + self.msvc_runtime_link_flags
        return self._filter_list_empty_fields(ret)

    @property
    def defines(self) -> list[str]:
        conf_flags = self._recipe.conf.get("tools.build:defines", default=[], check_type=list)
        ret = [self.ndebug, self.gcc_cxx11_abi] + self.extra_defines + conf_flags
        return self._filter_list_empty_fields(ret)

    @property
    def rcflags(self) -> list[str]:
        conf_flags = self._recipe.conf.get("tools.build:rcflags", default=[], check_type=list)
        return self._filter_list_empty_fields(conf_flags)

    def _include_obj_arc_flags(self, env: Environment):
        enable_arc = self._recipe.conf.get("tools.apple:enable_arc", check_type=bool)
        fobj_arc = ""
        if enable_arc:
            fobj_arc = "-fobjc-arc"
        if enable_arc is False:
            fobj_arc = "-fno-objc-arc"
        if fobj_arc:
            env.append('OBJCFLAGS', [fobj_arc])
            env.append('OBJCXXFLAGS', [fobj_arc])

    def environment(self) -> Environment:
        env = Environment()
        # Setting Android cross-compilation flags (if exist)
        if self.android_cross_flags:
            for env_var, env_value in self.android_cross_flags.items():
                unix_env_value = unix_path(self._recipe, env_value)
                env.define(env_var, unix_env_value)
        else:
            # Setting user custom compiler executables flags
            compilers_by_conf = self._recipe.conf.get(
                "tools.build:compiler_executables", default={}, check_type=dict)
            if compilers_by_conf:
                compilers_mapping = {
                    "c": "CC", "cpp": "CXX", "cuda": "NVCC", "fortran": "FC", "rc": "RC", "nm": "NM", "ranlib": "RANLIB", "objdump": "OBJDUMP", "strip": "STRIP",
                }
                for comp, env_var in compilers_mapping.items():
                    if comp in compilers_by_conf:
                        compiler = compilers_by_conf[comp]
                        # upstream issue 13780
                        compiler = unix_path(self._recipe, compiler)
                        env.define(env_var, compiler)
            compiler_setting = self._recipe.settings.get_safe("compiler")
            if compiler_setting == "msvc":
                # None of them defined, if one is defined by user, user should define the other too
                if "c" not in compilers_by_conf and "cpp" not in compilers_by_conf:
                    env.define("CC", "cl -nologo")
                    env.define("CXX", "cl -nologo")
                    env.define("LD", "link -nologo")
                    env.define("AR", "lib")
                    env.define("NM", "dumpbin -symbols")
                    env.define("OBJDUMP", ":")
                    env.define("RANLIB", ":")
                    env.define("STRIP", ":")

        env.append("CPPFLAGS", [f"-D{d}" for d in self.defines])
        env.append("CXXFLAGS", self.cxxflags)
        env.append("CFLAGS", self.cflags)
        env.append("LDFLAGS", self.ldflags)
        if self.rcflags:
            env.append("RCFLAGS", self.rcflags)
        env.prepend_path("PKG_CONFIG_PATH", self._recipe.folders.generators)
        # Objective C/C++
        self._include_obj_arc_flags(env)
        # Issue related: upstream issue 15486
        if self._is_cross_building and self._recipe.conf_build:
            compilers_build_mapping = (self._recipe.conf_build.get(
                "tools.build:compiler_executables", default={}, check_type=dict))
            if "c" in compilers_build_mapping:
                env.define("CC_FOR_BUILD", compilers_build_mapping["c"])
            if "cpp" in compilers_build_mapping:
                env.define("CXX_FOR_BUILD", compilers_build_mapping["cpp"])
        return env

    def vars(self):
        return self.environment().vars(self._recipe, scope="build")

    def generate(self, env: Environment | None = None, scope: str = "build"):
        env = env or self.environment()
        env = env.vars(self._recipe, scope=scope)
        env.save_script("autotoolstoolchain")
        self.generate_args()
        VCVars(self._recipe).generate(scope=scope)

    def _default_configure_shared_flags(self) -> list[str]:
        args = []
        # Just add these flags if there's a shared option defined (never add to exe's)
        shared = self._recipe.options.get_safe("shared")
        if shared is True:
            args.extend(["--enable-shared", "--disable-static"])
        elif shared is False:
            args.extend(["--disable-shared", "--enable-static"])

        return args

    def _default_configure_install_flags(self) -> list[str]:
        configure_install_flags = []

        def _get_argument(argument_name, cppinfo_name):
            elements = getattr(self._recipe.infos.package, cppinfo_name)
            return f"--{argument_name}=${{prefix}}/{elements[0]}" if elements else ""

        # If someone want arguments but not the defaults can pass them in args manually
        configure_install_flags.extend(
            [
                f"--prefix={self._prefix}",
                _get_argument("bindir", "bindirs"),
                _get_argument("sbindir", "bindirs"),
                _get_argument("libdir", "libdirs"),
                _get_argument("includedir", "includedirs"),
                _get_argument("oldincludedir", "includedirs"),
                _get_argument("datarootdir", "resdirs"),
            ])
        return [el for el in configure_install_flags if el]

    @staticmethod
    def _default_autoreconf_flags() -> list[str]:
        return ["--force", "--install"]

    def _get_triplets(self) -> list[str]:
        triplets = []
        for flag, value in (
                ("--host=", self._host), ("--build=", self._build), ("--target=", self._target),
        ):
            if value:
                triplets.append(f'{flag}{value}')
        return triplets

    def update_configure_args(self, updated_flags: dict[str, Any]):
        """
        Helper to update/prune flags from ``self.configure_args``.

        :param updated_flags: ``dict`` with arguments as keys and their argument values.
                              Notice that if argument value is ``None``, this one will be pruned.
        """
        self._update_flags("configure_args", updated_flags)

    def update_make_args(self, updated_flags: dict[str, Any]):
        """
        Helper to update/prune arguments from ``self.make_args``.

        :param updated_flags: ``dict`` with arguments as keys and their argument values.
                              Notice that if argument value is ``None``, this one will be pruned.
        """
        self._update_flags("make_args", updated_flags)

    def update_autoreconf_args(self, updated_flags: dict[str, Any]):
        """
        Helper to update/prune arguments from ``self.autoreconf_args``.

        :param updated_flags: ``dict`` with arguments as keys and their argument values.
                              Notice that if argument value is ``None``, this one will be pruned.
        """
        self._update_flags("autoreconf_args", updated_flags)

    # FIXME: Remove all these update_xxxx whenever xxxx_args are dicts or new ones replace them
    def _update_flags(self, attr_name: str, updated_flags: dict[str, Any]):

        def _list_to_dict(flags):
            ret = {}
            for flag in flags:
                # Only splitting if "=" is there
                option = flag.split("=", 1)
                if len(option) == 2:
                    ret[option[0]] = option[1]
                else:
                    ret[option[0]] = ""
            return ret

        def _dict_to_list(flags):
            return [f"{k}={v}" if v else k for k, v in flags.items() if v is not None]

        self_args = getattr(self, attr_name)
        # FIXME: if xxxxx_args -> dict-type at some point, all these lines could be removed
        options = _list_to_dict(self_args)
        # Add/update/remove the current xxxxx_args with the new flags given
        options.update(updated_flags)
        # Update the current ones
        setattr(self, attr_name, _dict_to_list(options))

    def generate_args(self):
        args = {
            "configure_args": cmd_args_to_string(self.configure_args), "make_args": cmd_args_to_string(self.make_args), "autoreconf_args": cmd_args_to_string(self.autoreconf_args),
        }
        save_toolchain_args(args, namespace=self._namespace)
