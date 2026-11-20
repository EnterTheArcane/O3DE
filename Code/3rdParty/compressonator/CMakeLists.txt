#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(compressonator
    VERSION "4.6.0"
    LICENSE "MIT and bundled third-party licenses"
    URL "https://github.com/GPUOpen-Tools/compressonator/archive/095a143ad8cfbf9ed5afa4ea63c735bfd0fd04dd.tar.gz"
    URL_HASH "182362fb92059a61d2d21a7899caee7788b88eb6e3743d16bf9a525678e8e2ff"
    GIT "https://github.com/GPUOpen-Tools/compressonator.git"
    GIT_HASH "095a143ad8cfbf9ed5afa4ea63c735bfd0fd04dd"
    PATCH_FILES "${CMAKE_CURRENT_LIST_DIR}/compressonator.patch"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(compressonator)

file(COPY
    "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/compressonator.h"
    "${compressonator_SOURCE_DIR}/cmp_core/source/cmp_core.h"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

o3de_disable_warnings()

ly_add_target(
    NAME compressonator STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            "${CMAKE_CURRENT_BINARY_DIR}/Include"
        PRIVATE
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/astc/arm"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/buffer"
            "${compressonator_SOURCE_DIR}/cmp_framework/common"
            "${compressonator_SOURCE_DIR}/cmp_framework/common/half"
            "${compressonator_SOURCE_DIR}/applications/_plugins/common"
            "${compressonator_SOURCE_DIR}/cmp_core/shaders"
            "${compressonator_SOURCE_DIR}/cmp_core/source"
            "${compressonator_SOURCE_DIR}/applications/_libs/cmp_math"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/apc"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/astc"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/atc"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/ati"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/basis"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/bc6h"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/bc7"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/block"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/common"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/dxt"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/dxtc"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/etc"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/etc/etcpack"
            "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/gt"
            "${compressonator_SOURCE_DIR}/cmp_framework"
            "${compressonator_SOURCE_DIR}/applications/_plugins/ccmp_sdk"
            "${compressonator_SOURCE_DIR}/applications/_libs/gpu_decode"
    COMPILE_DEFINITIONS
        PRIVATE
            USE_ETCPACK
            ENABLE_MAKE_COMPATIBLE_API
            USE_OLD_SWIZZLE
            $<$<PLATFORM_ID:Linux>:_LINUX>
            $<$<PLATFORM_ID:Windows>:NOMINMAX;_CRT_SECURE_NO_WARNINGS>
    BUILD_DEPENDENCIES
        PRIVATE
            3rdParty::stb
            3rdParty::OpenEXR
)

set_target_properties(compressonator PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SYSTEM TRUE
)
target_compile_features(compressonator PRIVATE cxx_std_17)
target_compile_options(compressonator PRIVATE
    ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-write-strings>
)

# Keep higher ISA instructions in dedicated translation units for runtime dispatch.
if(MSVC)
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_core/source/core_simd_avx.cpp" PROPERTIES
        COMPILE_OPTIONS /arch:AVX2
    )
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_core/source/core_simd_avx512.cpp" PROPERTIES
        COMPILE_OPTIONS /arch:AVX-512
    )
else()
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_core/source/core_simd_sse.cpp" PROPERTIES
        COMPILE_OPTIONS -march=nehalem
    )
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_core/source/core_simd_avx.cpp" PROPERTIES
        COMPILE_OPTIONS -march=haswell
    )
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_core/source/core_simd_avx512.cpp" PROPERTIES
        COMPILE_OPTIONS -march=knl
    )
endif()

find_package(Threads REQUIRED)
ly_target_link_libraries(compressonator PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

if(WIN32)
    # The upstream Direct3D sample uses MSVC temporary addresses and traditional token pasting.
    set_source_files_properties("${compressonator_SOURCE_DIR}/cmp_compressonatorlib/brotlig/BrotligCompute.cpp" PROPERTIES
        COMPILE_OPTIONS "/permissive;/Zc:preprocessor-"
    )

    target_include_directories(compressonator PRIVATE "${compressonator_SOURCE_DIR}/cmp_compressonatorlib/brotlig")
    ly_target_link_libraries(compressonator PRIVATE
        3rdParty::brotlig
        3rdParty::directx-headers
        d3d12
        dxgi
        dxguid
        dxcompiler
        advapi32
        wintrust
        crypt32
    )

    ly_parse_third_party_dependencies(3rdParty::DirectXShaderCompilerDxc)
    get_target_property(cmp_dxc_files 3rdParty::DirectXShaderCompilerDxc INTERFACE_LY_TARGET_FILES)
    foreach(cmp_dxc_file IN LISTS cmp_dxc_files)
        if(cmp_dxc_file MATCHES "^([^\n]+/(dxcompiler|dxil)\\.dll)\n")
            ly_add_target_files(TARGETS compressonator FILES "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/compressonator
)
