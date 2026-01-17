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

if (NOT DEFINED O3DE_COMPILER_CACHE AND DEFINED ENV{O3DE_COMPILER_CACHE})
    set(O3DE_COMPILER_CACHE "$ENV{O3DE_COMPILER_CACHE}")
endif ()

block()
    set(compiler_cache_enabled OFF)
    if (O3DE_COMPILER_CACHE)
        set(compiler_cache_enabled ON)
    endif ()

    set(O3DE_COMPILER_CACHE_ENABLED "${compiler_cache_enabled}" CACHE BOOL "Compiler cache enabled" FORCE)
    message(STATUS "[COMPILER CACHE] Cache is enabled")
endblock()

function(o3de_compiler_cache_activation CACHE_EXE_PATH)
    if (NOT O3DE_COMPILER_CACHE_ENABLED)
        return()
    endif ()

    unset(cache_exe)

    set(_cc_value "${O3DE_COMPILER_CACHE}")
    string(TOUPPER "${_cc_value}" _cc_upper)

    # Helper: whether _cc_value is "auto-enabled"
    set(_cc_auto FALSE)
    if (_cc_upper STREQUAL "1" OR
        _cc_upper STREQUAL "ON" OR
        _cc_upper STREQUAL "TRUE" OR
        _cc_upper STREQUAL "YES")
        set(_cc_auto TRUE)
    endif ()

    # 1) If it looks like a path (contains / or \), treat as path
    if (_cc_value MATCHES "[/\\\\]")
        set(cache_path "${_cc_value}")
        cmake_path(ABSOLUTE_PATH cache_path OUTPUT_VARIABLE cache_path)

        if (IS_DIRECTORY "${cache_path}")
            find_program(cache_exe
                NAMES sccache sccache.exe ccache ccache.exe
                PATHS "${cache_path}" "${cache_path}/bin"
                NO_DEFAULT_PATH
            )
            if (NOT cache_exe)
                message(FATAL_ERROR
                    "[COMPILER CACHE] O3DE_COMPILER_CACHE points to a directory, but no sccache/ccache was found in "
                    "'${cache_path}' or '${cache_path}/bin'"
                )
            endif ()
        else ()
            if (NOT EXISTS "${cache_path}")
                message(FATAL_ERROR "[COMPILER CACHE] O3DE_COMPILER_CACHE path does not exist: ${cache_path}")
            endif ()
            set(cache_exe "${cache_path}")
        endif ()

    else ()
        # 2) Not a path: either auto-detect, or explicit program name
        if (_cc_auto)
            # Auto: prefer sccache then ccache
            find_program(cache_exe NAMES sccache sccache.exe)
            if (NOT cache_exe)
                find_program(cache_exe NAMES ccache ccache.exe)
            endif ()
        else ()
            # Explicit program name (e.g. "sccache" or "ccache" or "mycache")
            find_program(cache_exe NAMES "${_cc_value}" "${_cc_value}.exe")
        endif ()

        if (NOT cache_exe)
            if (_cc_auto)
                message(FATAL_ERROR
                    "[COMPILER CACHE] Cache is enabled, but neither sccache nor ccache was found on PATH. "
                    "Install one or set O3DE_COMPILER_CACHE to a path (file or directory)."
                )
            else ()
                message(FATAL_ERROR
                    "[COMPILER CACHE] O3DE_COMPILER_CACHE is set to '${_cc_value}', but it could not be found on PATH. "
                    "Either install it and add to PATH, or set O3DE_COMPILER_CACHE to a path (file or directory)."
                )
            endif ()
        endif ()
    endif ()

    cmake_path(ABSOLUTE_PATH cache_exe OUTPUT_VARIABLE cache_exe)

    set(O3DE_COMPILER_CACHE_PATH "${cache_exe}" CACHE FILEPATH "Resolved path to compiler cache program" FORCE)
    message(STATUS "[COMPILER CACHE] Found at ${cache_exe}, using it for this build")

    if (CMAKE_GENERATOR MATCHES "^Ninja.*")
        set(${CACHE_EXE_PATH} "${cache_exe}" PARENT_SCOPE)
    else ()
        set(_copied_cache_exe "${CMAKE_BINARY_DIR}/cl.exe")
        file(COPY_FILE "${cache_exe}" "${_copied_cache_exe}" ONLY_IF_DIFFERENT)
        set(${CACHE_EXE_PATH} "${_copied_cache_exe}" PARENT_SCOPE)
    endif ()
endfunction()
