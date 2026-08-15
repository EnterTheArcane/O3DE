#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(TARGET 3rdParty::Jolt)
    return()
endif()

include("${LY_ROOT_FOLDER}/include/Jolt/JoltPackage.cmake")

if(DEFINED LY_JOLT_DOUBLE_PRECISION
    AND NOT LY_JOLT_DOUBLE_PRECISION STREQUAL Jolt_PACKAGE_DOUBLE_PRECISION)
    message(FATAL_ERROR
        "LY_JOLT_DOUBLE_PRECISION=${LY_JOLT_DOUBLE_PRECISION} does not match the installed Jolt library "
        "(${Jolt_PACKAGE_DOUBLE_PRECISION})."
    )
endif()
if(DEFINED LY_JOLT_ENABLE_DEBUG_RENDERING
    AND NOT LY_JOLT_ENABLE_DEBUG_RENDERING STREQUAL Jolt_PACKAGE_DEBUG_RENDERING)
    message(FATAL_ERROR
        "LY_JOLT_ENABLE_DEBUG_RENDERING=${LY_JOLT_ENABLE_DEBUG_RENDERING} does not match the installed Jolt library "
        "(${Jolt_PACKAGE_DEBUG_RENDERING})."
    )
endif()
if(DEFINED LY_JOLT_ENABLE_DETAILED_PROFILING
    AND NOT LY_JOLT_ENABLE_DETAILED_PROFILING STREQUAL Jolt_PACKAGE_DETAILED_PROFILING)
    message(FATAL_ERROR
        "LY_JOLT_ENABLE_DETAILED_PROFILING=${LY_JOLT_ENABLE_DETAILED_PROFILING} does not match the installed Jolt library "
        "(${Jolt_PACKAGE_DETAILED_PROFILING})."
    )
endif()
if(DEFINED LY_JOLT_ENABLE_SIMULATION_STATISTICS
    AND NOT LY_JOLT_ENABLE_SIMULATION_STATISTICS STREQUAL Jolt_PACKAGE_SIMULATION_STATISTICS)
    message(FATAL_ERROR
        "LY_JOLT_ENABLE_SIMULATION_STATISTICS=${LY_JOLT_ENABLE_SIMULATION_STATISTICS} does not match the installed Jolt library "
        "(${Jolt_PACKAGE_SIMULATION_STATISTICS})."
    )
endif()
if(DEFINED LY_JOLT_SIMD_LEVEL
    AND NOT LY_JOLT_SIMD_LEVEL STREQUAL Jolt_PACKAGE_SIMD_LEVEL)
    message(FATAL_ERROR
        "LY_JOLT_SIMD_LEVEL=${LY_JOLT_SIMD_LEVEL} does not match the installed Jolt library "
        "(${Jolt_PACKAGE_SIMD_LEVEL})."
    )
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL Jolt_PACKAGE_CXX_COMPILER_ID
    OR NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL Jolt_PACKAGE_CXX_COMPILER_VERSION)
    message(FATAL_ERROR
        "Jolt was built with ${Jolt_PACKAGE_CXX_COMPILER_ID} ${Jolt_PACKAGE_CXX_COMPILER_VERSION}, but the current "
        "toolchain is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}. Release IPO archives require the installed "
        "engine toolchain."
    )
endif()

set(LY_JOLT_DOUBLE_PRECISION ${Jolt_PACKAGE_DOUBLE_PRECISION})
set(LY_JOLT_ENABLE_DEBUG_RENDERING ${Jolt_PACKAGE_DEBUG_RENDERING})
set(LY_JOLT_ENABLE_DETAILED_PROFILING ${Jolt_PACKAGE_DETAILED_PROFILING})
set(LY_JOLT_ENABLE_SIMULATION_STATISTICS ${Jolt_PACKAGE_SIMULATION_STATISTICS})
set(LY_JOLT_SIMD_LEVEL ${Jolt_PACKAGE_SIMD_LEVEL})
set(jolt_library_folder "${LY_ROOT_FOLDER}/lib/${PAL_PLATFORM_NAME}")

add_library(JoltNative STATIC IMPORTED GLOBAL)
set_target_properties(JoltNative PROPERTIES
    IMPORTED_LOCATION "${jolt_library_folder}/profile/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_DEBUG "${jolt_library_folder}/debug/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_RELEASE "${jolt_library_folder}/release/${CMAKE_STATIC_LIBRARY_PREFIX}Jolt${CMAKE_STATIC_LIBRARY_SUFFIX}"
    JOLT_PATCH_HASH "${Jolt_PACKAGE_PATCH_HASH}"
    JOLT_PATCH_REVISION "${Jolt_PACKAGE_PATCH_REVISION}"
    JOLT_SOURCE_REVISION "${Jolt_PACKAGE_COMMIT}"
    JOLT_SOURCE_ROOT "${LY_ROOT_FOLDER}/include/Jolt/Source"
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_options(JoltNative INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
    )
else()
    target_link_options(JoltNative INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_CXX_COMPILE_OPTIONS_IPO}>"
        "$<$<CONFIG:Release>:${CMAKE_CXX_LINK_OPTIONS_IPO}>"
    )
endif()
ly_target_include_system_directories(TARGET JoltNative INTERFACE "${LY_ROOT_FOLDER}/include")
get_property(this_gem_root GLOBAL PROPERTY "@GEMROOT:${gem_name}@")
include("${LY_ROOT_FOLDER}/include/Jolt/JoltNativeHeaders.cmake")
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(JoltNative INTERFACE ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH})
elseif(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(JoltNative INTERFACE
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        /clang:-ffp-contract=off
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(JoltNative INTERFACE
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        -ffp-contract=off
    )
else()
    message(FATAL_ERROR
        "Jolt deterministic floating-point settings are not defined for ${CMAKE_CXX_COMPILER_ID}."
    )
endif()

target_compile_definitions(JoltNative INTERFACE
    JPH_CROSS_PLATFORM_DETERMINISTIC
    JPH_OBJECT_LAYER_BITS=32
    JPH_OBJECT_STREAM
    JPH_USE_CPU_COMPUTE
)
if(Jolt_PACKAGE_DOUBLE_PRECISION)
    target_compile_definitions(JoltNative INTERFACE JPH_DOUBLE_PRECISION)
endif()
if(Jolt_PACKAGE_DEBUG_RENDERING)
    target_compile_definitions(JoltNative INTERFACE JPH_DEBUG_RENDERER)
endif()
if(Jolt_PACKAGE_DETAILED_PROFILING)
    target_compile_definitions(JoltNative INTERFACE JPH_EXTERNAL_PROFILE)
endif()
if(Jolt_PACKAGE_SIMULATION_STATISTICS)
    target_compile_definitions(JoltNative INTERFACE JPH_TRACK_SIMULATION_STATS)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x86|i[3-6]86)$"
    OR CMAKE_VS_PLATFORM_NAME MATCHES "^(Win32|x64)$")
    if(Jolt_PACKAGE_SIMD_LEVEL MATCHES "^(SSE41|SSE42|AVX|AVX2|AVX512)$")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_SSE4_1)
    endif()
    if(Jolt_PACKAGE_SIMD_LEVEL MATCHES "^(SSE42|AVX|AVX2|AVX512)$")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_SSE4_2)
    endif()
    if(Jolt_PACKAGE_SIMD_LEVEL MATCHES "^(AVX2|AVX512)$")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_LZCNT JPH_USE_TZCNT)
    endif()
    if(Jolt_PACKAGE_SIMD_LEVEL MATCHES "^(AVX|AVX2|AVX512)$")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_AVX)
    endif()
    if(Jolt_PACKAGE_SIMD_LEVEL MATCHES "^(AVX2|AVX512)$")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_AVX2 JPH_USE_F16C)
    endif()
    if(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX512")
        target_compile_definitions(JoltNative INTERFACE JPH_USE_AVX512)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX512")
            target_compile_options(JoltNative INTERFACE /arch:AVX512)
        elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX2")
            target_compile_options(JoltNative INTERFACE /arch:AVX2)
        elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX")
            target_compile_options(JoltNative INTERFACE /arch:AVX)
        endif()
    elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX512")
        target_compile_options(
            JoltNative
            INTERFACE
            -mavx512f
            -mavx512vl
            -mavx512dq
            -mavx2
            -mbmi
            -mpopcnt
            -mlzcnt
            -mf16c)
    elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX2")
        target_compile_options(JoltNative INTERFACE -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c)
    elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "AVX")
        target_compile_options(JoltNative INTERFACE -mavx -mpopcnt)
    elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "SSE42")
        target_compile_options(JoltNative INTERFACE -msse4.2 -mpopcnt)
    elseif(Jolt_PACKAGE_SIMD_LEVEL STREQUAL "SSE41")
        target_compile_options(JoltNative INTERFACE -msse4.1)
    else()
        target_compile_options(JoltNative INTERFACE -msse2)
    endif()
elseif(EMSCRIPTEN)
    target_compile_options(JoltNative INTERFACE -msimd128 -msse4.2)
endif()

add_library(3rdParty::Jolt ALIAS JoltNative)
set(Jolt_FOUND TRUE)
