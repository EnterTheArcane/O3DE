#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)
    return()
endif()

ly_add_target(
    NAME ShaderCompiler STATIC
    NAMESPACE AZ
    FILES_CMAKE
        shadercompiler_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            3rdParty::antlr4
            3rdParty::jsoncpp
)

target_compile_options(ShaderCompiler PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    update_pip_requirements(${CMAKE_CURRENT_SOURCE_DIR}/Tests/requirements.txt shadercompiler_tests)

    ly_add_target(
        NAME ShaderCompiler.Tests EXECUTABLE
        NAMESPACE AZ
        FILES_CMAKE shadercompiler_tests_files.cmake
        INCLUDE_DIRECTORIES PRIVATE Source
        BUILD_DEPENDENCIES PRIVATE AZ::ShaderCompiler AZ::AzCore 3rdParty::antlr4
    )

    target_compile_options(ShaderCompiler.Tests PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})

    ly_add_test(NAME AZ::ShaderCompiler.Tests TEST_LIBRARY standalone TEST_COMMAND $<TARGET_FILE:ShaderCompiler.Tests>)

    ly_add_target(
        NAME ShaderCompiler.PreprocessingBenchmark EXECUTABLE
        NAMESPACE AZ
        FILES_CMAKE shadercompiler_benchmark_files.cmake
        INCLUDE_DIRECTORIES PRIVATE Source
        BUILD_DEPENDENCIES PRIVATE AZ::ShaderCompiler AZ::AzCore 3rdParty::antlr4
    )

    target_compile_options(ShaderCompiler.PreprocessingBenchmark PRIVATE ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS})
endif()

ly_add_target(
    NAME Azslc EXECUTABLE
    OUTPUT_NAME azslc
    NAMESPACE AZ
    FILES_CMAKE
        azslc_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            AZ::ShaderCompiler
            3rdParty::antlr4
            3rdParty::cli11
            3rdParty::jsoncpp
)

target_compile_options(Azslc PRIVATE
    ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
)

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    ly_add_test(
        NAME AZ::ShaderCompiler.PreprocessingConformance
        PARENT_NAME AZ::Azslc
        TEST_LIBRARY standalone
        TEST_COMMAND ${LY_PYTHON_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/Tests/preprocessing_conformance.py
            $<TARGET_FILE:ShaderCompiler.Tests> $<TARGET_FILE:Azslc>
        RUNTIME_DEPENDENCIES AZ::ShaderCompiler.Tests AZ::Azslc
    )

    ly_add_test(
        NAME AZ::ShaderCompiler.NativePreprocessing
        PARENT_NAME AZ::Azslc
        TEST_LIBRARY standalone
        TEST_COMMAND ${LY_PYTHON_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/Tests/native_preprocessing_tests.py $<TARGET_FILE:Azslc>
        RUNTIME_DEPENDENCIES AZ::Azslc
    )

    ly_add_test(
        NAME AZ::ShaderCompiler.LegacySuite
        PARENT_NAME AZ::Azslc
        TEST_LIBRARY standalone
        TEST_COMMAND ${LY_PYTHON_CMD} ${CMAKE_CURRENT_SOURCE_DIR}/Tests/run_compiler_suite.py $<TARGET_FILE:Azslc>
        RUNTIME_DEPENDENCIES AZ::Azslc
    )
endif()

set_source_files_properties(
    Source/Grammar/azslLexer.cpp
    Source/Grammar/azslParser.cpp
    Source/Grammar/azslParserBaseListener.cpp
    Source/Grammar/azslParserListener.cpp
    PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>"
)
