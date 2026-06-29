from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMakeToolchain, CMake
from thirdparty.files import load, save, get, copy, replace_in_file
from thirdparty.microsoft import msvc_runtime_flag, is_msvc


# Packages PhysX normally fetches via physx/dependencies.xml + packman. We download them
# directly from the packman CDN (no packman scripts). Each entry is (archive, sha256).
_PACKMAN_CDN = "https://d4i3qtqj3r0z5.cloudfront.net"

_CMAKEMODULES = (
    "CMakeModules@1.28.trunk.31965103.7z",
    "0cbfc1fad314443140943264089e3e6fc28917bb6393960eb35a50716e8c472b",
)

# GPU runtime blobs are only built for / shipped on Win + Linux x64 and Linux arm64.
_PHYSXGPU = (
    "PhysXGpu@104.1-5.1.1253.32184287-public.zip",
    "381b35750a0596d4a83f85eba5bb79edfdd63aae6865aabbf8e160b58422b0df",
)

_PHYSXDEVICE = (
    "PhysXDevice@18.12.7.3.7z",
    "c9137c1978daa2d5c62ca44dcd472fbc597e1717b3a0011fb849148790202c39",
)

# All PhysX 5 library targets (used for PIC patching and static-lib packaging).
_PHYSX_TARGETS = (
    "FastXml",
    "LowLevel",
    "LowLevelAABB",
    "LowLevelDynamics",
    "PhysX",
    "PhysXCharacterKinematic",
    "PhysXCommon",
    "PhysXCooking",
    "PhysXExtensions",
    "PhysXFoundation",
    "PhysXPvdSDK",
    "PhysXTask",
    "PhysXVehicle",
    "PhysXVehicle2",
    "SceneQuery",
    "SimulationController",
)
# Internal helper static libs. On mac/linux/android/ios these are merged into the public
# libraries, but on Windows they are emitted as separate archives that static consumers must
# link (matching the legacy FindPhysX5.cmake EXTRA_STATIC_LIBS list).
_PHYSX_INTERNAL_STATIC = (
    "LowLevel",
    "LowLevelAABB",
    "LowLevelDynamics",
    "PhysXTask",
    "SceneQuery",
    "SimulationController",
)


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    release_build_type: Literal["profile", "release"] = "release"
    enable_simd: bool = True
    enable_float_point_precise_math: bool = False


class Recipe(RecipeBase[_Options]):
    name = "physx"
    version = "5.1.1"
    license = "BSD-3-Clause"

    def configure(self):
        if self.settings.os != "Windows":
            self.options.enable_float_point_precise_math = False
        if self.settings.os not in ["Windows", "Android"]:
            self.options.enable_simd = False
        # Android needs an API level; default it when the profile didn't supply one.
        if self.settings.os == "Android" and not self.settings.get_safe("os.api_level"):
            self.settings.os.api_level = 24

    def requirements(self):
        self.requires_tool("cmake")
        if self.settings.os == "Android":
            # Provides the NDK and the tools.android:ndk_path config the toolchain needs.
            self.requires_tool("android-ndk")

    def source(self):
        get(
            self,
            url="https://github.com/o3de/PhysX/archive/refs/heads/release/104.1.tar.gz",
            sha256="e488ad0ae2447d2394ce95f96770cdb4ce49c07b4c3da903117a9fe2e08b2839",
            destination=self.folders.source,
            strip_root=True)

        # CMakeModules is the only mandatory build-time external (clangMetadata, vswhere,
        # freeglut, opengl, rapidjson are for tooling/snippets/PVD/metadata-gen, all disabled).
        get(self, url=f"{_PACKMAN_CDN}/{_CMAKEMODULES[0]}", sha256=_CMAKEMODULES[1],
            destination=self.folders.source / "externals" / "CMakeModules")

        # Prebuilt GPU runtime libs, shipped to match the legacy package.
        if self._ships_physxgpu():
            get(self, url=f"{_PACKMAN_CDN}/{_PHYSXGPU[0]}", sha256=_PHYSXGPU[1],
                destination=self.folders.source / "externals" / "PhysXGpu")
        if self._ships_physxdevice():
            get(self, url=f"{_PACKMAN_CDN}/{_PHYSXDEVICE[0]}", sha256=_PHYSXDEVICE[1],
                destination=self.folders.source / "externals" / "PhysXDevice")

        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        # PhysX's per-config names (debug/checked/profile/release) are non-standard CMake
        # configs. A multi-config generator lets us select them via --config at build time;
        # with a single-config generator the framework would force CMAKE_BUILD_TYPE to the
        # standard settings.build_type and "checked"/"profile" could not be selected.
        tc.generator = "Ninja Multi-Config"

        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.pic

        # physx/compiler/public/CMakeLists.txt
        tc.cache_variables["TARGET_BUILD_PLATFORM"] = self._target_build_platform()
        tc.cache_variables["PHYSX_ROOT_DIR"] = (self.folders.source / "physx").as_posix()
        tc.cache_variables["PX_BUILDSNIPPETS"] = False
        tc.cache_variables["PX_BUILDPVDRUNTIME"] = False
        tc.cache_variables["PX_CMAKE_SUPPRESS_REGENERATION"] = False

        # Point CMake at the directly-downloaded CMakeModules. CMAKEMODULES_VERSION must be set,
        # otherwise public/CMakeLists.txt overwrites CMAKEMODULES_PATH with the (empty) packman
        # PM_CMakeModules_PATH environment variable.
        cmakemodules = self.folders.source / "externals" / "CMakeModules"
        tc.cache_variables["CMAKEMODULES_PATH"] = cmakemodules.as_posix()
        tc.cache_variables["CMAKEMODULES_NAME"] = "CMakeModules"
        tc.cache_variables["CMAKEMODULES_VERSION"] = "1.28"

        # GameWorks output layout: <PX_OUTPUT_LIB_DIR>/bin/<platform>/<config>/<libs>
        px_output = (self.folders.build / "px_output").as_posix()
        tc.cache_variables["PX_OUTPUT_LIB_DIR"] = px_output
        tc.cache_variables["PX_OUTPUT_BIN_DIR"] = px_output

        # externals/CMakeModules/NvidiaBuildOptions.cmake
        tc.cache_variables["NV_USE_GAMEWORKS_OUTPUT_DIRS"] = True
        tc.cache_variables["NV_APPEND_CONFIG_NAME"] = False
        tc.cache_variables["NV_FORCE_64BIT_SUFFIX"] = False
        tc.cache_variables["NV_FORCE_32BIT_SUFFIX"] = False
        # PX_OUTPUT_ARCH must be DEFINED to get the "_64" bitness suffix on the lib names; on mac
        # it also selects the build architecture (arm64 vs x86_64).
        tc.cache_variables["PX_OUTPUT_ARCH"] = "arm" if self.settings.arch == "ARM" else "x86"
        if is_msvc(self):
            tc.cache_variables["NV_USE_STATIC_WINCRT"] = "MT" in msvc_runtime_flag(self)
            tc.cache_variables["NV_USE_DEBUG_WINCRT"] = "d" in msvc_runtime_flag(self)

        # physx/source/compiler/cmake/CMakeLists.txt
        if self.settings.os in ["Windows", "Android"]:
            tc.cache_variables["PX_SCALAR_MATH"] = not self.options.enable_simd
        tc.cache_variables["PX_GENERATE_STATIC_LIBRARIES"] = not self.options.shared
        tc.cache_variables["PX_GENERATE_SOURCE_DISTRO"] = False

        if self.settings.os == "Android":
            # Use the modern NDK toolchain. The legacy one calls enable_language() while
            # including the NDK toolchain file, before CMake resolves the Ninja make
            # program, which breaks configure with the multi-config Ninja generator.
            tc.cache_variables["ANDROID_USE_LEGACY_TOOLCHAIN_FILE"] = False

        if self.settings.os == "Windows":
            # GPU projects are disabled; we ship the prebuilt PhysXGpu/PhysXDevice blobs instead.
            tc.cache_variables["PX_COPY_EXTERNAL_DLL"] = False
            tc.cache_variables["PX_FLOAT_POINT_PRECISE_MATH"] = self.options.enable_float_point_precise_math
            tc.cache_variables["PX_USE_NVTX"] = False
            tc.cache_variables["GPU_DLL_COPIED"] = True
            tc.cache_variables["PX_GENERATE_GPU_PROJECTS"] = False

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.folders.source / "physx" / "compiler" / "public")
        cmake.build(build_type=self._physx_build_type())

    def package(self):
        cmake = CMake(self)
        # PhysX has no INSTALL(TARGETS); the install target only copies the public headers
        # (and the generated PxConfig.h) into <package>/include.
        cmake.install(build_type=self._physx_build_type())

        save(self, self.folders.package / "licenses" / "LICENSE.md",
             load(self, self.folders.source / "LICENSE.md"))

        # Collect the built libraries from the GameWorks output tree (only the config we built
        # is present, so a recursive copy picks up exactly that config).
        lib_src = self.folders.build / "px_output" / "bin"
        lib_dst = self.folders.package / "lib"
        bin_dst = self.folders.package / "bin"
        for pattern in ("*.a", "*.lib", "*.so", "*.so.*", "*.dylib*"):
            copy(self, pattern=pattern, src=lib_src, dst=lib_dst, keep_path=False)
        copy(self, pattern="*.dll", src=lib_src, dst=bin_dst, keep_path=False)

        self._copy_gpu_libs()

    def package_info(self):
        self.info.set_property("cmake_file_name", "PhysX")

        # The static libraries are compiled with PX_PHYSX_STATIC_LIB, but PhysX only writes that
        # define into the public PxConfig.h on Windows. Expose it to consumers on every platform
        # so header linkage matches the libraries (matches the legacy FindPhysX5.cmake).
        if not self.options.shared:
            self.info.defines = ["PX_PHYSX_STATIC_LIB"]

        def component(key, target, libs, requires=None, system_libs=None):
            comp = self.info.components[key]
            comp.set_property("cmake_target_name", f"PhysX::{target}")
            comp.libs = [self._lib(lib) for lib in libs]
            if requires:
                comp.requires = requires
            if system_libs:
                comp.system_libs = system_libs

        foundation_syslibs = None
        if self.settings.os == "Linux":
            foundation_syslibs = ["m", "pthread", "rt"]
        elif self.settings.os == "Android":
            foundation_syslibs = ["log"]
        elif self.settings.os == "Windows":
            foundation_syslibs = ["ws2_32"]
        component("physxfoundation", "PhysXFoundation", ["PhysXFoundation"], system_libs=foundation_syslibs)

        component("physxcommon", "PhysXCommon", ["PhysXCommon"], requires=["physxfoundation"],
                  system_libs=["m"] if self.settings.os == "Linux" else None)
        component("physxpvdsdk", "PhysXPvdSDK", ["PhysXPvdSDK"], requires=["physxfoundation"])

        physxmain_syslibs = None
        if self.settings.os == "Linux":
            physxmain_syslibs = ["m", "dl"] if self.settings.arch == "X64" else ["m"]
        component("physxmain", "PhysX", ["PhysX"],
                  requires=["physxpvdsdk", "physxcommon", "physxfoundation"], system_libs=physxmain_syslibs)

        component("physxcooking", "PhysXCooking", ["PhysXCooking"],
                  requires=["physxfoundation", "physxcommon"],
                  system_libs=["m"] if self.settings.os == "Linux" else None)
        component("physxextensions", "PhysXExtensions", ["PhysXExtensions"],
                  requires=["physxfoundation", "physxpvdsdk", "physxmain", "physxcommon"])
        component("physxcharacterkinematic", "PhysXCharacterKinematic", ["PhysXCharacterKinematic"],
                  requires=["physxfoundation", "physxcommon", "physxextensions"])
        component("physxvehicle", "PhysXVehicle", ["PhysXVehicle"],
                  requires=["physxfoundation", "physxpvdsdk", "physxextensions"])
        component("physxvehicle2", "PhysXVehicle2", ["PhysXVehicle2"],
                  requires=["physxfoundation", "physxpvdsdk", "physxextensions"])

        # On Windows the internal helper libs are separate static archives that must also be
        # linked (elsewhere they are merged into the public libraries). Attach them to the main
        # PhysX component so consumers pick them up transitively.
        if not self.options.shared and self.settings.os == "Windows":
            for target in _PHYSX_INTERNAL_STATIC:
                key = target.lower()
                comp = self.info.components[key]
                comp.set_property("cmake_target_name", f"PhysX::{target}")
                comp.libs = [self._lib(target)]
                comp.requires = ["physxfoundation"]
                self.info.components["physxmain"].requires.append(key)

    # ----- helpers -------------------------------------------------------------------------

    def _lib(self, base: str) -> str:
        # Static libs: <Name>_static_64 ; shared import/runtime libs: <Name>_64
        return f"{base}_static_64" if not self.options.shared else f"{base}_64"

    def _patch_sources(self):
        cmake_dir = self.folders.source / "physx" / "source" / "compiler" / "cmake"

        # Let the recipe toolchain control PIC via CMAKE_POSITION_INDEPENDENT_CODE instead of
        # PhysX forcing it on globally and per target.
        replace_in_file(self, cmake_dir / "CMakeLists.txt",
                        "SET(CMAKE_POSITION_INDEPENDENT_CODE ON)", "", strict=False)
        for target in _PHYSX_TARGETS:
            replace_in_file(
                self, cmake_dir / f"{target}.cmake",
                f"SET_TARGET_PROPERTIES({target} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)",
                "", strict=False)

        # Don't treat warnings as errors (modern toolchains emit far more warnings than PhysX's
        # 2022-era flag lists suppress; mac/ios additionally use -Weverything).
        replace_in_file(self, cmake_dir / "windows" / "CMakeLists.txt", "/WX ", "", strict=False)
        for cmake_os in ("linux", "mac", "android", "ios"):
            replace_in_file(self, cmake_dir / cmake_os / "CMakeLists.txt", "-Werror", "",
                            strict=False)

        # Public headers shouldn't force consumers to define exactly one of NDEBUG/_DEBUG.
        replace_in_file(
            self, self.folders.source / "physx" / "include" / "foundation" / "PxPreprocessor.h",
            "#error Exactly one of NDEBUG and _DEBUG needs to be defined!",
            "// #error Exactly one of NDEBUG and _DEBUG needs to be defined!", strict=False)

    def _copy_gpu_libs(self):
        # PhysXGpu / PhysXDevice are prebuilt; their packman archives only ship checked/profile/
        # release, so debug builds reuse the checked variant.
        gpu_config = {"debug": "checked"}.get(self._physx_build_type(), self._physx_build_type())

        if self._ships_physxgpu():
            gpu_bin = (self.folders.source / "externals" / "PhysXGpu" / "bin"
                       / self._gpu_platform_dir() / gpu_config)
            if self.settings.os == "Windows":
                copy(self, pattern="PhysXGpu*.dll", src=gpu_bin,
                     dst=self.folders.package / "bin", keep_path=False)
            else:
                copy(self, pattern="*PhysXGpu*.so", src=gpu_bin,
                     dst=self.folders.package / "lib", keep_path=False)

        if self._ships_physxdevice():
            device_bin = self.folders.source / "externals" / "PhysXDevice" / "bin" / "x86"
            copy(self, pattern="PhysXDevice64.dll", src=device_bin,
                 dst=self.folders.package / "bin", keep_path=False)

    def _ships_physxgpu(self) -> bool:
        if self.settings.os == "Linux":
            return self.settings.arch in ("X64", "ARM")
        return self.settings.os == "Windows" and self.settings.arch == "X64"

    def _ships_physxdevice(self) -> bool:
        return self.settings.os == "Windows" and self.settings.arch == "X64"

    def _gpu_platform_dir(self) -> str:
        if self.settings.os == "Windows":
            return "win.x86_64.vc141.mt"
        return "linux.aarch64" if self.settings.arch == "ARM" else "linux.clang"

    def _physx_build_type(self) -> str:
        if self.settings.build_type == "Debug":
            return "debug"
        if self.settings.build_type == "RelWithDebInfo":
            return "checked"
        if self.settings.build_type == "Release":
            return "profile" if self.options.release_build_type == "profile" else "release"
        return "release"

    def _target_build_platform(self) -> str | None:
        return {
            "Windows": "windows",
            "Linux": "linux",
            "Mac": "mac",
            "Android": "android",
            "iOS": "ios",
        }.get(str(self.settings.os))
