#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_target(
    NAME mcpp STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Source
    COMPILE_DEFINITIONS PRIVATE HAVE_CONFIG_H=1 MCPP_LIB=1
    TARGET_PROPERTIES C_STANDARD 90 C_STANDARD_REQUIRED YES
)

target_compile_options(mcpp PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(mcpp PRIVATE -Wno-implicit-function-declaration)
endif()

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
