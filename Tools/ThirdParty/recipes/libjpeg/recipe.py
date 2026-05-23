from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_conandata_patches, chdir, copy, get, load, replace_in_file, rm, rmdir, save
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import MSBuild, MSBuildToolchain
import os
import re

class Recipe(RecipeBase):
    name = "libjpeg"
    version = "9f"
    license = "IJG"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    @property
    def _is_cl_like(self):
        return self.settings.compiler.get_safe("runtime") is not None

    @property
    def _settings_build(self):
        return getattr(self, "settings_build", self.settings)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def build_requirements(self):
        if self._settings_build.os == "Windows" and not self._is_cl_like:
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2/latest")

    def source(self):
        get(self, url="https://ijg.org/files/jpegsrc.v9f.tar.gz", sha256="04705c110cb2469caa79fb71fba3d7bf834914706e9641a4589485c1f832565b", destination=self.source_folder, strip_root=True)

    def generate(self):
        if self._is_cl_like:
            tc = MSBuildToolchain(self)
            tc.cflags.append("-DLIBJPEG_BUILDING")
            if not self.options.shared:
                tc.cflags.append(" -DLIBJPEG_STATIC")
            tc.generate()
        else:
            env = VirtualBuildEnv(self)
            env.generate()
            tc = AutotoolsToolchain(self)
            tc.extra_defines.append("LIBJPEG_BUILDING")
            tc.generate()

    def build(self):
        apply_conandata_patches(self)
        if self._is_cl_like:
            with chdir(self, self.source_folder):
                self.run("nmake /f makefile.vs setupcopy-v16")

                # Rename target to 'libjpeg.lib' to match legacy behaviour (otherwise we break backwards compatibility)
                # static: "libjpeg.lib"
                # shared: "libjpeg.lib" (import), "libjpeg-9.dll" (DLL)
                jpeg_vcxproj = os.path.join(self.source_folder, "jpeg.vcxproj")
                target_name = "libjpeg-9" if self.options.shared else "libjpeg"
                replace_in_file(self, jpeg_vcxproj, """<PropertyGroup Label="UserMacros" />""",
                                f""" <PropertyGroup Label="UserMacros" /><PropertyGroup Label="TargetName"> <TargetName>{target_name}</TargetName></PropertyGroup>
                                """)
                if self.options.shared:
                    replace_in_file(self, jpeg_vcxproj, "</SubSystem>",
                                    "</SubSystem><ImportLibrary>$(OutDir)libjpeg.lib</ImportLibrary>")

                # Support static/shared
                if self.options.shared:
                    replace_in_file(self, jpeg_vcxproj,
                        "<ConfigurationType>StaticLibrary</ConfigurationType>",
                        "<ConfigurationType>DynamicLibrary</ConfigurationType>"
                    )

                # Don't force LTO
                replace_in_file(self, jpeg_vcxproj, "<WholeProgramOptimization>true</WholeProgramOptimization>", "")

                # Inject conan-generated .props file
                # Note: importing it right before Microsoft.Cpp.props also ensures we correctly
                #       handle the toolset setting
                conantoolchain_props = os.path.join(self.generators_folder, MSBuildToolchain.filename)
                replace_in_file(
                    self, jpeg_vcxproj,
                    """<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />""",
                    f"""<Import Project="{conantoolchain_props}" /><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />""",
                )

                # Patch settings for a different build type
                if self.settings.build_type is not "Release":
                    replacements = {
                        "Release": str(self.settings.build_type)
                    }
                    if self.settings.build_type == "Debug":
                        replacements.update({
                            "<Optimization>Full": "<Optimization>Disabled",
                            "NDEBUG;": "_DEBUG;",
                        })
                    for key, value in replacements.items():
                        replace_in_file(self, jpeg_vcxproj, key, value)

                    replace_in_file(self, os.path.join(self.source_folder, "jpeg.sln"), "Release", str(self.settings.build_type))

                msbuild = MSBuild(self)
                if self.settings.arch == "x86":
                    # This .sln uses "Win32" instead of the usual "x86"
                    # as the solution platform, so need to override this
                    msbuild.platform = "Win32"
                msbuild.build(sln="jpeg.sln")
        else:
            autotools = Autotools(self)
            autotools.configure()
            autotools.make()

    def package(self):
        copy(self, "README", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        if self._is_cl_like:
            for filename in ["jpeglib.h", "jerror.h", "jconfig.h", "jmorecfg.h"]:
                copy(self, filename, src=self.source_folder, dst=os.path.join(self.package_folder, "include"), keep_path=False)

            copy(self, "*.lib", src=self.source_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            if self.options.shared:
                copy(self, "*.dll", src=self.source_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
        else:
            autotools = Autotools(self)
            autotools.install()
            if self.settings.os == "Windows" and self.options.shared:
                rm(self, "*[!.dll]", os.path.join(self.package_folder, "bin"))
            else:
                rmdir(self, os.path.join(self.package_folder, "bin"))
            rm(self, "*.la", os.path.join(self.package_folder, "lib"))
            rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
            rmdir(self, os.path.join(self.package_folder, "share"))
            fix_apple_shared_install_name(self)

        for fn in ("jpegint.h", "transupp.h",):
            copy(self, fn, src=self.source_folder, dst=os.path.join(self.package_folder, "include"))

        for fn in ("jinclude.h", "transupp.c",):
            copy(self, fn, src=self.source_folder, dst=os.path.join(self.package_folder, "res"))

        # Remove export decorations of transupp symbols
        for relpath in os.path.join("include", "transupp.h"), os.path.join("res", "transupp.c"):
            path = os.path.join(self.package_folder, relpath)
            save(self, path, re.subn(r"(?:EXTERN|GLOBAL)\(([^)]+)\)", r"\1", load(self, path))[0])

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "JPEG")
        self.cpp_info.set_property("cmake_target_name", "JPEG::JPEG")
        self.cpp_info.set_property("pkg_config_name", "libjpeg")
        prefix = "lib" if self._is_cl_like else ""
        self.cpp_info.libs = [f"{prefix}jpeg"]
        self.cpp_info.resdirs = ["res"]
        if not self.options.shared:
            self.cpp_info.defines.append("LIBJPEG_STATIC")

        # TODO: to remove in conan v2 once legacy generators removed
        self.cpp_info.names["cmake_find_package"] = "JPEG"
        self.cpp_info.names["cmake_find_package_multi"] = "JPEG"
