#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(brotlig
    VERSION "1.1"
    LICENSE "MIT"
    URL "https://github.com/GPUOpen-LibrariesAndSDKs/brotli_g_sdk/archive/5a213b9609308ae652e6f2f3e58d036aea97584c.tar.gz"
    URL_HASH "e1f8dec05fed93c2dab8bcc7ca8f40a2dfbcba6533e81af5d383dacc8d9bcf52"
    GIT "https://github.com/GPUOpen-LibrariesAndSDKs/brotli_g_sdk.git"
    GIT_HASH "5a213b9609308ae652e6f2f3e58d036aea97584c"
    PATCH_FILES "${CMAKE_CURRENT_LIST_DIR}/brotlig.patch"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(brotlig)

file(
    COPY "${brotlig_SOURCE_DIR}/inc/"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include"
    FILES_MATCHING PATTERN "*.h"
)

o3de_disable_warnings()

ly_add_target(
    NAME brotlig STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            "${CMAKE_CURRENT_BINARY_DIR}/Include"
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/Include/common"
            "${CMAKE_CURRENT_BINARY_DIR}/Include/decoder"
            "${CMAKE_CURRENT_BINARY_DIR}/Include/encoder"
    COMPILE_DEFINITIONS
        PRIVATE
            $<$<PLATFORM_ID:Windows>:NOMINMAX;_CRT_SECURE_NO_WARNINGS>
    BUILD_DEPENDENCIES
        PUBLIC
            3rdParty::brotli
)

set_target_properties(brotlig PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SYSTEM TRUE
)
target_compile_features(brotlig PUBLIC cxx_std_17)
target_compile_options(brotlig PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})

find_package(Threads REQUIRED)
ly_target_link_libraries(brotlig PRIVATE Threads::Threads)

if(WIN32)
    ly_add_target_files(
        TARGETS brotlig
        OUTPUT_SUBDIRECTORY plugins/compute
        FILES "${brotlig_SOURCE_DIR}/src/decoder/BrotliGCompute.hlsl"
    )
endif()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/brotlig
)
