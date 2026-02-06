#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# This module supports multiple CMake generators:
# - For Ninja and Makefile generators: Uses CMAKE_<LANG>_COMPILER_LAUNCHER
# - For Xcode generator: Uses CMAKE_XCODE_ATTRIBUTE_CC/CXX with wrapper scripts
#
# Usage:
#   Set O3DE_COMPILER_CACHE to one of: on, ccache, sccache, or off
#   - on: Automatically detect and use ccache or sccache if available
#   - ccache: Use ccache (fail if not found)
#   - sccache: Use sccache (fail if not found)
#   - off: Disable compiler caching (default)
#
# This file must be included BEFORE project() for Xcode generator support.

include_guard(GLOBAL)

set(_o3de_compiler_cache "off")
if (DEFINED ENV{O3DE_COMPILER_CACHE} AND NOT "$ENV{O3DE_COMPILER_CACHE}" STREQUAL "")
    set(_o3de_compiler_cache "$ENV{O3DE_COMPILER_CACHE}")
endif ()

set(O3DE_COMPILER_CACHE "${_o3de_compiler_cache}" CACHE STRING "Compiler cache to use (on, ccache, sccache, off)")
set_property(CACHE O3DE_COMPILER_CACHE PROPERTY STRINGS "on" "ccache" "sccache" "off")

# Allow case-insensitive values (e.g. CCACHE, ccache, On, OFF)
string(TOLOWER "${O3DE_COMPILER_CACHE}" _o3de_compiler_cache)

if (_o3de_compiler_cache STREQUAL "off")
    message(STATUS "Compiler cache: Disabled")
    return()
endif ()

# Find the cache executable
set(_compiler_cache_executable)
set(_compiler_cache_name)

if (_o3de_compiler_cache STREQUAL "ccache" OR _o3de_compiler_cache STREQUAL "on")
    find_program(_ccache_executable ccache)
    if (_ccache_executable)
        set(_compiler_cache_executable "${_ccache_executable}")
        set(_compiler_cache_name "ccache")
    elseif (_o3de_compiler_cache STREQUAL "ccache")
        message(FATAL_ERROR "O3DE_COMPILER_CACHE is set to ccache but ccache was not found")
    endif ()
endif ()

if (NOT _compiler_cache_executable AND (_o3de_compiler_cache STREQUAL "sccache" OR _o3de_compiler_cache STREQUAL "on"))
    find_program(_sccache_executable sccache)
    if (_sccache_executable)
        set(_compiler_cache_executable "${_sccache_executable}")
        set(_compiler_cache_name "sccache")
    elseif (_o3de_compiler_cache STREQUAL "sccache")
        message(FATAL_ERROR "O3DE_COMPILER_CACHE is set to sccache but sccache was not found")
    endif ()
endif ()

if (NOT _compiler_cache_executable)
    if (_o3de_compiler_cache STREQUAL "on")
        message(STATUS "Compiler cache: Not found (ccache/sccache not in PATH)")
    endif ()
    return()
endif ()

message(STATUS "Compiler cache: ${_compiler_cache_name} (${_compiler_cache_executable})")

# Configure based on generator type
if (CMAKE_GENERATOR MATCHES "Xcode")
    # Xcode generator does not support CMAKE_<LANG>_COMPILER_LAUNCHER.
    # Use CMAKE_XCODE_ATTRIBUTE_CC/CXX with wrapper scripts instead.
    # See: https://crascit.com/2016/04/09/using-ccache-with-cmake/

    # Create wrapper scripts in the build directory
    set(_wrapper_dir "${CMAKE_BINARY_DIR}/_compiler_cache_wrappers")
    file(MAKE_DIRECTORY "${_wrapper_dir}")

    # Get the actual compiler paths - these will be resolved after project() is called
    # For now, use xcrun to find the compilers from the current Xcode installation
    execute_process(
        COMMAND xcrun -f clang
        OUTPUT_VARIABLE _c_compiler
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _xcrun_result
    )
    if (NOT _xcrun_result EQUAL 0)
        message(WARNING "CompilerCache: Failed to find clang via xcrun, disabling compiler cache for Xcode")
        return()
    endif ()

    execute_process(
        COMMAND xcrun -f clang++
        OUTPUT_VARIABLE _cxx_compiler
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _xcrun_result
    )
    if (NOT _xcrun_result EQUAL 0)
        message(WARNING "CompilerCache: Failed to find clang++ via xcrun, disabling compiler cache for Xcode")
        return()
    endif ()

    # Create C compiler wrapper
    set(_launch_c "${_wrapper_dir}/launch-c")
    file(WRITE "${_launch_c}"
        "#!/usr/bin/env sh
exec \"${_compiler_cache_executable}\" \"${_c_compiler}\" \"$@\"
")
    file(CHMOD "${_launch_c}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    # Create C++ compiler wrapper
    set(_launch_cxx "${_wrapper_dir}/launch-cxx")
    file(WRITE "${_launch_cxx}"
        "#!/usr/bin/env sh
exec \"${_compiler_cache_executable}\" \"${_cxx_compiler}\" \"$@\"
")
    file(CHMOD "${_launch_cxx}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    # Configure Xcode to use the wrappers
    # These must be set before the first project() call to take effect
    set(CMAKE_XCODE_ATTRIBUTE_CC "${_launch_c}" CACHE STRING "Xcode C compiler wrapper with ${_compiler_cache_name}")
    set(CMAKE_XCODE_ATTRIBUTE_CXX "${_launch_cxx}" CACHE STRING "Xcode C++ compiler wrapper with ${_compiler_cache_name}")
    set(CMAKE_XCODE_ATTRIBUTE_LD "${_launch_c}" CACHE STRING "Xcode linker wrapper with ${_compiler_cache_name}")
    set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${_launch_cxx}" CACHE STRING "Xcode C++ linker wrapper with ${_compiler_cache_name}")

    message(STATUS "Compiler cache: Configured for Xcode via wrapper scripts")
else ()
    # Ninja and Makefile generators support CMAKE_<LANG>_COMPILER_LAUNCHER directly
    set(CMAKE_C_COMPILER_LAUNCHER "${_compiler_cache_executable}" CACHE STRING "C compiler launcher")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_compiler_cache_executable}" CACHE STRING "C++ compiler launcher")

    message(STATUS "Compiler cache: Configured via CMAKE_COMPILER_LAUNCHER")
endif ()

unset(_compiler_cache_executable)
unset(_compiler_cache_name)
unset(_ccache_executable)
unset(_sccache_executable)
unset(_o3de_compiler_cache)
