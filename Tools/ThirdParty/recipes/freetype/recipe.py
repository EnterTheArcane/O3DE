import os
import re
import textwrap

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.files import (
    collect_libs, copy, load,
    get, replace_in_file, rmdir, save,
)
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "freetype"
    version = "2.14.3"
    license = "FTL"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_bzip2": [True, False],
        "subpixel": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_bzip2": True,
        "subpixel": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("brotli")
        self.requires("bzip2")
        self.requires("libpng")
        self.requires("zlib")

    def latest_version(self):
        repo = GithubRepository(self, "freetype/freetype")
        return Version(repo.latest_release.removeprefix("VER-").replace("-", "."))

    def source(self):
        get(
            self,
            url="https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz",
            sha256="36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["FT_REQUIRE_ZLIB"] = True
        tc.variables["FT_DISABLE_ZLIB"] = False
        tc.variables["FT_REQUIRE_PNG"] = True
        tc.variables["FT_DISABLE_PNG"] = False
        tc.variables["FT_REQUIRE_BZIP2"] = True
        tc.variables["FT_DISABLE_BZIP2"] = False
        # TODO: Harfbuzz can be added as an option as soon as it is available.
        tc.variables["FT_REQUIRE_HARFBUZZ"] = False
        tc.variables["FT_DISABLE_HARFBUZZ"] = True
        tc.variables["FT_REQUIRE_BROTLI"] = True
        tc.variables["FT_DISABLE_BROTLI"] = False
        # Generate a relocatable shared lib on Macos
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
        tc.generate()

    def _patch_sources(self):
        # Do not accidentally enable dependencies we have disabled
        cmakelists = os.path.join(self.source_folder, "CMakeLists.txt")
        if_harfbuzz_found = "if (HarfBuzz_FOUND)"
        replace_in_file(self, cmakelists, "find_package(HarfBuzz ${HARFBUZZ_MIN_VERSION})", "", strict=False)
        replace_in_file(self, cmakelists, if_harfbuzz_found, "if(0)", strict=False)
        # the custom FindBrotliDec of upstream is too fragile
        replace_in_file(
            self, cmakelists,
            "find_package(BrotliDec REQUIRED)",
            "find_package(Brotli REQUIRED)\n"
            "set(BROTLIDEC_FOUND 1)\n"
            "set(BROTLIDEC_LIBRARIES \"brotli::brotli\")",
            strict=False)

        config_h = os.path.join(self.source_folder, "include", "freetype", "config", "ftoption.h")
        if self.options.subpixel:
            replace_in_file(self, config_h, "/* #define FT_CONFIG_OPTION_SUBPIXEL_RENDERING */", "#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING", strict=False)

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def _make_freetype_config(self, version):
        freetype_config_in = os.path.join(self.source_folder, "builds", "unix", "freetype-config.in")
        if not os.path.isdir(os.path.join(self.package_folder, "bin")):
            os.makedirs(os.path.join(self.package_folder, "bin"))
        freetype_config = os.path.join(self.package_folder, "bin", "freetype-config")
        save(self, freetype_config, load(self, freetype_config_in))
        libs = "-lfreetyped" if self.settings.build_type == "Debug" else "-lfreetype"
        staticlibs = f"-lm {libs}" if self.settings.os == "Linux" else libs
        replace_in_file(self, freetype_config, r"%PKG_CONFIG%", r"/bin/false")  # never use pkg-config
        replace_in_file(self, freetype_config, r"%prefix%", r"$recipe_prefix")
        replace_in_file(self, freetype_config, r"%exec_prefix%", r"$recipe_exec_prefix")
        replace_in_file(self, freetype_config, r"%includedir%", r"$recipe_includedir")
        replace_in_file(self, freetype_config, r"%libdir%", r"$recipe_libdir")
        replace_in_file(self, freetype_config, r"%ft_version%", r"$recipe_ftversion")
        replace_in_file(self, freetype_config, r"%LIBSSTATIC_CONFIG%", r"$recipe_staticlibs")
        replace_in_file(self, freetype_config, r"-lfreetype", libs)
        replace_in_file(
            self, freetype_config, r"export LC_ALL", textwrap.dedent(
                """\
                            export LC_ALL
                            BINDIR=$(dirname $0)
                            recipe_prefix=$(dirname $BINDIR)
                            recipe_exec_prefix=${{recipe_prefix}}/bin
                            recipe_includedir=${{recipe_prefix}}/include
                            recipe_libdir=${{recipe_prefix}}/lib
                            recipe_ftversion={version}
                            recipe_staticlibs="{staticlibs}"
                        """).format(version=version, staticlibs=staticlibs))

    def _extract_libtool_version(self):
        conf_raw = load(self, os.path.join(self.source_folder, "builds", "unix", "configure.raw"))
        return next(re.finditer(r"^version_info='([0-9:]+)'", conf_raw, flags=re.M)).group(1).replace(":", ".")

    @property
    def _libtool_version_txt(self):
        return os.path.join(self.package_folder, "res", "freetype-libtool-version.txt")

    def package(self):
        cmake = CMake(self)
        cmake.install()

        libtool_version = self._extract_libtool_version()
        save(self, self._libtool_version_txt, libtool_version)
        self._make_freetype_config(libtool_version)

        doc_folder = os.path.join(self.source_folder, "docs")
        license_folder = os.path.join(self.package_folder, "licenses")
        copy(self, "FTL.TXT", doc_folder, license_folder)
        copy(self, "GPLv2.TXT", doc_folder, license_folder)
        copy(self, "LICENSE.TXT", doc_folder, license_folder)

        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        self._create_cmake_module_variables(
            os.path.join(self.package_folder, self._module_vars_rel_path)
        )
        self._create_cmake_module_alias_targets(
            os.path.join(self.package_folder, self._module_target_rel_path),
            {"freetype": "Freetype::Freetype"}
        )

    def _create_cmake_module_variables(self, module_file):
        content = textwrap.dedent(
            f"""\
            set(FREETYPE_FOUND TRUE)
            if(DEFINED Freetype_INCLUDE_DIRS)
                set(FREETYPE_INCLUDE_DIRS ${{Freetype_INCLUDE_DIRS}})
            endif()
            if(DEFINED Freetype_LIBRARIES)
                set(FREETYPE_LIBRARIES ${{Freetype_LIBRARIES}})
            endif()
            set(FREETYPE_VERSION_STRING "{self.version}")
        """)
        save(self, module_file, content)

    def _create_cmake_module_alias_targets(self, module_file, targets):
        content = ""
        for alias, aliased in targets.items():
            content += textwrap.dedent(
                """\
                                if(TARGET {aliased} AND NOT TARGET {alias})
                                    add_library({alias} INTERFACE IMPORTED)
                                    set_property(TARGET {alias} PROPERTY INTERFACE_LINK_LIBRARIES {aliased})
                                endif()
                            """.format(alias=alias, aliased=aliased))
        save(self, module_file, content)

    @property
    def _module_vars_rel_path(self):
        return os.path.join("lib", "cmake", f"recipe-official-{self.name}-variables.cmake")

    @property
    def _module_target_rel_path(self):
        return os.path.join("lib", "cmake", f"recipe-official-{self.name}-targets.cmake")

    @staticmethod
    def _chmod_plus_x(filename):
        if os.name == "posix" and (os.stat(filename).st_mode & 0o111) != 0o111:
            os.chmod(filename, os.stat(filename).st_mode | 0o111)

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_module_file_name", "Freetype")
        self.cpp_info.set_property("cmake_file_name", "freetype")
        self.cpp_info.set_property("cmake_target_name", "Freetype::Freetype")
        self.cpp_info.set_property("cmake_target_aliases", ["freetype"])  # other possible target name in upstream config file
        self.cpp_info.set_property("cmake_build_modules", [self._module_vars_rel_path])
        self.cpp_info.set_property("pkg_config_name", "freetype2")
        self.cpp_info.libs = collect_libs(self)
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
        self.cpp_info.includedirs.append(os.path.join("include", "freetype2"))

        libtool_version = load(self, self._libtool_version_txt).strip()
        self.conf_info.define("user.freetype:libtool_version", libtool_version)
        self.cpp_info.set_property("system_package_version", libtool_version)

        self.cpp_info.set_property("component_version", libtool_version)
        freetype_config = os.path.join(self.package_folder, "bin", "freetype-config")
        self._chmod_plus_x(freetype_config)
