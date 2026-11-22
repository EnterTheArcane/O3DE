#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 3.25)
project(NvCloth LANGUAGES CXX)

set(PROJECT_ROOT_DIR "${NVCLOTH_SOURCE_DIR}/NvCloth/")
set(PXSHARED_ROOT_DIR "${NVCLOTH_SOURCE_DIR}/PxShared")
set(NVCLOTH_LIBTYPE STATIC)
set(NVCLOTH_COMPILE_DEFS
    NV_CLOTH_IMPORT= PX_CALL_CONV= NV_CLOTH_ENABLE_CUDA=0 NV_CLOTH_ENABLE_DX11=0
    "$<$<CONFIG:Debug>:_DEBUG;PX_DEBUG=1;PX_CHECKED=1>"
    "$<$<CONFIG:RelWithDebInfo>:PX_PROFILE=1>")

if(WIN32)
    set(NVCLOTH_PLATFORM_SOURCE_FILES "${PROJECT_ROOT_DIR}src/ps/windows/PsWindowsAtomic.cpp")
    if(NVCLOTH_TARGET_ARCH STREQUAL "x86_64")
        set(avx_source "${PROJECT_ROOT_DIR}src/avx/SwSolveConstraints.cpp")
        list(APPEND NVCLOTH_PLATFORM_SOURCE_FILES "${avx_source}")
        set_source_files_properties("${avx_source}" PROPERTIES COMPILE_OPTIONS "/arch:AVX")
    endif()
else()
    set(NVCLOTH_PLATFORM_SOURCE_FILES "${PROJECT_ROOT_DIR}src/ps/unix/PsUnixAtomic.cpp")
    if(NVCLOTH_TARGET_ARCH STREQUAL "armv8")
        list(APPEND NVCLOTH_PLATFORM_SOURCE_FILES
            "${PROJECT_ROOT_DIR}src/neon/NeonCollision.cpp"
            "${PROJECT_ROOT_DIR}src/neon/NeonSelfCollision.cpp"
            "${PROJECT_ROOT_DIR}src/neon/NeonSolverKernel.cpp")
    endif()
endif()

include("${PROJECT_ROOT_DIR}compiler/cmake/common/NvCloth.cmake")
set_target_properties(NvCloth PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(NvCloth PRIVATE /GR-)
else()
    target_compile_options(NvCloth PRIVATE -fno-rtti -fno-exceptions)
endif()
