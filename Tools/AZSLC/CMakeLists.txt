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

set(azslc_VERSION_MAJOR 1)
set(azslc_VERSION_MINOR 9)
set(azslc_VERSION_PATCH 0)
set(azslc_VERSION "${azslc_VERSION_MAJOR}.${azslc_VERSION_MINOR}.${azslc_VERSION_PATCH}")

add_subdirectory(3rdParty)

set(azslc_generated_files)
if(WIN32)
    configure_file(
        Platform/Windows/Source/azslc.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/azslc.rc
        @ONLY
    )
    list(APPEND azslc_generated_files ${CMAKE_CURRENT_BINARY_DIR}/azslc.rc)
endif()

ly_add_target(
    NAME AZSLC EXECUTABLE
    NAMESPACE AZ
    OUTPUT_NAME azslc
    NO_UNITY
    FILES_CMAKE
        azslc_files.cmake
    GENERATED_FILES
        ${azslc_generated_files}
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
            Source/Grammar
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            3rdParty::antlr4
            3rdParty::cli11
            3rdParty::jsoncpp
)

target_compile_options(AZSLC PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})

if(WIN32)
    target_compile_options(AZSLC PRIVATE
        -U_HAS_EXCEPTIONS
        -D_HAS_EXCEPTIONS=1
    )
endif()

if(NOT MSVC)
    target_compile_options(AZSLC PRIVATE
        -Wno-deprecated-literal-operator
        -Wno-language-extension-token
        -Wno-missing-braces
        -Wno-range-loop-construct
        -Wno-unused-lambda-capture
        -Wno-unused-function
        -Wno-unused-variable
    )
endif()

add_subdirectory(Tests)
