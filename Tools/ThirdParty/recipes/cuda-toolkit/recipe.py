from thirdparty import RecipeBase
from thirdparty.errors import RecipeException
from thirdparty.files import copy, get


# CUDA toolkit redistributable components, sourced directly from NVIDIA's redist CDN (the same
# per-component archives the CUDA installer assembles). We only pull the pieces needed to COMPILE
# and LINK CUDA code (nvcc toolchain + cudart + CCCL headers, plus nvrtc/nvjitlink that nvcc's
# device link step can require) -- this is a build tool, not a redistributable runtime.
#
# Pinned to 12.9.1, the latest CUDA 12.x. PhysX 5.6.1 targets CUDA 12.8 and still requires Volta
# (sm_70); CUDA 13 dropped sm_70 and reorganized the redist (no cuda_cccl), so it is not used here.
_REDIST_BASE = "https://developer.download.nvidia.com/compute/cuda/redist/"

# component -> platform key -> (archive relative path, sha256). Values are from redistrib_12.9.1.json.
_COMPONENTS = {
    "cuda_nvcc": {
        "windows-x86_64": ("cuda_nvcc/windows-x86_64/cuda_nvcc-windows-x86_64-12.9.86-archive.zip",
                           "227b109663b5e57d2718bcabb24a4ba0d9d4e52d958e327dc476f7c28691be85"),
        "linux-x86_64": ("cuda_nvcc/linux-x86_64/cuda_nvcc-linux-x86_64-12.9.86-archive.tar.xz",
                         "7a1a5b652e5ef85c82b721d10672fc9a2dbaab44e9bd3c65a69517bf53998c35"),
        "linux-sbsa": ("cuda_nvcc/linux-sbsa/cuda_nvcc-linux-sbsa-12.9.86-archive.tar.xz",
                       "0aa1fce92dbae76c059c27eefb9d0ffb58e1291151e44ff7c7f1fc2dd9376c0d"),
    },
    "cuda_cudart": {
        "windows-x86_64": ("cuda_cudart/windows-x86_64/cuda_cudart-windows-x86_64-12.9.79-archive.zip",
                           "179e9c43b0735ffe67207b3da556eb5a0c50f3047961882b7657d3b822d34ef8"),
        "linux-x86_64": ("cuda_cudart/linux-x86_64/cuda_cudart-linux-x86_64-12.9.79-archive.tar.xz",
                         "1f6ad42d4f530b24bfa35894ccf6b7209d2354f59101fd62ec4a6192a184ce99"),
        "linux-sbsa": ("cuda_cudart/linux-sbsa/cuda_cudart-linux-sbsa-12.9.79-archive.tar.xz",
                       "8b422a3b2cb8452cb678181b0bf9d7aa7342df168b5319c5488ae3b8514101fc"),
    },
    "cuda_cccl": {
        "windows-x86_64": ("cuda_cccl/windows-x86_64/cuda_cccl-windows-x86_64-12.9.27-archive.zip",
                           "17aaa7c6b8f94a417d8f3261780b7e34b9cbdfab7513bce86768623b06aa28b5"),
        "linux-x86_64": ("cuda_cccl/linux-x86_64/cuda_cccl-linux-x86_64-12.9.27-archive.tar.xz",
                         "8b1a5095669e94f2f9afd7715533314d418179e9452be61e2fde4c82a3e542aa"),
        "linux-sbsa": ("cuda_cccl/linux-sbsa/cuda_cccl-linux-sbsa-12.9.27-archive.tar.xz",
                       "8c3da24801b500f1d9217d191bb4b63e5d2096c8e7d0b7695e876853180ba82f"),
    },
    "cuda_nvrtc": {
        "windows-x86_64": ("cuda_nvrtc/windows-x86_64/cuda_nvrtc-windows-x86_64-12.9.86-archive.zip",
                           "1aa0644fa53c8ca34cdc73db17bcc73530557bdd3f582c7bfdbd7916c8b48f65"),
        "linux-x86_64": ("cuda_nvrtc/linux-x86_64/cuda_nvrtc-linux-x86_64-12.9.86-archive.tar.xz",
                         "82913658363892dbc0f2638b070476234476e06e084fed60db861cb7e161a6af"),
        "linux-sbsa": ("cuda_nvrtc/linux-sbsa/cuda_nvrtc-linux-sbsa-12.9.86-archive.tar.xz",
                       "fb2d50c791465f333fc2236d2419170cf7a7886f48dd9b967a10f8233c686029"),
    },
    "libnvjitlink": {
        "windows-x86_64": ("libnvjitlink/windows-x86_64/libnvjitlink-windows-x86_64-12.9.86-archive.zip",
                           "ee7175da9628d47ccc92dce6d28d57ca77633e79079a2aee90e2a645edcd1384"),
        "linux-x86_64": ("libnvjitlink/linux-x86_64/libnvjitlink-linux-x86_64-12.9.86-archive.tar.xz",
                         "392cac3144b52ba14900bc7259ea6405ae6da88a8c704eab9bbbcc9ba4824b07"),
        "linux-sbsa": ("libnvjitlink/linux-sbsa/libnvjitlink-linux-sbsa-12.9.86-archive.tar.xz",
                       "9c9227c1e9122fd8448cafced3b32bc69f40d3c041d25034ea23611a1262852f"),
    },
}

# Subdirectories of a merged CUDA toolkit tree we package (those that exist for the platform).
_TOOLKIT_DIRS = ("bin", "include", "lib", "lib64", "nvvm", "targets")


class Recipe(RecipeBase):
    name = "cuda-toolkit"
    version = "12.9.1"
    license = "NVIDIA CUDA Toolkit EULA"

    def build(self):
        plat = self._redist_platform()
        # Each component archive has a single <component>-<plat>-<ver>-archive/ root; strip_root drops
        # it so the bin/ include/ lib/ nvvm/ trees from every component merge into one toolkit root.
        for relpath, sha256 in (comp[plat] for comp in _COMPONENTS.values()):
            get(self, url=_REDIST_BASE + relpath, sha256=sha256,
                destination=self.folders.build, strip_root=True)

    def package(self):
        for subdir in _TOOLKIT_DIRS:
            src = self.folders.build / subdir
            if src.is_dir():
                copy(self, "*", src=src, dst=self.folders.package / subdir)
        # Each redist archive ships a LICENSE at its root (identical CUDA EULA across components).
        copy(self, "LICENSE", src=self.folders.build, dst=self.folders.package / "licenses")

    def package_info(self):
        # Build tool only: nothing for consumers to compile/link against directly.
        self.info.includedirs = []
        self.info.libdirs = []

        root = self.folders.package
        nvcc = root / "bin" / ("nvcc.exe" if self.settings.os == "Windows" else "nvcc")

        # Build-time discovery for consumers: nvcc on PATH plus the standard CUDA env vars. CMake's
        # ENABLE_LANGUAGE(CUDA) honors CUDACXX, and FindCUDAToolkit honors CUDAToolkit_ROOT/CUDA_PATH,
        # so a consumer just needs these in its build environment (composed in via requires_tool).
        self.info.buildenv.prepend_path("PATH", root / "bin")
        self.info.buildenv.define_path("CUDA_PATH", root)
        self.info.buildenv.define_path("CUDAToolkit_ROOT", root)
        self.info.buildenv.define_path("CUDACXX", nvcc)

    def _redist_platform(self) -> str:
        os_name = str(self.settings.os)
        arch = str(self.settings.arch)
        if os_name == "Windows" and arch == "X64":
            return "windows-x86_64"
        if os_name == "Linux" and arch == "X64":
            return "linux-x86_64"
        if os_name == "Linux" and arch == "ARM":
            return "linux-sbsa"
        raise RecipeException(
            f"cuda-toolkit is only available for Windows/Linux x86_64 and Linux aarch64, "
            f"not {os_name}/{arch}")
