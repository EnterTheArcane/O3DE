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

block()
    set(_o3de_compiler_cache "off")
    if (DEFINED ENV{O3DE_COMPILER_CACHE} AND NOT "$ENV{O3DE_COMPILER_CACHE}" STREQUAL "")
        set(_o3de_compiler_cache "$ENV{O3DE_COMPILER_CACHE}")
    endif ()

    set(O3DE_COMPILER_CACHE "${_o3de_compiler_cache}" CACHE STRING "Compiler cache to use (on, ccache, sccache, off)")
    set_property(CACHE O3DE_COMPILER_CACHE PROPERTY STRINGS "on" "ccache" "sccache" "off")
    string(TOLOWER "${O3DE_COMPILER_CACHE}" _o3de_compiler_cache)

    if (_o3de_compiler_cache STREQUAL "off")
        message(VERBOSE "CompilerCache: Disabled")
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
            message(STATUS "CompilerCache: Not found (ccache/sccache not in PATH)")
        endif ()
        return()
    endif ()

    message(STATUS "CompilerCache: ${_compiler_cache_name} (${_compiler_cache_executable})")

    if (CMAKE_GENERATOR MATCHES "Xcode")
        # Xcode generator does not support CMAKE_<LANG>_COMPILER_LAUNCHER.
        # Use CMAKE_XCODE_ATTRIBUTE_CC/CXX with wrapper scripts instead.
        # See: https://crascit.com/2016/04/09/using-ccache-with-cmake/

        set(_wrapper_dir "${CMAKE_BINARY_DIR}/bin")
        file(MAKE_DIRECTORY "${_wrapper_dir}")

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

        set(_launch_c "${_wrapper_dir}/launch-c")
        file(WRITE "${_launch_c}" "#!/usr/bin/env sh\nexec \"${_compiler_cache_executable}\" \"${_c_compiler}\" \"$@\"\n")
        file(CHMOD "${_launch_c}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

        set(_launch_cxx "${_wrapper_dir}/launch-cxx")
        file(WRITE "${_launch_cxx}" "#!/usr/bin/env sh\nexec \"${_compiler_cache_executable}\" \"${_cxx_compiler}\" \"$@\"\n")
        file(CHMOD "${_launch_cxx}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

        set(CMAKE_XCODE_ATTRIBUTE_CC "${_launch_c}" CACHE STRING "Xcode C compiler wrapper with ${_compiler_cache_name}")
        set(CMAKE_XCODE_ATTRIBUTE_CXX "${_launch_cxx}" CACHE STRING "Xcode C++ compiler wrapper with ${_compiler_cache_name}")
        set(CMAKE_XCODE_ATTRIBUTE_LD "${_launch_c}" CACHE STRING "Xcode linker wrapper with ${_compiler_cache_name}")
        set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${_launch_cxx}" CACHE STRING "Xcode C++ linker wrapper with ${_compiler_cache_name}")

        message(STATUS "CompilerCache: Configured for Xcode via wrapper scripts")
    else ()
        set(CMAKE_C_COMPILER_LAUNCHER "${_compiler_cache_executable}" CACHE STRING "C compiler launcher")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${_compiler_cache_executable}" CACHE STRING "C++ compiler launcher")

        message(STATUS "CompilerCache: Configured via CMAKE_COMPILER_LAUNCHER")
    endif ()
endblock()
