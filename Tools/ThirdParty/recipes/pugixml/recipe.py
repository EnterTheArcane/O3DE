import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import collect_libs, copy, get, load, replace_in_file, rmdir, save
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "pugixml"
    version = "1.15"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "header_only": [True, False],
        "wchar_mode": [True, False],
        "no_exceptions": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "header_only": False,
        "wchar_mode": False,
        "no_exceptions": False,
    }

    def configure(self):
        if self.options.shared or self.options.header_only:
            self.options.rm_safe("fPIC")
        if self.options.header_only:
            del self.options.shared

    def latest_version(self):
        repo = GithubRepository(self, "zeux/pugixml")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz",
            sha256="655ade57fa703fb421c2eb9a0113b5064bddb145d415dd1f88c79353d90d511a",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        if not self.options.header_only:
            tc = CMakeToolchain(self)
            tc.variables["BUILD_TESTS"] = False
            # For msvc shared
            tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
            tc.generate()

    def build(self):
        if not self.options.header_only:
            header_file = os.path.join(self.folders.source, "src", "pugiconfig.hpp")
            # For the library build mode, options applied via change the configuration file
            if self.options.wchar_mode:
                replace_in_file(self, header_file, "// #define PUGIXML_WCHAR_MODE", "#define PUGIXML_WCHAR_MODE", strict=False)
            if self.options.no_exceptions:
                replace_in_file(self, header_file, "// #define PUGIXML_NO_EXCEPTIONS", "#define PUGIXML_NO_EXCEPTIONS", strict=False)
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        readme_contents = load(self, os.path.join(self.folders.source, "readme.txt"))
        license_contents = readme_contents[readme_contents.find("This library is"):]
        save(self, os.path.join(self.folders.package, "licenses", "LICENSE"), license_contents)
        if self.options.header_only:
            source_dir = os.path.join(self.folders.source, "src")
            copy(self, "*", src=source_dir, dst=os.path.join(self.folders.package, "include"))
        else:
            cmake = CMake(self)
            cmake.install()
            rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
            rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "pugixml")
        self.info.set_property("cmake_target_name", "pugixml::pugixml")
        self.info.set_property("pkg_config_name", "pugixml")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.system_libs.append("m")
        if self.options.header_only:
            # For the "header_only" mode, options applied via global definitions
            self.info.defines.append("PUGIXML_HEADER_ONLY")
            if self.options.wchar_mode:
                self.info.defines.append("PUGIXML_WCHAR_MODE")
            if self.options.no_exceptions:
                self.info.defines.append("PUGIXML_NO_EXCEPTIONS")
            self.info.bindirs = []
            self.info.libdirs = []
        else:
            self.info.set_property(
                "cmake_target_aliases",
                ["pugixml::shared"] if self.options.shared else ["pugixml::static"],
            )
            self.info.libs = collect_libs(self)
