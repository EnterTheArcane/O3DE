#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(zstd_system_libraries)
if(UNIX)
    find_package(Threads REQUIRED)
    if(CMAKE_THREAD_LIBS_INIT)
        list(APPEND zstd_system_libraries PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    endif()
endif()

ly_add_target(
    NAME zstd STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Source
    COMPILE_DEFINITIONS
        PRIVATE
            ZSTD_DISABLE_ASM
            ZSTD_LEGACY_SUPPORT=5
            ZSTD_MULTITHREAD
            $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    BUILD_DEPENDENCIES
        ${zstd_system_libraries}
        PRIVATE
            3rdParty::xxhash
)

target_compile_options(zstd PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

ly_get_engine_relative_source_dir(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    third_party_install_destination
)

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
        README.md
    DESTINATION
        "${third_party_install_destination}"
)
