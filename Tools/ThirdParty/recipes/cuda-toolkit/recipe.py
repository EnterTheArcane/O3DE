from thirdparty import RecipeBase
from thirdparty.errors import RecipeException
from thirdparty.files import copy, get


# CUDA toolkit redistributable components, sourced directly from NVIDIA's redist CDN (the same
# per-component archives the CUDA installer assembles). We only pull the pieces needed to COMPILE
# and LINK CUDA code (nvcc toolchain + cudart + CCCL headers, plus nvrtc/nvjitlink that nvcc's
# device link step can require) -- this is a build tool, not a redistributable runtime.
#
# Pinned to 13.3.0, the latest CUDA. PhysX 5.6.1 targets the CUDA 12 API, but its GPU recipe applies
# small CUDA-13 source patches; CUDA 13 (unlike 12.x) supports very new MSVC toolsets. CUDA 13
# reorganized the redist: cuda_cccl became the standalone `cccl`, and the CRT headers / NVVM split
# out of cuda_nvcc into `cuda_crt` / `libnvvm`.
_REDIST_BASE = "https://developer.download.nvidia.com/compute/cuda/redist/"

# component -> platform key -> (archive relative path, sha256). Values are from redistrib_13.3.0.json.
_COMPONENTS = {
    "cuda_nvcc": {
        "windows-x86_64": ("cuda_nvcc/windows-x86_64/cuda_nvcc-windows-x86_64-13.3.33-archive.zip",
                           "8fed1ab69ed4e637ad76baff572579630674df9ff02570777800782ee5bdfbc5"),
        "linux-x86_64": ("cuda_nvcc/linux-x86_64/cuda_nvcc-linux-x86_64-13.3.33-archive.tar.xz",
                         "93b098bda4a562ebf3541523ce82adc43f106a81dcf28bcbf8f0d8e093d1c66f"),
        "linux-sbsa": ("cuda_nvcc/linux-sbsa/cuda_nvcc-linux-sbsa-13.3.33-archive.tar.xz",
                       "b5dde44aadd52234af3944ae3b2e74e811ad8e71fb600bcc9dfe6d8540353499"),
    },
    "cuda_crt": {
        "windows-x86_64": ("cuda_crt/windows-x86_64/cuda_crt-windows-x86_64-13.3.33-archive.zip",
                           "752c528281a06a0ddf89237d760ffd6acde1b9cd59efc35803c2591127ef55f0"),
        "linux-x86_64": ("cuda_crt/linux-x86_64/cuda_crt-linux-x86_64-13.3.33-archive.tar.xz",
                         "4755d36d24c6ef7697a2d3e1dbb23c4562c9c0d97d48390d4cbd8ab32dec5b5f"),
        "linux-sbsa": ("cuda_crt/linux-sbsa/cuda_crt-linux-sbsa-13.3.33-archive.tar.xz",
                       "6f6194918c00b980d8fd2111bf0aa004977760855c6e1528e0653bf4c889fbef"),
    },
    "cuda_cudart": {
        "windows-x86_64": ("cuda_cudart/windows-x86_64/cuda_cudart-windows-x86_64-13.3.29-archive.zip",
                           "1feb7dd266813ffe8dbc24e115183a5ac35a4795c8d34aca0df85ab616b64d9c"),
        "linux-x86_64": ("cuda_cudart/linux-x86_64/cuda_cudart-linux-x86_64-13.3.29-archive.tar.xz",
                         "1e59c4888267d27ba1a9bd0f3669a6439db1334a96e754cd9013c7c73e18dc9d"),
        "linux-sbsa": ("cuda_cudart/linux-sbsa/cuda_cudart-linux-sbsa-13.3.29-archive.tar.xz",
                       "0cdd73d11885062daf3aa98ad4d7b8bd84f89b398be11f7054edea9ed31f597d"),
    },
    "cccl": {
        "windows-x86_64": ("cccl/windows-x86_64/cccl-windows-x86_64-13.3.3.3.1-archive.zip",
                           "607dcfca31da168171fbdae5b7096ade646c4c2b1e0ff2899077dde0ccbdd6fb"),
        "linux-x86_64": ("cccl/linux-x86_64/cccl-linux-x86_64-13.3.3.3.1-archive.tar.xz",
                         "67746da12f16229ac4ebde78ce7895e42b069d1d3e2ae2d2d25f90bc43679d68"),
        "linux-sbsa": ("cccl/linux-sbsa/cccl-linux-sbsa-13.3.3.3.1-archive.tar.xz",
                       "37e9024c5e24a9e9d1618c4fb7b36e74a0a68fac91d589867676952204ecde5b"),
    },
    "cuda_nvrtc": {
        "windows-x86_64": ("cuda_nvrtc/windows-x86_64/cuda_nvrtc-windows-x86_64-13.3.33-archive.zip",
                           "8519f678588610bf380ccaac130729aa1a624c407183e7ad9c319c19ecc63d2f"),
        "linux-x86_64": ("cuda_nvrtc/linux-x86_64/cuda_nvrtc-linux-x86_64-13.3.33-archive.tar.xz",
                         "9e8f78278215babd1236b137252424ca7912c185bd093201f5d97f7dd763b74a"),
        "linux-sbsa": ("cuda_nvrtc/linux-sbsa/cuda_nvrtc-linux-sbsa-13.3.33-archive.tar.xz",
                       "d0502b25799be62a50b743c640e94a1722d20b1ee4ab70d697d71750f04d3b8a"),
    },
    "libnvjitlink": {
        "windows-x86_64": ("libnvjitlink/windows-x86_64/libnvjitlink-windows-x86_64-13.3.33-archive.zip",
                           "43bc22509507c138c86885191bb2709b5d23506ea6abdc8bc64d9960e2b63363"),
        "linux-x86_64": ("libnvjitlink/linux-x86_64/libnvjitlink-linux-x86_64-13.3.33-archive.tar.xz",
                         "f79e25bb1ef2f22f26c09897f6cb8719634f38e7330bd4700a1ea9ec9591eaff"),
        "linux-sbsa": ("libnvjitlink/linux-sbsa/libnvjitlink-linux-sbsa-13.3.33-archive.tar.xz",
                       "6ed3a14646bd53e25ccf03a52586cdd12b07ad48cf81fe79deac49b5d64c2ce6"),
    },
    "libnvvm": {
        "windows-x86_64": ("libnvvm/windows-x86_64/libnvvm-windows-x86_64-13.3.33-archive.zip",
                           "e8e48fcceb3ffeb3e421f29fc40252580c6dfd2a841bea3490782233048a5f00"),
        "linux-x86_64": ("libnvvm/linux-x86_64/libnvvm-linux-x86_64-13.3.33-archive.tar.xz",
                         "fc9c1fd5844e44c0e5eeb051378c1b13cf0e3bb3fe4966d5103c38885424f802"),
        "linux-sbsa": ("libnvvm/linux-sbsa/libnvvm-linux-sbsa-13.3.33-archive.tar.xz",
                       "5f8ca5c9a10c3c9804b045960ee6192281efec4c7d83d5f3245ec2de8612118e"),
    },
}

# Subdirectories of a merged CUDA toolkit tree we package (those that exist for the platform).
_TOOLKIT_DIRS = ("bin", "include", "lib", "lib64", "nvvm", "targets")


class Recipe(RecipeBase):
    name = "cuda-toolkit"
    version = "13.3.0"
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
