from thirdparty import RecipeBase
from thirdparty.tools.gnu import PkgConfig

class Recipe(RecipeBase):
    name = "opengl"
    version = "0.0.0"
    version = "system"
    license = "MIT"

    def package_info(self):
        # TODO: Workaround for #2311 until a better solution can be found

        self.cpp_info.set_property("cmake_file_name", "opengl_system")

        self.cpp_info.bindirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        if self.settings.os == "Macos":
            self.cpp_info.defines.append("GL_SILENCE_DEPRECATION=1")
            self.cpp_info.frameworks.append("OpenGL")
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["opengl32"]
        elif self.settings.os in ["Linux", "FreeBSD", "SunOS"]:
            pkg_config = PkgConfig(self, 'gl')
            pkg_config.fill_cpp_info(self.cpp_info, is_system=self.settings.os != "FreeBSD")
