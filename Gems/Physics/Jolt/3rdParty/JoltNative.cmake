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

set(jolt_native_content_name O3DEJoltPhysics_5_6_0)
set(jolt_native_owner "Gem::Jolt@5.6.0")

get_property(jolt_native_registered_owner GLOBAL PROPERTY O3DE_JOLT_NATIVE_OWNER)
if(TARGET Jolt)
    get_target_property(jolt_native_target_owner Jolt O3DE_JOLT_NATIVE_OWNER)
    if(NOT jolt_native_registered_owner STREQUAL jolt_native_owner
        OR NOT jolt_native_target_owner STREQUAL jolt_native_owner)
        message(FATAL_ERROR
            "The Jolt Gem found a foreign target named Jolt. The native Jolt target is a private "
            "implementation detail and cannot be supplied or replaced by another project."
        )
    endif()

    return()
endif()

if(jolt_native_registered_owner)
    message(FATAL_ERROR
        "The private Jolt native registration is inconsistent: owner "
        "${jolt_native_registered_owner} is registered without its target."
    )
endif()

string(TOLOWER "${jolt_native_content_name}" jolt_native_content_name_lower)
get_property(
    jolt_native_content_declared
    GLOBAL PROPERTY "_FetchContent_${jolt_native_content_name_lower}_savedDetails"
    DEFINED)
if(jolt_native_content_declared)
    message(FATAL_ERROR
        "The Jolt Gem found a foreign FetchContent declaration for ${jolt_native_content_name}. "
        "The versioned native dependency identity is provider-owned."
    )
endif()

string(TOUPPER "${jolt_native_content_name}" jolt_native_content_name_upper)
if(FETCHCONTENT_SOURCE_DIR_${jolt_native_content_name_upper})
    message(FATAL_ERROR
        "FETCHCONTENT_SOURCE_DIR_${jolt_native_content_name_upper} cannot override the private "
        "patched Jolt source. Use the pinned provider-owned dependency."
    )
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
    o3de_fetch_content(${jolt_native_content_name}
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

    # SSE4.1 is the provider's x86 floor. Later extensions remain disabled so
    # the same binary runs on every supported x86 processor and does not use
    # fused operations that would break cross-platform determinism.
    set(USE_AVX OFF)
    set(USE_AVX2 OFF)
    set(USE_AVX512 OFF)
    set(USE_F16C OFF)
    set(USE_FMADD OFF)
    set(USE_LZCNT OFF)
    set(USE_SSE4_1 ON)
    set(USE_SSE4_2 OFF)
    set(USE_TZCNT OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(USE_WASM_SIMD ON)

    FetchContent_MakeAvailable(${jolt_native_content_name})
endblock()

if(NOT TARGET Jolt)
    message(FATAL_ERROR "JoltPhysics 5.6.0 did not create the required private Jolt target.")
endif()

get_target_property(jolt_native_target_type Jolt TYPE)
if(NOT jolt_native_target_type STREQUAL "OBJECT_LIBRARY")
    message(FATAL_ERROR
        "The private Jolt target must be an object library, but ${jolt_native_target_type} was created."
    )
endif()

set_property(TARGET Jolt PROPERTY O3DE_JOLT_NATIVE_OWNER "${jolt_native_owner}")
set_property(GLOBAL PROPERTY O3DE_JOLT_NATIVE_OWNER "${jolt_native_owner}")

# Jolt 5.6 emits x86 feature definitions for MSVC only when generated by Visual
# Studio. Ninja leaves CMAKE_VS_PLATFORM_NAME empty, so make the private native
# ABI explicit for MSVC/Ninja as well. MSVC x64 does not have or require an
# /arch:SSE4.1 switch.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x86|i.86)$")
    target_compile_definitions(Jolt PUBLIC JPH_USE_SSE4_1)
endif()

# clang-cl inherits -mf16c from the engine's Windows configuration. Clang
# treats F16C as an AVX feature and Jolt then infers JPH_USE_AVX from the
# compiler macros. Override every optional x86 extension after the native
# target is created so the effective code generation matches the requested
# SSE4.1 ABI.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x86|i.86)$"
    AND CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(Jolt PUBLIC
        -mno-avx
        -mno-avx2
        -mno-avx512f
        -mno-f16c
        -mno-fma
        -msse4.1
    )
endif()

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

FetchContent_GetProperties(${jolt_native_content_name} SOURCE_DIR jolt_source_dir)

set_property(
    DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/jolt-5.6.0-o3de.patch")
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/jolt-5.6.0-o3de.patch" jolt_native_patch_hash)
string(CONCAT jolt_native_build_identity
    "content=${jolt_native_content_name};"
    "version=5.6.0;"
    "archive_sha256=6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e;"
    "git=e77f175595e64cb44218cc9d9d56fc365ad0e36a;"
    "patch_sha256=${jolt_native_patch_hash};"
    "compiler=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION};"
    "frontend=${CMAKE_CXX_COMPILER_FRONTEND_VARIANT};"
    "system=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR};"
    "pointer_size=${CMAKE_SIZEOF_VOID_P};"
    "double_precision=${LY_JOLT_DOUBLE_PRECISION};"
    "debug_rendering=${LY_JOLT_ENABLE_DEBUG_RENDERING};"
    "detailed_profiling=${LY_JOLT_ENABLE_DETAILED_PROFILING};"
    "simulation_statistics=${LY_JOLT_ENABLE_SIMULATION_STATISTICS};"
    "broadphase_statistics=${LY_JOLT_ENABLE_BROADPHASE_STATISTICS};"
    "narrowphase_statistics=${LY_JOLT_ENABLE_NARROWPHASE_STATISTICS};"
    "cpu_compute=ON;"
    "object_layer_bits=32;"
    "sse4_1=ON;"
    "sse4_2=OFF;"
    "avx=OFF;"
    "avx2=OFF;"
    "avx512=OFF;"
    "f16c=OFF;"
    "fmadd=OFF;"
    "lzcnt=OFF;"
    "tzcnt=OFF;"
    "cross_platform_deterministic=ON;"
    "rtti=OFF;"
    "exceptions=OFF")
string(SHA256 jolt_native_build_hash "${jolt_native_build_identity}")
string(SUBSTRING "${jolt_native_build_hash}" 0 16 jolt_native_build_fingerprint)
set(
    JOLT_NATIVE_BUILD_FINGERPRINT
    "0x${jolt_native_build_fingerprint}ULL"
    CACHE INTERNAL "Configured private Jolt native build fingerprint" FORCE)
set(
    JOLT_NATIVE_BUILD_IDENTITY
    "${jolt_native_build_identity}"
    CACHE INTERNAL "Configured private Jolt native build identity" FORCE)

target_sources(Jolt PRIVATE "${CMAKE_CURRENT_LIST_DIR}/JoltNativeBuildIdentity.cpp")
target_compile_definitions(
    Jolt PRIVATE
    JOLT_NATIVE_BUILD_FINGERPRINT=${JOLT_NATIVE_BUILD_FINGERPRINT})

# Only the license ships. Jolt's headers and sources are intentionally not installed:
# nothing in an installed engine compiles against them, and withholding them is what
# stops downstream code from including Jolt directly.
ly_install(FILES
    ${jolt_source_dir}/LICENSE
    DESTINATION include/Jolt
    COMPONENT CORE
)
