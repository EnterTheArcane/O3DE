#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_target(
    NAME ${gem_name}.API ${PAL_TRAIT_MONOLITHIC_DRIVEN_LIBRARY_TYPE}
    NAMESPACE Gem
    FILES_CMAKE
        box3d_api_files.cmake
        box3d_private_files.cmake
    COMPILE_DEFINITIONS
        PRIVATE
            BOX3D_API_EXPORTS
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
        PUBLIC
            Include
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzCore
        PRIVATE
            3rdParty::Box3D
)
set_property(TARGET ${gem_name}.API PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

ly_add_target(
    NAME ${gem_name}.Module ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    OUTPUT_NAME ${gem_name}.Gem
    FILES_CMAKE
        box3d_shared_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.API
)

# Keep the Gem's canonical target while avoiding a case-insensitive collision
# between the private native library and the alias backing target.
ly_create_alias(
    NAME ${gem_name}
    NAMESPACE Gem
    INTERNAL_NAME ${gem_name}.GemAlias
    TARGETS Gem::${gem_name}.Module
)

ly_add_source_properties(
    SOURCES
        Source/Box3D/Module.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            O3DE_GEM_NAME=${gem_name}_Module
            O3DE_GEM_VERSION=${gem_version}
)

ly_create_alias(NAME ${gem_name}.Clients NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Servers NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Unified NAMESPACE Gem TARGETS Gem::${gem_name})

ly_create_alias(NAME ${gem_name}.Clients.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Servers.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Unified.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Tools.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Builders.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)

if(PAL_TRAIT_BUILD_HOST_TOOLS)
    ly_add_target(
        NAME ${gem_name}.Editor.Static STATIC
        NAMESPACE Gem
        FILES_CMAKE
            box3d_editor_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzFramework
                AZ::AzToolsFramework
                Gem::${gem_name}.API
    )

    ly_add_target(
        NAME ${gem_name}.Editor GEM_MODULE
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.Editor.Gem
        FILES_CMAKE
            box3d_editor_module_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
        BUILD_DEPENDENCIES
            PRIVATE
                Gem::${gem_name}.Editor.Static
    )

    ly_add_source_properties(
        SOURCES
            Source/Box3D/Editor/Module.cpp
        PROPERTY COMPILE_DEFINITIONS
            VALUES
                O3DE_GEM_NAME=${gem_name}
                O3DE_GEM_VERSION=${gem_version}
    )

    ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
    ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
else()
    ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name})
    ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name})
endif()

o3de_add_variant_dependencies_for_gem_dependencies(
    GEM_NAME ${gem_name}
    VARIANTS Clients Servers Unified Tools Builders
)

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    ly_add_target(
        NAME ${gem_name}.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.Tests.Gem
        FILES_CMAKE
            box3d_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AzTest
                AZ::AzFramework
                Gem::${gem_name}.API
    )

    ly_add_googletest(NAME Gem::${gem_name}.Tests)

    ly_add_googlebenchmark(
        NAME Gem::${gem_name}.Benchmarks
        TARGET Gem::${gem_name}.Tests
        COMPONENT Physics
        TIMEOUT 600
    )

    ly_add_pytest(
        NAME Gem::${gem_name}.BenchmarkComparator
        PATH ${CMAKE_CURRENT_LIST_DIR}/Tests/test_compare_benchmarks.py
        COMPONENT Physics
        TIMEOUT 30
    )

    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        ly_add_target(
            NAME ${gem_name}.Editor.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.Editor.Tests.Gem
            FILES_CMAKE
                box3d_editor_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AzTest
                    AZ::AzToolsFramework
                    AZ::AzToolsFrameworkTestCommon
                    Gem::${gem_name}.Editor.Static
        )

        ly_add_googletest(NAME Gem::${gem_name}.Editor.Tests)
    endif()
endif()
