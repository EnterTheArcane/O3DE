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

set(jolt_gpu_hair_dx12_supported FALSE)
if(PAL_PLATFORM_NAME STREQUAL "Windows" AND TARGET Gem::Atom_RHI_DX12.Interface)
    set(jolt_gpu_hair_dx12_supported TRUE)
endif()

set(jolt_gpu_hair_vulkan_supported FALSE)
if(TARGET Gem::Atom_RHI_Vulkan.Interface)
    set(jolt_gpu_hair_vulkan_supported TRUE)
endif()

set(jolt_gpu_hair_metal_supported FALSE)
if(TARGET Gem::Atom_RHI_Metal.Interface)
    set(jolt_gpu_hair_metal_supported TRUE)
endif()

set(jolt_client_targets Gem::${gem_name})
if(jolt_gpu_hair_dx12_supported OR jolt_gpu_hair_vulkan_supported OR jolt_gpu_hair_metal_supported)
    ly_parse_third_party_dependencies("3rdParty::DirectXShaderCompilerDxc")
    get_target_property(
        jolt_dxc_target_files
        3rdParty::DirectXShaderCompilerDxc
        INTERFACE_LY_TARGET_FILES)
    foreach(jolt_dxc_target_file IN LISTS jolt_dxc_target_files)
        string(REPLACE "\n" ";" jolt_dxc_target_file_parts "${jolt_dxc_target_file}")
        list(GET jolt_dxc_target_file_parts 0 jolt_dxc_file)
        get_filename_component(jolt_dxc_file_name "${jolt_dxc_file}" NAME)
        if(jolt_dxc_file_name STREQUAL "dxc.exe" OR jolt_dxc_file_name STREQUAL "dxc")
            set(jolt_dxc_executable "${jolt_dxc_file}")
        elseif(jolt_dxc_file_name STREQUAL "dxcompiler.dll")
            set(jolt_dxcompiler_library "${jolt_dxc_file}")
        elseif(jolt_dxc_file_name STREQUAL "dxil.dll")
            set(jolt_dxil_library "${jolt_dxc_file}")
        endif()
    endforeach()

    if(NOT jolt_dxc_executable)
        message(FATAL_ERROR "The DirectX Shader Compiler package is incomplete.")
    endif()
    if(jolt_gpu_hair_dx12_supported AND (NOT jolt_dxcompiler_library OR NOT jolt_dxil_library))
        message(FATAL_ERROR "The DirectX Shader Compiler runtime is incomplete.")
    endif()

    if(jolt_gpu_hair_metal_supported)
        ly_parse_third_party_dependencies("3rdParty::SPIRVCross")
        get_target_property(
            jolt_spirv_cross_target_files
            3rdParty::SPIRVCross
            INTERFACE_LY_TARGET_FILES)
        foreach(jolt_spirv_cross_target_file IN LISTS jolt_spirv_cross_target_files)
            string(REPLACE "\n" ";" jolt_spirv_cross_target_file_parts "${jolt_spirv_cross_target_file}")
            list(GET jolt_spirv_cross_target_file_parts 0 jolt_spirv_cross_file)
            get_filename_component(jolt_spirv_cross_file_name "${jolt_spirv_cross_file}" NAME)
            if(jolt_spirv_cross_file_name STREQUAL "spirv-cross"
                OR jolt_spirv_cross_file_name STREQUAL "spirv-cross.exe")
                set(jolt_spirv_cross_executable "${jolt_spirv_cross_file}")
            endif()
        endforeach()
        if(NOT jolt_spirv_cross_executable)
            message(FATAL_ERROR "The SPIRV-Cross package is incomplete.")
        endif()

        find_program(jolt_xcrun_executable xcrun REQUIRED)
        set(jolt_metal_sdk macosx)
        set(jolt_spirv_cross_msl_options --msl)
        if(PAL_PLATFORM_NAME STREQUAL "iOS")
            set(jolt_metal_sdk iphoneos)
            list(APPEND jolt_spirv_cross_msl_options --msl-ios)
            if(CMAKE_OSX_SYSROOT MATCHES "simulator")
                set(jolt_metal_sdk iphonesimulator)
            endif()
        endif()
    endif()

    include(jolt_gpu_hair_shader_files.cmake)
    set(jolt_hair_shader_headers)
    foreach(jolt_hair_shader_header IN LISTS JOLT_GPU_HAIR_SHADER_HEADERS)
        list(APPEND jolt_hair_shader_headers "${jolt_source_dir}/${jolt_hair_shader_header}")
    endforeach()
    set(jolt_hair_shader_output_directory ${CMAKE_CURRENT_BINARY_DIR}/Jolt/Shaders)
    file(MAKE_DIRECTORY ${jolt_hair_shader_output_directory})
    foreach(jolt_hair_shader IN LISTS JOLT_GPU_HAIR_SHADER_NAMES)
        set(jolt_hair_shader_source ${jolt_source_dir}/Jolt/Shaders/${jolt_hair_shader}.hlsl)
        if(jolt_gpu_hair_dx12_supported)
            set(jolt_hair_dxil_output ${jolt_hair_shader_output_directory}/${jolt_hair_shader}.dxil)
            add_custom_command(
                OUTPUT ${jolt_hair_dxil_output}
                COMMAND
                    ${jolt_dxc_executable}
                    -E main
                    -T cs_6_0
                    -I ${jolt_source_dir}/Jolt/Shaders
                    -WX
                    -all_resources_bound
                    -O3
                    ${jolt_hair_shader_source}
                    -Fo ${jolt_hair_dxil_output}
                DEPENDS
                    ${jolt_hair_shader_source}
                    ${jolt_hair_shader_headers}
                VERBATIM
            )
            list(APPEND jolt_hair_shader_outputs ${jolt_hair_dxil_output})
        endif()

        if(jolt_gpu_hair_vulkan_supported)
            set(jolt_hair_spirv_output ${jolt_hair_shader_output_directory}/${jolt_hair_shader}.spv)
            add_custom_command(
                OUTPUT ${jolt_hair_spirv_output}
                COMMAND
                    ${jolt_dxc_executable}
                    -E main
                    -T cs_6_0
                    -I ${jolt_source_dir}/Jolt/Shaders
                    -WX
                    -all_resources_bound
                    -O3
                    -spirv
                    -fvk-use-dx-layout
                    ${jolt_hair_shader_source}
                    -Fo ${jolt_hair_spirv_output}
                DEPENDS
                    ${jolt_hair_shader_source}
                    ${jolt_hair_shader_headers}
                VERBATIM
            )
            list(APPEND jolt_hair_shader_outputs ${jolt_hair_spirv_output})
        endif()

        if(jolt_gpu_hair_metal_supported)
            set(jolt_hair_metal_spirv_output ${jolt_hair_shader_output_directory}/${jolt_hair_shader}.metal.spv)
            set(jolt_hair_metal_source_output ${jolt_hair_shader_output_directory}/${jolt_hair_shader}.metal)
            set(jolt_hair_metal_air_output ${jolt_hair_shader_output_directory}/${jolt_hair_shader}.air)
            add_custom_command(
                OUTPUT ${jolt_hair_metal_air_output}
                COMMAND
                    ${jolt_dxc_executable}
                    -E main
                    -T cs_6_0
                    -I ${jolt_source_dir}/Jolt/Shaders
                    -WX
                    -all_resources_bound
                    -O3
                    -spirv
                    -fvk-use-dx-layout
                    -fspv-entrypoint-name=${jolt_hair_shader}
                    ${jolt_hair_shader_source}
                    -Fo ${jolt_hair_metal_spirv_output}
                COMMAND
                    ${jolt_spirv_cross_executable}
                    ${jolt_hair_metal_spirv_output}
                    ${jolt_spirv_cross_msl_options}
                    --output ${jolt_hair_metal_source_output}
                COMMAND
                    ${jolt_xcrun_executable}
                    -sdk ${jolt_metal_sdk}
                    metal
                    -c ${jolt_hair_metal_source_output}
                    -o ${jolt_hair_metal_air_output}
                DEPENDS
                    ${jolt_hair_shader_source}
                    ${jolt_hair_shader_headers}
                VERBATIM
            )
            list(APPEND jolt_hair_metal_air_outputs ${jolt_hair_metal_air_output})
        endif()
    endforeach()
    if(jolt_gpu_hair_metal_supported)
        set(jolt_hair_metal_library ${jolt_hair_shader_output_directory}/Jolt.metallib)
        add_custom_command(
            OUTPUT ${jolt_hair_metal_library}
            COMMAND
                ${jolt_xcrun_executable}
                -sdk ${jolt_metal_sdk}
                metallib
                -o ${jolt_hair_metal_library}
                ${jolt_hair_metal_air_outputs}
            DEPENDS ${jolt_hair_metal_air_outputs}
            VERBATIM
        )
        list(APPEND jolt_hair_shader_outputs ${jolt_hair_metal_library})
    endif()
    add_custom_target(
        ${gem_name}.GpuHair.Shaders
        DEPENDS ${jolt_hair_shader_outputs})

    set(jolt_gpu_hair_files_cmake jolt_gpu_hair_files.cmake)
    set(jolt_gpu_hair_compile_definitions)
    set(jolt_gpu_hair_build_dependencies
        Jolt
        AZ::AzCore
        Gem::${gem_name}.API
        Gem::Atom_RHI.Public
        Gem::Atom_RPI.Public)
    set(jolt_gpu_hair_runtime_dependencies
        Gem::Atom_RHI.Private)

    if(jolt_gpu_hair_dx12_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.DX12.Native STATIC
            NAMESPACE Gem
            FILES_CMAKE
                jolt_gpu_hair_dx12_native_files.cmake
            TARGET_PROPERTIES
                O3DE_PRIVATE_TARGET TRUE
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
        )
        target_include_directories(
            ${gem_name}.GpuHair.DX12.Native
            PRIVATE
                ${jolt_source_dir}
        )
        target_compile_definitions(${gem_name}.GpuHair.DX12.Native PRIVATE JPH_USE_DX12)
        target_compile_options(${gem_name}.GpuHair.DX12.Native ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})
        target_link_libraries(
            ${gem_name}.GpuHair.DX12.Native
            PRIVATE
                d3d12.lib
                d3dcompiler.lib
                dxgi.lib
                dxguid.lib
        )
        set_property(
            TARGET ${gem_name}.GpuHair.DX12.Native
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

        list(APPEND jolt_gpu_hair_files_cmake jolt_gpu_hair_dx12_files.cmake)
        list(APPEND jolt_gpu_hair_compile_definitions JOLT_GPU_HAIR_DX12 JPH_USE_DX12)
        list(APPEND jolt_gpu_hair_build_dependencies
            Gem::${gem_name}.GpuHair.DX12.Native
            Gem::Atom_RHI_DX12.Interface)
        list(APPEND jolt_gpu_hair_runtime_dependencies Gem::Atom_RHI_DX12.Private)
    endif()

    if(jolt_gpu_hair_vulkan_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.Vulkan.Native STATIC
            NAMESPACE Gem
            FILES_CMAKE
                jolt_gpu_hair_vulkan_native_files.cmake
            TARGET_PROPERTIES
                O3DE_PRIVATE_TARGET TRUE
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
                    Gem::Atom_RHI_Vulkan.Interface
        )
        target_include_directories(
            ${gem_name}.GpuHair.Vulkan.Native
            PRIVATE
                ${jolt_source_dir}
        )
        target_compile_definitions(${gem_name}.GpuHair.Vulkan.Native PRIVATE JPH_USE_VK)
        target_compile_options(${gem_name}.GpuHair.Vulkan.Native ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})
        set_property(
            TARGET ${gem_name}.GpuHair.Vulkan.Native
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

        list(APPEND jolt_gpu_hair_files_cmake jolt_gpu_hair_vulkan_files.cmake)
        list(APPEND jolt_gpu_hair_compile_definitions JOLT_GPU_HAIR_VULKAN JPH_USE_VK)
        list(APPEND jolt_gpu_hair_build_dependencies
            Gem::${gem_name}.GpuHair.Vulkan.Native
            Gem::Atom_RHI_Vulkan.Interface)
        list(APPEND jolt_gpu_hair_runtime_dependencies Gem::Atom_RHI_Vulkan.Private)
    endif()

    if(jolt_gpu_hair_metal_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.Metal.Native STATIC
            NAMESPACE Gem
            FILES_CMAKE
                jolt_gpu_hair_metal_native_files.cmake
            TARGET_PROPERTIES
                O3DE_PRIVATE_TARGET TRUE
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
        )
        target_include_directories(
            ${gem_name}.GpuHair.Metal.Native
            PRIVATE
                ${jolt_source_dir}
        )
        target_compile_definitions(${gem_name}.GpuHair.Metal.Native PRIVATE JPH_USE_MTL)
        target_compile_options(${gem_name}.GpuHair.Metal.Native ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})
        target_link_libraries(
            ${gem_name}.GpuHair.Metal.Native
            PRIVATE
                "-framework CoreGraphics"
                "-framework Foundation"
                "-framework Metal"
                "-framework MetalKit"
        )
        set_property(
            TARGET ${gem_name}.GpuHair.Metal.Native
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

        list(APPEND jolt_gpu_hair_files_cmake jolt_gpu_hair_metal_files.cmake)
        list(APPEND jolt_gpu_hair_compile_definitions JOLT_GPU_HAIR_METAL JPH_USE_MTL)
        list(APPEND jolt_gpu_hair_build_dependencies
            Gem::${gem_name}.GpuHair.Metal.Native
            Gem::Atom_RHI_Metal.Interface)
        list(APPEND jolt_gpu_hair_runtime_dependencies Gem::Atom_RHI_Metal.Private)
    endif()

    ly_add_target(
        NAME ${gem_name}.GpuHair.Static STATIC
        NAMESPACE Gem
        FILES_CMAKE
            ${jolt_gpu_hair_files_cmake}
        TARGET_PROPERTIES
            O3DE_PRIVATE_TARGET TRUE
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
                ${jolt_source_dir}
        COMPILE_DEFINITIONS
            PRIVATE
                ${jolt_gpu_hair_compile_definitions}
        BUILD_DEPENDENCIES
            PRIVATE
                ${jolt_gpu_hair_build_dependencies}
    )
    add_dependencies(
        ${gem_name}.GpuHair.Static
        ${gem_name}.GpuHair.Shaders)
    set_property(
        TARGET ${gem_name}.GpuHair.Static
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

    ly_add_target(
        NAME ${gem_name}.GpuHair ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
        NAMESPACE Gem
        OUTPUT_NAME ${gem_name}.GpuHair.Gem
        FILES_CMAKE
            jolt_gpu_hair_module_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
        BUILD_DEPENDENCIES
            PRIVATE
                Jolt
                AZ::AzCore
                Gem::${gem_name}.API
                Gem::${gem_name}.GpuHair.Static
        RUNTIME_DEPENDENCIES
            ${jolt_gpu_hair_runtime_dependencies}
    )

    ly_add_target_files(
        TARGETS
            ${gem_name}.GpuHair
        FILES
            ${jolt_hair_shader_outputs}
        OUTPUT_SUBDIRECTORY
            Jolt/Shaders
    )
    if(jolt_dxcompiler_library AND jolt_dxil_library)
        ly_add_target_files(
            TARGETS
                ${gem_name}.GpuHair
            FILES
                ${jolt_dxcompiler_library}
                ${jolt_dxil_library}
        )
    endif()

    list(APPEND jolt_client_targets Gem::${gem_name}.GpuHair)

    if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND jolt_gpu_hair_dx12_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.DX12.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.GpuHair.DX12.Tests.Gem
            FILES_CMAKE
                jolt_gpu_hair_dx12_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            COMPILE_DEFINITIONS
                PRIVATE
                    JPH_USE_DX12
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
                    AZ::AzTest
                    AZ::AzFramework
                    Gem::${gem_name}.Private.Static
                    Gem::${gem_name}.GpuHair.Static
        )
        target_link_libraries(${gem_name}.GpuHair.DX12.Tests PRIVATE d3d12.lib)
        add_dependencies(
            ${gem_name}.GpuHair.DX12.Tests
            ${gem_name}.GpuHair.Shaders)
        ly_add_target_files(
            TARGETS
                ${gem_name}.GpuHair.DX12.Tests
            FILES
                ${jolt_hair_shader_outputs}
            OUTPUT_SUBDIRECTORY
                Jolt/Shaders
        )
        ly_add_target_files(
            TARGETS
                ${gem_name}.GpuHair.DX12.Tests
            FILES
                ${jolt_dxcompiler_library}
                ${jolt_dxil_library}
        )
        ly_add_googletest(NAME Gem::${gem_name}.GpuHair.DX12.Tests)
    endif()

    if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND jolt_gpu_hair_vulkan_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.Vulkan.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.GpuHair.Vulkan.Tests.Gem
            FILES_CMAKE
                jolt_gpu_hair_vulkan_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            COMPILE_DEFINITIONS
                PRIVATE
                    JPH_USE_VK
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
                    AZ::AzTest
                    AZ::AzFramework
                    Gem::${gem_name}.Private.Static
                    Gem::${gem_name}.GpuHair.Static
                    Gem::Atom_RHI_Vulkan.Interface
        )
        add_dependencies(
            ${gem_name}.GpuHair.Vulkan.Tests
            ${gem_name}.GpuHair.Shaders)
        ly_add_target_files(
            TARGETS
                ${gem_name}.GpuHair.Vulkan.Tests
            FILES
                ${jolt_hair_shader_outputs}
            OUTPUT_SUBDIRECTORY
                Jolt/Shaders
        )
        ly_add_googletest(NAME Gem::${gem_name}.GpuHair.Vulkan.Tests)
    endif()

    if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND jolt_gpu_hair_metal_supported)
        ly_add_target(
            NAME ${gem_name}.GpuHair.Metal.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            OUTPUT_NAME ${gem_name}.GpuHair.Metal.Tests.Gem
            FILES_CMAKE
                jolt_gpu_hair_metal_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Include
                    Source
                    Tests
            COMPILE_DEFINITIONS
                PRIVATE
                    JPH_USE_MTL
            BUILD_DEPENDENCIES
                PRIVATE
                    Jolt
                    AZ::AzTest
                    AZ::AzFramework
                    Gem::${gem_name}.Private.Static
                    Gem::${gem_name}.GpuHair.Static
                    Gem::Atom_RHI_Metal.Interface
        )
        target_link_libraries(
            ${gem_name}.GpuHair.Metal.Tests
            PRIVATE
                "-framework CoreGraphics"
                "-framework Foundation"
                "-framework Metal"
                "-framework MetalKit"
        )
        add_dependencies(
            ${gem_name}.GpuHair.Metal.Tests
            ${gem_name}.GpuHair.Shaders)
        ly_add_target_files(
            TARGETS
                ${gem_name}.GpuHair.Metal.Tests
            FILES
                ${jolt_hair_shader_outputs}
            OUTPUT_SUBDIRECTORY
                Jolt/Shaders
        )
        ly_add_googletest(NAME Gem::${gem_name}.GpuHair.Metal.Tests)
    endif()
endif()

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

    set(jolt_tools_targets Gem::${gem_name}.Editor)
    if(TARGET ${gem_name}.GpuHair)
        list(APPEND jolt_tools_targets Gem::${gem_name}.GpuHair)
    endif()
    ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS ${jolt_tools_targets})
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
                Gem::Atom_RHI.Public
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
