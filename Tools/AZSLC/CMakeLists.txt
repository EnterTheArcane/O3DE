#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if (NOT PAL_TRAIT_BUILD_HOST_TOOLS)
    return()
endif ()

add_subdirectory(3rdParty/antlr4)
add_subdirectory(3rdParty/cli11)

ly_add_target(
    NAME AZSLC EXECUTABLE
    NAMESPACE AZ
    OUTPUT_NAME azslc
    FILES_CMAKE
        azslc_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
            Source/Grammar
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            3rdParty::antlr4
            3rdParty::cli11
)

target_compile_options(AZSLC PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})

if (NOT MSVC)
    target_compile_options(AZSLC PRIVATE
        -Wno-deprecated-literal-operator
        -Wno-language-extension-token
        -Wno-missing-braces
        -Wno-range-loop-construct
        -Wno-unused-lambda-capture
        -Wno-unused-function
        -Wno-unused-variable
    )
endif ()

add_subdirectory(Tests)
