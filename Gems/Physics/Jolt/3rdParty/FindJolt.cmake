#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(TARGET 3rdParty::Jolt)
    return()
endif()

set(jolt_source_revision "e77f175595e64cb44218cc9d9d56fc365ad0e36a")
set(jolt_patch_revision "jolt-v5.6.0-o3de-10")
set(jolt_archive_hash "6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e")
set(jolt_archive_name "v5.6.0.tar.gz")
set(jolt_patch_file "${CMAKE_CURRENT_LIST_DIR}/jolt-5.6.0-o3de.patch")
file(SHA256 "${jolt_patch_file}" jolt_patch_hash)

option(LY_JOLT_DOUBLE_PRECISION "Build Jolt with double-precision world positions" OFF)
option(LY_JOLT_ENABLE_DEBUG_RENDERING "Build the native diagnostic renderer" ON)
option(
    LY_JOLT_ENABLE_DETAILED_PROFILING
    "Route Jolt native profiling scopes through the AzCore Physics budget"
    OFF)
option(
    LY_JOLT_ENABLE_SIMULATION_STATISTICS
    "Collect per-body native simulation cost counters for diagnostic builds"
    OFF)
set(
    LY_JOLT_SIMD_LEVEL
    "AVX2"
    CACHE STRING
    "Jolt x86 SIMD level: SSE2, SSE41, SSE42, AVX, AVX2, or AVX512")
set_property(CACHE LY_JOLT_SIMD_LEVEL PROPERTY STRINGS SSE2 SSE41 SSE42 AVX AVX2 AVX512)

if(NOT LY_JOLT_SIMD_LEVEL MATCHES "^(SSE2|SSE41|SSE42|AVX|AVX2|AVX512)$")
    message(FATAL_ERROR "LY_JOLT_SIMD_LEVEL must be SSE2, SSE41, SSE42, AVX, AVX2, or AVX512.")
endif()

block()
    o3de_fetch_content(JoltPhysics
        VERSION "v5.6.0"
        LICENSE "MIT"
        URL "https://github.com/jrouwe/JoltPhysics/archive/refs/tags/${jolt_archive_name}"
        URL_HASH "${jolt_archive_hash}"
        GIT "https://github.com/jrouwe/JoltPhysics.git"
        GIT_HASH "${jolt_source_revision}"
        PATCH_FILES "${jolt_patch_file}"
        SOURCE_SUBDIR Build
        EXCLUDE_FROM_ALL
    )

    set(CMAKE_MESSAGE_LOG_LEVEL ${O3DE_FETCHCONTENT_MESSAGE_LEVEL})
    set(CPP_EXCEPTIONS_ENABLED OFF)
    set(CPP_RTTI_ENABLED OFF)
    set(CROSS_PLATFORM_DETERMINISTIC ON)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(DEBUG_RENDERER_IN_DISTRIBUTION ${LY_JOLT_ENABLE_DEBUG_RENDERING})
    set(DISABLE_CUSTOM_ALLOCATOR OFF)
    set(DOUBLE_PRECISION ${LY_JOLT_DOUBLE_PRECISION})
    set(ENABLE_ALL_WARNINGS OFF)
    set(ENABLE_INSTALL OFF)
    set(ENABLE_OBJECT_STREAM ON)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
    set(GENERATE_DEBUG_SYMBOLS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(JPH_BUILD_SHARED_LIBS OFF)
    set(JPH_SHADER_DEBUG_SYMBOLS OFF)
    set(JPH_SHADER_OPTIMIZATION ON)
    set(JPH_TRACK_SIMULATION_STATS ${LY_JOLT_ENABLE_SIMULATION_STATISTICS})
    set(JPH_USE_CPU_COMPUTE ON)
    set(JPH_USE_DX12 OFF)
    set(JPH_USE_EXTERNAL_PROFILE ${LY_JOLT_ENABLE_DETAILED_PROFILING})
    set(JPH_USE_MTL OFF)
    set(JPH_USE_VK OFF)
    set(OBJECT_LAYER_BITS 32)
    set(OVERRIDE_CXX_FLAGS OFF)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DISTRIBUTION ${LY_JOLT_ENABLE_DETAILED_PROFILING})
    set(TRACK_BROADPHASE_STATS OFF)
    set(TRACK_NARROWPHASE_STATS OFF)
    set(USE_ASSERTS OFF)
    set(USE_AVX OFF)
    set(USE_AVX2 OFF)
    set(USE_AVX512 OFF)
    set(USE_F16C OFF)
    set(USE_FMADD OFF)
    set(USE_LZCNT OFF)
    set(USE_SSE4_1 OFF)
    set(USE_SSE4_2 OFF)
    set(USE_TZCNT OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(USE_WASM_SIMD ON)

    if(LY_JOLT_SIMD_LEVEL MATCHES "^(SSE41|SSE42|AVX|AVX2|AVX512)$")
        set(USE_SSE4_1 ON)
    endif()
    if(LY_JOLT_SIMD_LEVEL MATCHES "^(SSE42|AVX|AVX2|AVX512)$")
        set(USE_SSE4_2 ON)
    endif()
    if(LY_JOLT_SIMD_LEVEL MATCHES "^(AVX2|AVX512)$")
        set(USE_LZCNT ON)
        set(USE_TZCNT ON)
    endif()
    if(LY_JOLT_SIMD_LEVEL MATCHES "^(AVX|AVX2|AVX512)$")
        set(USE_AVX ON)
    endif()
    if(LY_JOLT_SIMD_LEVEL MATCHES "^(AVX2|AVX512)$")
        set(USE_AVX2 ON)
        set(USE_F16C ON)
    endif()
    if(LY_JOLT_SIMD_LEVEL STREQUAL "AVX512")
        set(USE_AVX512 ON)
    endif()

    FetchContent_MakeAvailable(JoltPhysics)
endblock()

set_target_properties(Jolt PROPERTIES DEBUG_POSTFIX "")
set_target_properties(Jolt PROPERTIES
    JOLT_PATCH_HASH "${jolt_patch_hash}"
    JOLT_PATCH_REVISION "${jolt_patch_revision}"
    JOLT_SOURCE_REVISION "${jolt_source_revision}"
)
set_property(TARGET Jolt PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(Jolt PUBLIC ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH})
elseif(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Jolt PUBLIC
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        /clang:-ffp-contract=off
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(Jolt PUBLIC
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        -ffp-contract=off
    )
else()
    message(FATAL_ERROR
        "Jolt deterministic floating-point settings are not defined for ${CMAKE_CXX_COMPILER_ID}."
    )
endif()

get_property(this_gem_root GLOBAL PROPERTY "@GEMROOT:${gem_name}@")
ly_get_engine_relative_source_dir(${this_gem_root} relative_this_gem_root)
o3de_fixup_fetchcontent_targets(
    IDE_FOLDER "${relative_this_gem_root}/External"
    TARGETS Jolt
)

FetchContent_GetProperties(JoltPhysics SOURCE_DIR jolt_source_dir)
set_property(TARGET Jolt PROPERTY JOLT_SOURCE_ROOT "${jolt_source_dir}")
get_target_property(Jolt_NATIVE_SOURCES Jolt SOURCES)
set(Jolt_NATIVE_HEADERS)
foreach(jolt_native_source IN LISTS Jolt_NATIVE_SOURCES)
    if(IS_ABSOLUTE "${jolt_native_source}")
        file(RELATIVE_PATH jolt_native_source "${jolt_source_dir}" "${jolt_native_source}")
    endif()
    if(jolt_native_source MATCHES "^Jolt/.+\\.(h|inl)$")
        list(APPEND Jolt_NATIVE_HEADERS "${jolt_native_source}")
    endif()
endforeach()
list(SORT Jolt_NATIVE_HEADERS)
set(Jolt_NATIVE_HEADER_LINES ${Jolt_NATIVE_HEADERS})
list(TRANSFORM Jolt_NATIVE_HEADER_LINES PREPEND "    \"")
list(TRANSFORM Jolt_NATIVE_HEADER_LINES APPEND "\"")
list(JOIN Jolt_NATIVE_HEADER_LINES "\n" Jolt_NATIVE_HEADER_LINES)
include(${this_gem_root}/Code/jolt_api_files.cmake)
foreach(jolt_public_header IN LISTS FILES)
    string(REGEX REPLACE "^Include/" "" jolt_relative_header "${jolt_public_header}")
    if(jolt_relative_header IN_LIST Jolt_NATIVE_HEADERS)
        message(FATAL_ERROR
            "Gem header ${jolt_relative_header} collides with a native Jolt header. "
            "Rename the Gem header before exposing it through Gem::${gem_name}.API."
        )
    endif()
endforeach()
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/Installer/JoltPackage.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/JoltPackage.cmake
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/Installer/JoltNativeHeaders.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/JoltNativeHeaders.cmake
    @ONLY
)
ly_install(FILES ${CMAKE_CURRENT_LIST_DIR}/Installer/FindJolt.cmake DESTINATION cmake/3rdParty)
foreach(jolt_native_header IN LISTS Jolt_NATIVE_HEADERS)
    get_filename_component(jolt_native_header_directory "${jolt_native_header}" DIRECTORY)
    ly_install(
        FILES "${jolt_source_dir}/${jolt_native_header}"
        DESTINATION "include/${jolt_native_header_directory}"
        COMPONENT CORE
    )
endforeach()
ly_install(FILES
    ${jolt_source_dir}/LICENSE
    ${CMAKE_CURRENT_BINARY_DIR}/JoltNativeHeaders.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/JoltPackage.cmake
    DESTINATION include/Jolt
    COMPONENT CORE
)
include(${this_gem_root}/Code/jolt_gpu_hair_shader_files.cmake)
include(${this_gem_root}/Code/jolt_gpu_hair_vulkan_native_files.cmake)
set(jolt_gpu_hair_install_sources
    ${JOLT_GPU_HAIR_SHADER_FILES}
    ${JOLT_GPU_HAIR_VULKAN_NATIVE_FILES}
)
if(PAL_PLATFORM_NAME STREQUAL "Windows")
    include(${this_gem_root}/Code/jolt_gpu_hair_dx12_native_files.cmake)
    list(APPEND jolt_gpu_hair_install_sources ${JOLT_GPU_HAIR_DX12_NATIVE_FILES})
endif()
if(PAL_PLATFORM_NAME STREQUAL "Mac" OR PAL_PLATFORM_NAME STREQUAL "iOS")
    include(${this_gem_root}/Code/jolt_gpu_hair_metal_native_files.cmake)
    list(APPEND jolt_gpu_hair_install_sources ${JOLT_GPU_HAIR_METAL_NATIVE_FILES})
endif()
foreach(jolt_gpu_hair_source IN LISTS jolt_gpu_hair_install_sources)
    get_filename_component(jolt_gpu_hair_source_directory "${jolt_gpu_hair_source}" DIRECTORY)
    ly_install(
        FILES "${jolt_source_dir}/${jolt_gpu_hair_source}"
        DESTINATION "include/Jolt/Source/${jolt_gpu_hair_source_directory}"
        COMPONENT CORE
    )
endforeach()

set(Jolt_FOUND TRUE PARENT_SCOPE)
