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
            Gem::${gem_name}.API
)
set_property(TARGET ${gem_name}.Private.Static PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
# Linking the object library folds native Jolt into this private archive.
target_link_libraries(${gem_name}.Private.Static PRIVATE Jolt)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_options(${gem_name}.Private.Static INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
    )
else()
    target_link_options(${gem_name}.Private.Static INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_CXX_COMPILE_OPTIONS_IPO}>"
        "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
    )
endif()
if(MSVC)
    target_compile_options(${gem_name}.Private.Static PRIVATE /GR-)
else()
    target_compile_options(${gem_name}.Private.Static PRIVATE -fno-rtti)
endif()

set_property(
    DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../3rdParty/jolt-5.6.0-o3de.patch")
file(
    SHA256
    "${CMAKE_CURRENT_LIST_DIR}/../3rdParty/jolt-5.6.0-o3de.patch"
    jolt_patch_hash)
string(SUBSTRING "${jolt_patch_hash}" 0 16 jolt_patch_fingerprint)
target_compile_definitions(${gem_name}.Private.Static PRIVATE
    JOLT_NATIVE_PATCH_FINGERPRINT=0x${jolt_patch_fingerprint}ULL
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
    set(jolt_test_files
        jolt_tests_files.cmake
    )
    set(jolt_test_dependencies
        Jolt
        AZ::AzTest
        AZ::AzFramework
        Gem::${gem_name}.Private.Static
    )

    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        list(APPEND jolt_test_files
            jolt_tools_tests_files.cmake
        )
        list(APPEND jolt_test_dependencies
            AZ::AssetBuilderSDK
            AZ::AzCoreTestCommon
            AZ::AzToolsFramework
            AZ::AzToolsFrameworkTestCommon
            Gem::${gem_name}.Editor.Static
        )
    endif()

    ly_add_target(
        NAME ${gem_name}.TestProviders MODULE
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.TestProviders
        FILES_CMAKE
            jolt_test_provider_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AzCore
                Gem::${gem_name}.API
    )

    ly_add_target(
        NAME ${gem_name}.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.Tests.Gem
        FILES_CMAKE
            ${jolt_test_files}
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                ${jolt_test_dependencies}
    )

    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        target_compile_definitions(
            ${gem_name}.Tests PRIVATE
            JOLT_TESTS_WITH_TOOLS
            JOLT_TEST_ENGINE_ROOT="${LY_ROOT_FOLDER}"
        )
    endif()

    add_dependencies(${gem_name}.Tests ${gem_name}.TestProviders)

    ly_add_googletest(NAME Gem::${gem_name}.Tests)

    ly_add_googlebenchmark(
        NAME Gem::${gem_name}.Benchmarks
        TARGET Gem::${gem_name}.Tests
        COMPONENT Physics
        TIMEOUT 600
    )
endif()
