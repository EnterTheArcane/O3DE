# Ported from conan-center-index/mimalloc by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import (
    apply_patches,
    get,
    copy,
    rm,
    rmdir,
    replace_in_file,
    collect_libs,
)
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime, VCVars
from thirdparty.tools.scm import Version
import os
import shutil


class Recipe(RecipeBase):
    name = "mimalloc"
    version = "3.3.2"
    license = "MIT"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "secure": [True, False],
        "override": [True, False],
        "inject": [True, False],
        "single_object": [True, False],
        "guarded": [True, False],
        "win_redirect": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "secure": False,
        "override": False,
        "inject": False,
        "single_object": False,
        "guarded": False,
        "win_redirect": False,
    }

    def source(self):
        get(
            url="https://github.com/microsoft/mimalloc/archive/v3.3.2.tar.gz",
            dest=self.source_folder,
            sha256="ca02384e007f46950598500dfaebde5ff9948c1d231f5a81b058799afa64bbbb",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MI_BUILD_TESTS"] = "OFF"
        tc.variables["MI_BUILD_SHARED"] = self.options.shared
        tc.variables["MI_BUILD_STATIC"] = not self.options.shared
        tc.variables["MI_BUILD_OBJECT"] = self.options.get("single_object", False)
        tc.variables["MI_OVERRIDE"] = "ON" if self.options.override else "OFF"
        tc.variables["MI_SECURE"] = "ON" if self.options.secure else "OFF"
        tc.variables["MI_WIN_REDIRECT"] = (
            "ON" if self.options.get("win_redirect") else "OFF"
        )
        tc.variables["MI_INSTALL_TOPLEVEL"] = "ON"
        tc.variables["MI_GUARDED"] = self.options.get("guarded", False)
        tc.generate()

        if self.is_windows:
            vcvars = VCVars(self)
            vcvars.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            pattern="LICENSE",
            dst=os.path.join(self.package_folder, "licenses"),
            src=self.source_folder,
        )
        cmake = CMake(self)
        cmake.install()

        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))

        if self.is_windows and self.options.shared:
            copy(
                "mimalloc-redirect.dll",
                src=os.path.join(self.source_folder, "bin"),
                dst=os.path.join(self.package_folder, "bin"),
            )
            copy(
                "minject.exe",
                src=os.path.join(self.source_folder, "bin"),
                dst=os.path.join(self.package_folder, "bin"),
            )

        rmdir(os.path.join(self.package_folder, "share"))

    @property
    def _obj_name(self):
        name = "mimalloc"
        if self.options.secure:
            name += "-secure"
        if self.build_type not in ("Release", "RelWithDebInfo", "MinSizeRel"):
            name += "-{}".format(str(self.build_type).lower())
        return name

    @property
    def _lib_name(self):
        name = "mimalloc" if self.is_windows else "libmimalloc"

        if self.is_windows and not self.options.shared:
            name += "-static"
        if self.options.secure:
            name += "-secure"
        if self.build_type not in ("Release", "RelWithDebInfo", "MinSizeRel"):
            name += "-{}".format(str(self.build_type).lower())
        return name
