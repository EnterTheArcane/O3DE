#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

#
# o3de_compiler_cache_activation activates compiler caching support for O3DE builds using MSVC.
# This supports ccache or sccache and can significantly speed up build times by caching compilation
# results and reusing them when possible, but only under certain conditions:
# 1. /Z7 (embedded debug) must be set (or, for CMake >= 3.30, set policy CMP0141 to NEW and use
#    CMAKE_MSVC_DEBUG_INFORMATION_FORMAT)
# 2. TrackFileAccess should be disabled or the cache folder has to be placed in %TMP% or %APPDATA%
#
# Compiler flag examples:
# https://github.com/ccache/ccache/wiki/MS-Visual-Studio
# https://github.com/mozilla/sccache?tab=readme-ov-file#usage
#
# - To enable compiler caching, set O3DE_COMPILER_CACHE to one of:
#   1. A truthy value (ON, TRUE, 1, YES, ENABLED)
#      - CMake will try to find sccache or ccache on PATH (prefers sccache, then ccache)
#   2. A program name (sccache or ccache, or another wrapper name)
#      - CMake will try to find that program on PATH
#   3. A path (absolute or relative) to:
#      - the cache program
#      - a directory containing ccache or sccache
#      - a directory containing a bin/ subdirectory with ccache or sccache
#
# - If O3DE_COMPILER_CACHE is unset or set to a falsy value (OFF, FALSE, 0, NO, DISABLED),
#   compiler caching is not used.
#
# - O3DE_COMPILER_CACHE can be set via:
#   - CMake variable: -DO3DE_COMPILER_CACHE=<value>
#   - Environment variable: O3DE_COMPILER_CACHE=<value>
#   - CMake variable takes precedence over environment variable
#
# - Once resolved, O3DE_COMPILER_CACHE_PATH is populated in the CMake cache for visibility
#   (path to the resolved executable or directory).
#

if(NOT DEFINED O3DE_COMPILER_CACHE AND DEFINED ENV{O3DE_COMPILER_CACHE})
    set(O3DE_COMPILER_CACHE "$ENV{O3DE_COMPILER_CACHE}")
endif()

set(O3DE_COMPILER_CACHE_ENABLED "${O3DE_COMPILER_CACHE}" CACHE BOOL "Enable compiler cache" FORCE)

function(o3de_compiler_cache_activation CACHE_EXE_PATH)
    if (NOT O3DE_COMPILER_CACHE)
        return()
    endif ()

    message(STATUS "[COMPILER CACHE] Cache is enabled")

    # Determine how to interpret O3DE_COMPILER_CACHE:
    # - If it looks like a path (contains / or \), treat it as a path (file or directory).
    # - Else if truthy, try sccache then ccache via PATH.
    # - Else treat it as a program name and find it on PATH.
    if (O3DE_COMPILER_CACHE MATCHES "[/\\\\]")
        set(cache_path "${O3DE_COMPILER_CACHE}")
    elseif (O3DE_COMPILER_CACHE)
        find_program(found_cache_exe NAMES sccache ccache)
        if (found_cache_exe)
            set(cache_path "${found_cache_exe}")
        else ()
            message(FATAL_ERROR "[COMPILER CACHE] Compiler cache is enabled, but neither sccache nor ccache was found on PATH")
        endif ()
    else ()
        find_program(found_cache_exe "${O3DE_COMPILER_CACHE}")
        if (found_cache_exe)
            set(cache_path "${found_cache_exe}")
        else ()
            message(FATAL_ERROR "[COMPILER CACHE] O3DE_COMPILER_CACHE is set to '${O3DE_COMPILER_CACHE}', but it could not be found on PATH")
        endif ()
    endif ()

    cmake_path(ABSOLUTE_PATH cache_path OUTPUT_VARIABLE cache_path)

    if (NOT EXISTS "${cache_path}")
        message(FATAL_ERROR "[COMPILER CACHE] Path does not exist: ${cache_path}")
    endif ()

    # Resolve executable:
    # - If cache_path is a directory, search in that directory (and common subdirs) for sccache/ccache.
    # - If cache_path is a file, use it directly.
    if (IS_DIRECTORY "${cache_path}")
        find_program(cache_exe
            NAMES sccache ccache
            PATHS "${cache_path}" "${cache_path}/bin"
            NO_DEFAULT_PATH
        )

        if (NOT cache_exe)
            message(FATAL_ERROR "[COMPILER CACHE] Could not find sccache or ccache in directory: ${cache_path}")
        endif ()
    else ()
        set(cache_exe "${cache_path}")
    endif ()

    set(O3DE_COMPILER_CACHE_PATH "${cache_exe}" CACHE FILEPATH "Resolved path to compiler cache program" FORCE)

    message(STATUS "[COMPILER CACHE] Found at ${cache_exe}, using it for this build")

    if (CMAKE_GENERATOR MATCHES "^Ninja.*") # "Ninja" or "Ninja Multi-Config"
        set(${CACHE_EXE_PATH} "${cache_exe}" PARENT_SCOPE)
    else ()
        set(${CACHE_EXE_PATH} "${CMAKE_BINARY_DIR}/cl.exe" PARENT_SCOPE)
        file(COPY_FILE "${cache_exe}" "${CACHE_EXE_PATH}" ONLY_IF_DIFFERENT)
    endif ()
endfunction()
