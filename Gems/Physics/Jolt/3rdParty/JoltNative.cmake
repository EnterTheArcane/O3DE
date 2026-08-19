#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

# Builds the native Jolt static library for this Gem.
#
# This is deliberately NOT a find module and Jolt is deliberately NOT published as
# 3rdParty::Jolt. The library is an implementation detail of the Jolt Gem: its ABI
# depends on the JPH_* macros it was compiled with, so anything that links it must
# be compiled with exactly the same configuration. Consumers go through
# Gem::${gem_name}.API instead, which exposes no Jolt types or headers.
#
# Keeping Jolt out of the 3rdParty:: namespace also keeps it out of
# ly_parse_third_party_dependencies, so no find_package() is ever issued for it and
# no find module has to be shipped to installed engines.

if(TARGET Jolt)
    return()
endif()

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
option(
    LY_JOLT_ENABLE_BROADPHASE_STATISTICS
    "Collect native broadphase query counters"
    OFF)
option(
    LY_JOLT_ENABLE_NARROWPHASE_STATISTICS
    "Collect native narrowphase query counters"
    OFF)
block()
    o3de_fetch_content(JoltPhysics
        VERSION "v5.6.0"
        LICENSE "MIT"
        URL "https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.6.0.tar.gz"
        URL_HASH "6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e"
        GIT "https://github.com/jrouwe/JoltPhysics.git"
        GIT_HASH "e77f175595e64cb44218cc9d9d56fc365ad0e36a"
        PATCH_FILES "${CMAKE_CURRENT_LIST_DIR}/jolt-5.6.0-o3de.patch"
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
    set(JPH_BUILD_OBJECT_LIBS ON)
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
    set(TRACK_BROADPHASE_STATS ${LY_JOLT_ENABLE_BROADPHASE_STATISTICS})
    set(TRACK_NARROWPHASE_STATS ${LY_JOLT_ENABLE_NARROWPHASE_STATISTICS})
    set(USE_ASSERTS OFF)

    # Jolt uses its SSE2 path when every optional x86 extension is disabled,
    # matching the engine's portable x64 baseline.
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

    FetchContent_MakeAvailable(JoltPhysics)
endblock()

set_property(TARGET Jolt PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(Jolt PUBLIC ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH})
elseif(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Jolt PUBLIC
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        /clang:-ffp-contract=off
        -Wno-overriding-option
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
set_property(TARGET Jolt PROPERTY FOLDER "${relative_this_gem_root}/External")
target_compile_options(Jolt ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

FetchContent_GetProperties(JoltPhysics SOURCE_DIR jolt_source_dir)

# Only the license ships. Jolt's headers and sources are intentionally not installed:
# nothing in an installed engine compiles against them, and withholding them is what
# stops downstream code from including Jolt directly.
ly_install(FILES
    ${jolt_source_dir}/LICENSE
    DESTINATION include/Jolt
    COMPONENT CORE
)
