__version__ = "0.1.0"
CONAN_COMPAT_VERSION = "2.27.1"

from thirdparty.internal.model.version import Version
from thirdparty.internal.model.conan_file import ConanFile as RecipeBase
from thirdparty.internal.model.cpp_info import CppInfo

conan_version = Version(CONAN_COMPAT_VERSION)

__all__ = [
    "__version__",
    "CONAN_COMPAT_VERSION",
    "conan_version",
    "RecipeBase",
    "CppInfo",
]

