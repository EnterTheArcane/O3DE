from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get
from thirdparty.scm import Version, WebReleaseIndex

import os


_REDIST_BASE = "https://developer.download.nvidia.com/compute/cuda/redist/"

_REDIST_PLATFORMS = {
    ("Windows", "X64"): ("windows-x86_64", "zip"),
    ("Linux", "X64"): ("linux-x86_64", "tar.xz"),
    ("Linux", "ARM"): ("linux-sbsa", "tar.xz"),
}

_COMPONENTS = {
    "cuda_nvcc": ("13.3.33", {
        ("Windows", "X64"): "8fed1ab69ed4e637ad76baff572579630674df9ff02570777800782ee5bdfbc5",
        ("Linux", "X64"): "93b098bda4a562ebf3541523ce82adc43f106a81dcf28bcbf8f0d8e093d1c66f",
        ("Linux", "ARM"): "b5dde44aadd52234af3944ae3b2e74e811ad8e71fb600bcc9dfe6d8540353499",
    }),
    "cuda_crt": ("13.3.33", {
        ("Windows", "X64"): "752c528281a06a0ddf89237d760ffd6acde1b9cd59efc35803c2591127ef55f0",
        ("Linux", "X64"): "4755d36d24c6ef7697a2d3e1dbb23c4562c9c0d97d48390d4cbd8ab32dec5b5f",
        ("Linux", "ARM"): "6f6194918c00b980d8fd2111bf0aa004977760855c6e1528e0653bf4c889fbef",
    }),
    "cuda_cudart": ("13.3.29", {
        ("Windows", "X64"): "1feb7dd266813ffe8dbc24e115183a5ac35a4795c8d34aca0df85ab616b64d9c",
        ("Linux", "X64"): "1e59c4888267d27ba1a9bd0f3669a6439db1334a96e754cd9013c7c73e18dc9d",
        ("Linux", "ARM"): "0cdd73d11885062daf3aa98ad4d7b8bd84f89b398be11f7054edea9ed31f597d",
    }),
    "cccl": ("13.3.3.3.1", {
        ("Windows", "X64"): "607dcfca31da168171fbdae5b7096ade646c4c2b1e0ff2899077dde0ccbdd6fb",
        ("Linux", "X64"): "67746da12f16229ac4ebde78ce7895e42b069d1d3e2ae2d2d25f90bc43679d68",
        ("Linux", "ARM"): "37e9024c5e24a9e9d1618c4fb7b36e74a0a68fac91d589867676952204ecde5b",
    }),
    "cuda_nvrtc": ("13.3.33", {
        ("Windows", "X64"): "8519f678588610bf380ccaac130729aa1a624c407183e7ad9c319c19ecc63d2f",
        ("Linux", "X64"): "9e8f78278215babd1236b137252424ca7912c185bd093201f5d97f7dd763b74a",
        ("Linux", "ARM"): "d0502b25799be62a50b743c640e94a1722d20b1ee4ab70d697d71750f04d3b8a",
    }),
    "libnvjitlink": ("13.3.33", {
        ("Windows", "X64"): "43bc22509507c138c86885191bb2709b5d23506ea6abdc8bc64d9960e2b63363",
        ("Linux", "X64"): "f79e25bb1ef2f22f26c09897f6cb8719634f38e7330bd4700a1ea9ec9591eaff",
        ("Linux", "ARM"): "6ed3a14646bd53e25ccf03a52586cdd12b07ad48cf81fe79deac49b5d64c2ce6",
    }),
    "libnvvm": ("13.3.33", {
        ("Windows", "X64"): "e8e48fcceb3ffeb3e421f29fc40252580c6dfd2a841bea3490782233048a5f00",
        ("Linux", "X64"): "fc9c1fd5844e44c0e5eeb051378c1b13cf0e3bb3fe4966d5103c38885424f802",
        ("Linux", "ARM"): "5f8ca5c9a10c3c9804b045960ee6192281efec4c7d83d5f3245ec2de8612118e",
    }),
}

# Subdirectories of a merged CUDA toolkit tree we package (those that exist for the platform).
_TOOLKIT_DIRS = ("bin", "include", "lib", "lib64", "nvvm", "targets")


class Recipe(RecipeBase):
    name = "cuda-toolkit"
    version = "13.3.0"
    license = "NVIDIA CUDA Toolkit EULA"

    def latest_version(self):
        index = WebReleaseIndex(self, "https://developer.nvidia.com/cuda-toolkit-archive")
        pattern = r"Latest Release[\s\S]*?CUDA Toolkit ([\d.]+)"
        return Version(index.latest_release(pattern))
    
    def validate(self):
        if self._platform_key not in _REDIST_PLATFORMS:
            raise RecipeInvalidConfiguration("Unsupported platform")

    def build(self):
        # Each component archive has a single <component>-<plat>-<ver>-archive/ root; strip_root drops
        # it so the bin/ include/ lib/ nvvm/ trees from every component merge into one toolkit root.
        for relpath, sha256 in _component_archives(self._platform_key):
            get(self, url=_REDIST_BASE + relpath, sha256=sha256,
                destination=self.folders.build, strip_root=True)

    def package(self):
        for subdir in _TOOLKIT_DIRS:
            src = self.folders.build / subdir
            if src.is_dir():
                copy(self, "*", src=src, dst=self.folders.package / subdir)
        # Each redist archive ships a LICENSE at its root (identical CUDA EULA across components).
        copy(self, "LICENSE", src=self.folders.build, dst=self.folders.package / "licenses")

        # CUDA 13's Linux redist ships libraries under lib/ (and lib/stubs/), but nvcc's profile
        # still searches lib64/ and lib64/stubs/ on 64-bit Linux. Add a lib64 -> lib symlink so
        # nvcc (and CMake's CUDA compiler detection) find libcudart_static/libcudadevrt/libcuda.
        if self.settings.os in ("Linux", "FreeBSD"):
            lib_dir = self.folders.package / "lib"
            lib64_dir = self.folders.package / "lib64"
            if lib_dir.is_dir() and not lib64_dir.exists():
                os.symlink("lib", lib64_dir)

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

    @property
    def _platform_key(self) -> tuple[str, str]:
        return str(self.settings.os), str(self.settings.arch)


def _component_archives(platform_key: tuple[str, str]):
    platform, extension = _REDIST_PLATFORMS[platform_key]
    for component, (version, hashes) in _COMPONENTS.items():
        filename = f"{component}-{platform}-{version}-archive.{extension}"
        yield f"{component}/{platform}/{filename}", hashes[platform_key]
