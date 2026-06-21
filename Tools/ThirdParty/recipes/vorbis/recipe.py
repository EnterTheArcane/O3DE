import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeConfigDeps, CMakeToolchain
from thirdparty.files import apply_patches, copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "vorbis"
    version = "1.3.7"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def requirements(self):
        self.requires("ogg", transitive_headers=True, transitive_libs=True)

    def latest_version(self):
        repo = GithubRepository(self, "xiph/vorbis")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/xiph/vorbis/archive/v1.3.7.tar.gz",
            sha256="270c76933d0934e42c5ee0a54a36280e2d87af1de3cc3e584806357e237afd13",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        # Relocatable shared lib on Macos
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
        tc.generate()
        cd = CMakeConfigDeps(self)
        cd.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Vorbis")
        # see https://github.com/recipe-io/recipe-center-index/pull/4173
        self.cpp_info.set_property("pkg_config_name", "vorbis-all-do-not-use")

        # vorbis
        self.cpp_info.components["vorbismain"].set_property("cmake_target_name", "Vorbis::vorbis")
        self.cpp_info.components["vorbismain"].set_property("pkg_config_name", "vorbis")
        self.cpp_info.components["vorbismain"].libs = ["vorbis"]
        if self.settings.os in ["Linux", "FreeBSD", "Android"]:
            self.cpp_info.components["vorbismain"].system_libs.append("m")
        if self.settings.os == "Android":
            self.cpp_info.components["vorbismain"].system_libs.append("log")
        self.cpp_info.components["vorbismain"].requires = ["ogg::ogg"]

        # TODO: Upstream VorbisConfig.cmake defines components 'Enc' and 'File',
        # which are related to imported targets Vorbis::vorbisenc and Vorbis::vorbisfile
        # Find a way to emulate this in CMakeConfigDeps. See upstream issue 10258

        # vorbisenc
        self.cpp_info.components["vorbisenc"].set_property("cmake_target_name", "Vorbis::vorbisenc")
        self.cpp_info.components["vorbisenc"].set_property("pkg_config_name", "vorbisenc")
        self.cpp_info.components["vorbisenc"].libs = ["vorbisenc"]
        self.cpp_info.components["vorbisenc"].requires = ["vorbismain"]

        # vorbisfile
        self.cpp_info.components["vorbisfile"].set_property("cmake_target_name", "Vorbis::vorbisfile")
        self.cpp_info.components["vorbisfile"].set_property("pkg_config_name", "vorbisfile")
        self.cpp_info.components["vorbisfile"].libs = ["vorbisfile"]
        self.cpp_info.components["vorbisfile"].requires = ["vorbismain"]

        # vorbisenc-alias
        self.cpp_info.components["vorbisenc-alias"].requires.append("vorbisenc")
        self.cpp_info.components["vorbisfile-alias"].requires.append("vorbisfile")
