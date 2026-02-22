#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# C++20 Modules support for O3DE
#
# This provides opt-in support for building O3DE libraries as C++20 named modules.
# When enabled, module interface files (.ixx) are compiled and consumers can use
# `import AzCore;` instead of individual #include directives.
#
# This follows the same pattern used by the Microsoft STL:
#   - A single .ixx file includes all relevant public headers
#   - An AZ_EXPORT macro (defined in PlatformDef.h) conditionally expands to
#     'export' when AZ_BUILD_CXX_MODULE is defined during module compilation
#   - Full backwards compatibility: without O3DE_CXX_MODULES, headers work normally
#
# Integration:
#   Pass INTERFACE_FILE to ly_add_target() to attach a module interface (.ixx)
#   directly to an existing target.  Consumers that link against the target can
#   then use `import <ModuleName>;` in place of #include directives.
#
# Prerequisites:
#   - CMake 3.28+ (for C++20 module scanning support, 4.x recommended)
#   - Clang 16+ or MSVC 17.4+ or GCC 14+
#   - Ninja 1.11+ (for Ninja generator)
#
# Usage:
#   cmake -B Build/Modules -G Ninja -DLY_UNITY_BUILD=OFF -DO3DE_CXX_MODULES=ON

option(O3DE_CXX_MODULES "Enable C++20 named module support for O3DE core libraries" OFF)

if(O3DE_CXX_MODULES)
    message(STATUS "C++20 Modules: ENABLED")

    # Ensure the C++ standard supports modules
    if(CMAKE_CXX_STANDARD LESS 20)
        message(FATAL_ERROR "O3DE_CXX_MODULES requires CMAKE_CXX_STANDARD >= 20 (current: ${CMAKE_CXX_STANDARD})")
    endif()

    # Enable module dependency scanning globally
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)

    # Log compiler information relevant to modules support
    message(STATUS "  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "  CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
            message(WARNING "Clang 16+ is recommended for C++20 modules. Current: ${CMAKE_CXX_COMPILER_VERSION}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.4")
            message(WARNING "MSVC 17.4+ is recommended for C++20 modules. Current: ${CMAKE_CXX_COMPILER_VERSION}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
            message(WARNING "GCC 14+ is recommended for C++20 modules. Current: ${CMAKE_CXX_COMPILER_VERSION}")
        endif()
    endif()

    # Unity builds and C++20 modules don't mix well — warn if both are enabled
    if(LY_UNITY_BUILD)
        message(FATAL_ERROR
            "O3DE_CXX_MODULES is ON but LY_UNITY_BUILD is also ON. "
            "Unity builds are not compatible with C++20 modules. "
            "Pass -DLY_UNITY_BUILD=OFF for best results.")
    endif()
else()
    # When modules are off, make sure the scanner doesn't run unnecessarily
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()
