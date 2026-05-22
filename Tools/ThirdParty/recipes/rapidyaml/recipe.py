from thirdparty import RecipeBase
from thirdparty.tools.microsoft import check_min_vs
from thirdparty.tools.files import apply_patches, get, copy, rm, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
import os


class Recipe(RecipeBase):
    name = "rapidyaml"
    version = "0.10.0"
    license = ("MIT",)
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_default_callbacks": [True, False],
        "with_tab_tokens": [True, False],
        "with_default_callback_uses_exceptions": [True, False],
        "with_assert": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_default_callbacks": True,
        "with_tab_tokens": False,
        "with_default_callback_uses_exceptions": False,
        "with_assert": False,
    }

    @property
    def _minimum_cpp_standard(self):
        return 11

    def requirements(self) -> list[str]:
        return []  # c4core is bundled in rapidyaml

    def source(self):
        get(
            url="https://github.com/biojppm/rapidyaml/releases/download/v0.10.0/rapidyaml-0.10.0-src.tgz",
            sha256="54eb1050789809a26c780f80857b7668a5b3123405d6514a65d733e4292c690b",
            dest=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["RYML_DEFAULT_CALLBACKS"] = self.options.with_default_callbacks
        if Version(self.version) >= "0.4.0":
            tc.variables["RYML_WITH_TAB_TOKENS"] = self.options.with_tab_tokens
        if Version(self.version) >= "0.6.0":
            tc.variables["RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS"] = (
                self.options.with_default_callback_uses_exceptions
            )
            tc.variables["RYML_USE_ASSERT"] = self.options.with_assert
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            pattern="LICENSE.txt",
            dst=os.path.join(self.package_folder, "licenses"),
            src=self.source_folder,
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "cmake"))
        rm("*.natvis", os.path.join(self.package_folder, "include"))
