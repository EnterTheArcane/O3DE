#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# DEPRECATED: This file is replaced by cmake/CompilerLauncher.cmake.
#
# To enable a compiler launcher, set CMAKE_C_COMPILER_LAUNCHER and/or
# CMAKE_CXX_COMPILER_LAUNCHER (via -D flag, environment variable, or CMake
# preset) instead of using O3DE_COMPILER_CACHE.
#
# CompilerLauncher.cmake handles Ninja, Makefile, Xcode, and Visual Studio
# generators automatically.

include_guard(GLOBAL)
message(DEPRECATION
    "cmake/CompilerCache.cmake is deprecated and will be removed in a future release. "
    "Compiler launcher support is now handled by cmake/CompilerLauncher.cmake. "
    "Set CMAKE_C_COMPILER_LAUNCHER and CMAKE_CXX_COMPILER_LAUNCHER instead of O3DE_COMPILER_CACHE."
)
