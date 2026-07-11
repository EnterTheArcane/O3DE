#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)
    return()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)

ly_add_target(
    NAME AZSLC EXECUTABLE
    NAMESPACE AZ
    OUTPUT_NAME azslc
    FILES_CMAKE
        azslc_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            3rdParty::ANTLR4
            3rdParty::CLI11
)

target_compile_options(AZSLC PRIVATE
    ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
)

add_subdirectory(Tests)
