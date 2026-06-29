from typing import Literal

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMakeToolchain, CMake
from thirdparty.files import load, save, get, copy, replace_in_file
from thirdparty.microsoft import msvc_runtime_flag, is_msvc


# Root PhysX library .cmake files (under physx/source/compiler/cmake). Used to strip PhysX's
# hardcoded per-target PIC so the recipe toolchain controls CMAKE_POSITION_INDEPENDENT_CODE.
# Note: PhysXVehicle.cmake builds the PhysXVehicle2 target, and FastXml only exists in the
# per-platform subdirs (mac/ios/android), so neither "PhysXVehicle2" nor "FastXml" is a root file.
_PHYSX_TARGETS = (
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
    "SceneQuery",
    "SimulationController",
)


class _Options(RecipeOptions):
    shared: bool = False
    pic: bool = True
    release_build_type: Literal["profile", "release"] = "release"
    enable_simd: bool = True
    enable_float_point_precise_math: bool = False
    # Build the CUDA GPU-acceleration modules (PhysXGpu). Only supported on Windows/Linux x64 and
    # Linux aarch64; auto-disabled elsewhere. Pulls in the cuda-toolkit build tool. On Windows the
    # active MSVC toolset must be one CUDA 12.9 supports (VS 2017-2022, i.e. <= 19.4x); a newer
    # toolset (e.g. VS18/19.5x) makes nvcc's cudafe++ frontend fail.
    gpu: bool = False
    # Compile a reduced GPU architecture set (sm_80+) for faster builds; off = full upstream set
    # (adds Volta sm_70). Only relevant when gpu is enabled.
    gpu_reduced_architectures: bool = True


class Recipe(RecipeBase[_Options]):
    name = "physx"
    version = "5.6.1"
    license = "BSD-3-Clause"

    def configure(self):
        if self.settings.os != "Windows":
            self.options.enable_float_point_precise_math = False
        if self.settings.os not in ["Windows", "Android"]:
            self.options.enable_simd = False
        # Android needs an API level; default it when the profile didn't supply one.
        if self.settings.os == "Android" and not self.settings.get_safe("os.api_level"):
            self.settings.os.api_level = 24
        # GPU acceleration only builds on Windows/Linux x64 and Linux aarch64 (PhysX disables CUDA
        # everywhere else). Silently fall back to CPU-only so a single cross-platform profile works.
        if self.options.gpu and not self._gpu_supported():
            self.output.warning(
                f"physx: gpu acceleration is not supported on {self.settings.os}/{self.settings.arch}; "
                "building CPU-only.")
            self.options.gpu = False

    def requirements(self):
        self.requires_tool("cmake")
        if self.settings.os == "Android":
            # Provides the NDK and the tools.android:ndk_path config the toolchain needs.
            self.requires_tool("android-ndk")
        if self._gpu_enabled():
            # Provides nvcc + the tools.cuda:root/nvcc config consumed in generate().
            self.requires_tool("cuda-toolkit")

    def source(self):
        get(
            self,
            url="https://github.com/o3de/PhysX/archive/0af1ce283240f8618a94456b6b819f97724cf6b7.tar.gz",
            sha256="3c0c89f8cb6210f623b2ac46ae2ff95101817209511a3190581feb0b4499d809",
            destination=self.folders.source,
            strip_root=True)

        # PhysX 5.6.1 embeds its CMake helper modules in-tree (physx/source/compiler/cmake/modules),
        # so unlike 5.1.1 there are no packman/CDN externals to download. Everything else listed in
        # physx/dependencies.xml (clangMetadata, freeglut, vswhere, opengl, rapidjson) is only used
        # by metadata-generation / snippets / tooling, all of which are disabled in this build.

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
        # GPU acceleration is opt-in in 5.6.1. When off (default), GPU projects aren't generated and
        # windows/CMakeLists.txt auto-defines DISABLE_CUDA_PHYSX (PX_SUPPORT_GPU_PHYSX=0) -- pure
        # CPU-only, matching mac/android, with nothing to fetch. When on, PhysX compiles PhysXGpu
        # from source using the cuda-toolkit we provide.
        tc.cache_variables["PX_GENERATE_GPU_PROJECTS"] = self._gpu_enabled()
        if self._gpu_enabled():
            # Point CMake's CUDA language + FindCUDAToolkit explicitly at our cuda-toolkit. Setting
            # these as cache variables (-D) is deterministic: it overrides any system-installed CUDA
            # that env-based discovery might otherwise pick up.
            cuda_root = self.dependencies.build["cuda-toolkit"].folders.package
            nvcc = cuda_root / "bin" / ("nvcc.exe" if self.settings.os == "Windows" else "nvcc")
            tc.cache_variables["CMAKE_CUDA_COMPILER"] = nvcc.as_posix()
            tc.cache_variables["CUDAToolkit_ROOT"] = cuda_root.as_posix()
            # Our pinned CUDA 12.9 nvcc may not recognize a very new host MSVC/GCC; don't let the
            # host-compiler version check abort configure.
            tc.cache_variables["CMAKE_CUDA_FLAGS"] = "-allow-unsupported-compiler"
            tc.cache_variables["PX_GENERATE_GPU_REDUCED_ARCHITECTURES"] = self.options.gpu_reduced_architectures

        # GameWorks output layout: <PX_OUTPUT_LIB_DIR>/bin/<platform>/<config>/<libs>. In 5.6.1 this
        # is the default structure (no NV_USE_GAMEWORKS_OUTPUT_DIRS toggle); the base dirs are
        # mandatory (NvidiaBuildOptions.cmake FATAL_ERRORs without them).
        px_output = (self.folders.build / "px_output").as_posix()
        tc.cache_variables["PX_OUTPUT_LIB_DIR"] = px_output
        tc.cache_variables["PX_OUTPUT_BIN_DIR"] = px_output

        # physx/source/compiler/cmake/modules/NvidiaBuildOptions.cmake
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
            tc.cache_variables["PX_FLOAT_POINT_PRECISE_MATH"] = self.options.enable_float_point_precise_math
            tc.cache_variables["PX_USE_NVTX"] = False

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

    def package_info(self):
        self.info.set_property("cmake_file_name", "PhysX")

        # In a CPU-only build the libs are compiled with DISABLE_CUDA_PHYSX (PX_SUPPORT_GPU_PHYSX=0);
        # on Win/Linux x64 the public headers would otherwise default that to 1, so propagate it to
        # consumers to keep the header/lib ABI consistent (mac/ios/android define it by platform). In
        # a GPU build PhysX leaves it undefined (PX_SUPPORT_GPU_PHYSX=1), so we must not define it.
        self.info.defines = [] if self._gpu_enabled() else ["DISABLE_CUDA_PHYSX"]

        # The static libraries are compiled with PX_PHYSX_STATIC_LIB, but PhysX only writes that
        # define into the public PxConfig.h on Windows. Expose it to consumers on every platform
        # so header linkage matches the libraries (matches the legacy FindPhysX5.cmake).
        if not self.options.shared:
            self.info.defines.append("PX_PHYSX_STATIC_LIB")

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
        component("physxvehicle2", "PhysXVehicle2", ["PhysXVehicle2"],
                  requires=["physxfoundation", "physxpvdsdk", "physxextensions"])

    # ----- helpers -------------------------------------------------------------------------

    def _gpu_supported(self) -> bool:
        # PhysX builds CUDA GPU projects on Windows/Linux x64 and Linux aarch64 only.
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        return (os_name == "Windows" and arch == "X64") or \
               (os_name == "Linux" and arch in ("X64", "ARM"))

    def _gpu_enabled(self) -> bool:
        return self.options.gpu and self._gpu_supported()

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
            # PhysXVehicle.cmake builds the PhysXVehicle2 target; otherwise file stem == target.
            lib = "PhysXVehicle2" if target == "PhysXVehicle" else target
            replace_in_file(
                self, cmake_dir / f"{target}.cmake",
                f"SET_TARGET_PROPERTIES({lib} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)",
                "", strict=False)

        # The windows build copies PhysXDevice + freeglut DLLs from the packman-provided
        # PM_PhysXDevice_PATH / PM_freeglut_PATH env vars. public/CMakeLists.txt forces
        # PUBLIC_RELEASE=1, so this copy fires regardless of our PX_COPY_EXTERNAL_DLL default. The
        # PhysXDevice copy is now guarded by IF(NOT PUBLIC_RELEASE) and skipped, but the freeglut
        # copy still runs and PM_freeglut_PATH is unset (we don't build snippets), collapsing the
        # path to "/bin//win64/freeglut.dll" and failing configure. Drop the PUBLIC_RELEASE force so
        # the block honors PX_COPY_EXTERNAL_DLL (left at its default-off; freeglut isn't needed).
        replace_in_file(self, cmake_dir / "windows" / "CMakeLists.txt",
                        "IF(PX_COPY_EXTERNAL_DLL OR PUBLIC_RELEASE)",
                        "IF(PX_COPY_EXTERNAL_DLL)", strict=False)

        # Don't treat warnings as errors (modern toolchains emit far more warnings than PhysX's
        # flag lists suppress; mac/ios additionally use -Weverything).
        replace_in_file(self, cmake_dir / "windows" / "CMakeLists.txt", "/WX ", "", strict=False)
        for cmake_os in ("linux", "mac", "android", "ios"):
            replace_in_file(self, cmake_dir / cmake_os / "CMakeLists.txt", "-Werror", "",
                            strict=False)

        # Public headers shouldn't force consumers to define exactly one of NDEBUG/_DEBUG.
        replace_in_file(
            self, self.folders.source / "physx" / "include" / "foundation" / "PxPreprocessor.h",
            "#error Exactly one of NDEBUG and _DEBUG needs to be defined!",
            "// #error Exactly one of NDEBUG and _DEBUG needs to be defined!", strict=False)

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
