#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)
    return()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)

ly_add_target(
    NAME ShaderCompiler STATIC
    NAMESPACE AZ
    NO_UNITY
    FILES_CMAKE
        shadercompiler_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
            Source/External
            Source/Grammar
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            3rdParty::ANTLR4
)

target_compile_options(ShaderCompiler PRIVATE
    ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated-enum-compare>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated-this-capture>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-range-loop-construct>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-function>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-lambda-capture>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-variable>
)

ly_add_target(
    NAME azslc EXECUTABLE
    NAMESPACE AZ
    NO_UNITY
    FILES_CMAKE
        azslc_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
            Source/External
            Source/Grammar
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::ShaderCompiler
            AZ::AzCore
            3rdParty::ANTLR4
)

target_compile_options(azslc PRIVATE
    ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated-declarations>
)

set_source_files_properties(
    Source/Grammar/azslLexer.cpp
    Source/Grammar/azslParser.cpp
    Source/Grammar/azslParserBaseListener.cpp
    Source/Grammar/azslParserListener.cpp
    PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>"
)
