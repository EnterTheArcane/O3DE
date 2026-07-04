from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeToolchain, CMakeDeps
from thirdparty.files import get, copy, replace_in_file


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    assembly: bool = True
    memopt: bool = True
    sparse: bool = True


class Recipe(RecipeBase[_Options]):
    name = "xnnpack"
    version = "cci.20241203"
    license = "BSD-3-Clause"

    def requirements(self):
        self.requires_tool("cmake")
        self.requires("cpuinfo")
        self.requires("fp16")
        self.requires("pthreadpool")
        self.requires("fxdiv")

    def source(self):
        get(
            self,
            url="https://github.com/google/XNNPACK/archive/51a0103656eff6fc9bfd39a4597923c4b542c883.tar.gz",
            sha256="3ff271a4f41e798616950b0c690d127afcc530a6180f78c78c4b2932920911aa",
            destination=self.folders.source,
            strip_root=True)
        copy(self, "xnnpack_project_include.cmake", src=self.folders.recipe, dst=self.folders.source)
        replace_in_file(
            self,
            self.folders.source / "CMakeLists.txt",
            "LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}",
            "LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["XNNPACK_LIBRARY_TYPE"] = "shared" if self.options.shared else "static"
        tc.cache_variables["CMAKE_PROJECT_XNNPACK_INCLUDE"] = str(self.folders.source / "xnnpack_project_include.cmake")
        tc.variables["XNNPACK_ENABLE_ASSEMBLY"] = self.options.assembly
        tc.variables["XNNPACK_ENABLE_MEMOPT"] = self.options.memopt
        tc.variables["XNNPACK_ENABLE_SPARSE"] = self.options.sparse
        tc.variables["XNNPACK_BUILD_TESTS"] = False
        tc.variables["XNNPACK_BUILD_BENCHMARKS"] = False
        tc.variables["XNNPACK_USE_SYSTEM_LIBS"] = True
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.pic
        tc.variables["CMAKE_SKIP_INSTALL_ALL_DEPENDENCY"] = True
        tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.cache_variables["XNNPACK_ENABLE_KLEIDIAI"] = False
        tc.generate()

        deps = CMakeDeps(self)
        # XNNPACK's CMake scripts reference bare (un-namespaced) target names.
        deps.set_property("cpuinfo", "cmake_target_name", "cpuinfo")
        deps.set_property("cpuinfo", "cmake_target_aliases", ["clog"])
        deps.set_property("pthreadpool", "cmake_target_name", "pthreadpool")
        deps.set_property("fp16", "cmake_target_name", "fp16")
        deps.set_property("fxdiv", "cmake_target_name", "fxdiv")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build(target="XNNPACK")

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.components["xnnpack"].set_property("cmake_target_name", "xnnpack::xnnpack")
        self.info.components["xnnpack"].libs = ["XNNPACK"]
        self.info.components["xnnpack"].requires = [
            "fxdiv::fxdiv",
            "fp16::fp16",
            "cpuinfo::cpuinfo",
            "pthreadpool::pthreadpool",
            "microkernels-prod",
        ]

        self.info.components["microkernels-prod"].libs = ["microkernels-prod"]
        self.info.components["microkernels-prod"].requires = ["fxdiv::fxdiv"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.info.components["xnnpack"].system_libs = ["m"]
            self.info.components["microkernels-prod"].system_libs = ["m"]
