#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_target(
    NAME ${gem_name}.API INTERFACE
    NAMESPACE Gem
    FILES_CMAKE
        jolt_api_files.cmake
    INCLUDE_DIRECTORIES
        INTERFACE
            Include
    BUILD_DEPENDENCIES
        INTERFACE
            AZ::AzCore
)

ly_add_target(
    NAME ${gem_name}.Private.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        jolt_private_files.cmake
    TARGET_PROPERTIES
        O3DE_PRIVATE_TARGET TRUE
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzCore
        PRIVATE
            Jolt
            Gem::${gem_name}.API
)
set_property(TARGET ${gem_name}.Private.Static PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
if(MSVC)
    target_compile_options(${gem_name}.Private.Static PRIVATE /GR-)
else()
    target_compile_options(${gem_name}.Private.Static PRIVATE -fno-rtti)
endif()

set(jolt_native_target Jolt)
get_target_property(jolt_source_dir ${jolt_native_target} JOLT_SOURCE_ROOT)
if(NOT jolt_source_dir)
    message(FATAL_ERROR "The Jolt package did not provide its native source root.")
endif()
get_target_property(jolt_patch_revision ${jolt_native_target} JOLT_PATCH_REVISION)
if(NOT jolt_patch_revision)
    message(FATAL_ERROR "The Jolt package did not provide its patch revision.")
endif()
get_target_property(jolt_patch_hash ${jolt_native_target} JOLT_PATCH_HASH)
if(NOT jolt_patch_hash)
    message(FATAL_ERROR "The Jolt package did not provide its patch hash.")
endif()
get_target_property(jolt_source_revision ${jolt_native_target} JOLT_SOURCE_REVISION)
if(NOT jolt_source_revision)
    message(FATAL_ERROR "The Jolt package did not provide its source revision.")
endif()
target_compile_definitions(${gem_name}.Private.Static PRIVATE
    JOLT_NATIVE_PATCH_HASH="${jolt_patch_hash}"
    JOLT_NATIVE_PATCH_REVISION="${jolt_patch_revision}"
    JOLT_NATIVE_SOURCE_REVISION="${jolt_source_revision}"
)

ly_add_target(
    NAME ${gem_name}.Module ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    OUTPUT_NAME ${gem_name}.Gem
    FILES_CMAKE
        jolt_shared_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.API
        PRIVATE
            Gem::${gem_name}.Private.Static
)

add_library(Gem::${gem_name} ALIAS ${gem_name}.Module)

ly_add_source_properties(
    SOURCES
        Source/Jolt/Module.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            O3DE_GEM_NAME=${gem_name}
            O3DE_GEM_VERSION=${gem_version}
)

set(jolt_client_targets Gem::${gem_name})

ly_create_alias(NAME ${gem_name}.Clients NAMESPACE Gem TARGETS ${jolt_client_targets})
ly_create_alias(NAME ${gem_name}.Servers NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Unified NAMESPACE Gem TARGETS ${jolt_client_targets})

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
            jolt_editor_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzFramework
                AZ::AzToolsFramework
                Gem::${gem_name}.Private.Static
            PRIVATE
                AZ::AssetBuilderSDK
    )

    ly_add_target(
        NAME ${gem_name}.Editor GEM_MODULE
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.Editor.Gem
        FILES_CMAKE
            jolt_editor_module_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AssetBuilderSDK
                Gem::${gem_name}.Editor.Static
    )

    ly_add_source_properties(
        SOURCES
            Source/Jolt/Editor/Module.cpp
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
            jolt_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                Jolt
                AZ::AzTest
                AZ::AzFramework
                Gem::${gem_name}.Private.Static
    )
    if(MSVC)
        target_compile_options(${gem_name}.Tests PRIVATE /GR-)
    else()
        target_compile_options(${gem_name}.Tests PRIVATE -fno-rtti)
    endif()

    ly_add_googletest(NAME Gem::${gem_name}.Tests)

    ly_add_googlebenchmark(
        NAME Gem::${gem_name}.Benchmarks
        TARGET Gem::${gem_name}.Tests
        COMPONENT Physics
        TIMEOUT 600
    )

    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        ly_add_target(
            NAME ${gem_name}.Editor.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.Editor.Tests.Gem
            FILES_CMAKE
                jolt_editor_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AssetBuilderSDK
                    AZ::AzCoreTestCommon
                    AZ::AzTest
                    AZ::AzToolsFramework
                    AZ::AzToolsFrameworkTestCommon
                    Gem::${gem_name}.Editor.Static
        )

        ly_add_googletest(NAME Gem::${gem_name}.Editor.Tests)

        ly_add_target(
            NAME ${gem_name}.Editor.Builder.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.Editor.Builder.Tests.Gem
            FILES_CMAKE
                jolt_editor_builder_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AssetBuilderSDK
                    AZ::AzCoreTestCommon
                    AZ::AzTest
                    AZ::AzToolsFramework
                    AZ::AzToolsFrameworkTestCommon
                    Gem::${gem_name}.Editor.Static
        )

        ly_add_googletest(NAME Gem::${gem_name}.Editor.Builder.Tests)
    endif()
endif()

# This directory's targets are re-declared as IMPORTED libraries in an installed
# engine, and the STATIC ones carry their private Jolt dependency into that
# declaration. Nothing re-runs this file there, so emit the imported Jolt target into
# the generated permutation.cmake ahead of the targets that link it.
#
# Only the archive is published. Jolt's headers are not installed, so the target is
# link-only and cannot be used to compile against Jolt directly.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND_STRING PROPERTY O3DE_SUBDIRECTORY_INSTALL_CODE
[==[
if(NOT TARGET Jolt)
    add_library(Jolt STATIC IMPORTED GLOBAL)
    set(jolt_library_folder "${LY_ROOT_FOLDER}/lib/${PAL_PLATFORM_NAME}")
    set_target_properties(Jolt PROPERTIES
        IMPORTED_LOCATION "${jolt_library_folder}/profile/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
        IMPORTED_LOCATION_DEBUG "${jolt_library_folder}/debug/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
        IMPORTED_LOCATION_RELEASE "${jolt_library_folder}/release/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_link_options(Jolt INTERFACE
            "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
        )
    else()
        target_link_options(Jolt INTERFACE
            "$<$<CONFIG:Release>:${CMAKE_CXX_COMPILE_OPTIONS_IPO}>"
            "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
        )
    endif()
endif()
]==])
