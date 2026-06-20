from thirdparty.microsoft.layout import vs_layout
from thirdparty.microsoft.msbuild import MSBuild
from thirdparty.microsoft.msbuilddeps import MSBuildDeps
from thirdparty.microsoft.subsystems import unix_path, unix_path_package_info_legacy
from thirdparty.microsoft.toolchain import MSBuildToolchain
from thirdparty.microsoft.nmaketoolchain import NMakeToolchain
from thirdparty.microsoft.nmakedeps import NMakeDeps
from thirdparty.microsoft.visual import msvc_runtime_flag, VCVars, is_msvc, \
    is_msvc_static_runtime, check_min_vs, msvs_toolset
