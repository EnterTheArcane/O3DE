#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# DEPRECATED: This file is replaced by cmake/CompilerLauncher.cmake.
#
# To enable a compiler launcher for MSVC / Visual Studio builds, set
# CMAKE_C_COMPILER_LAUNCHER and CMAKE_CXX_COMPILER_LAUNCHER (via -D flag,
# environment variable, or CMake preset) instead of using
# O3DE_ENABLE_COMPILER_CACHE and O3DE_COMPILER_CACHE_PATH.
#
# CompilerLauncher.cmake handles the cl.exe wrapper and CMAKE_VS_GLOBALS
# setup automatically.

include_guard(GLOBAL)
message(DEPRECATION
    "cmake/Platform/Common/MSVC/CompilerCache_msvc.cmake is deprecated and will be removed in a future release. "
    "Compiler launcher support is now handled by cmake/CompilerLauncher.cmake. "
    "Set CMAKE_C_COMPILER_LAUNCHER and CMAKE_CXX_COMPILER_LAUNCHER instead of "
    "O3DE_ENABLE_COMPILER_CACHE / O3DE_COMPILER_CACHE_PATH."
)

# Provide a no-op stub so existing code that calls this function won't fail
function(o3de_compiler_cache_activation CACHE_EXE_PATH)
    message(DEPRECATION "o3de_compiler_cache_activation() is deprecated. Use cmake/CompilerLauncher.cmake instead.")
endfunction()
