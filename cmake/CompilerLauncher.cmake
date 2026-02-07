#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# Unified compiler launcher support.
#
# Adapts CMAKE_<LANG>_COMPILER_LAUNCHER for generators that don't natively support it.
# Users simply set CMAKE_C_COMPILER_LAUNCHER and/or CMAKE_CXX_COMPILER_LAUNCHER to a compiler launcher executable
# (e.g. ccache, sccache) and this module handles the rest.
#
# Supported generators:
#   - Ninja / Makefile: Native support via CMAKE_<LANG>_COMPILER_LAUNCHER
#   - Visual Studio: cl.exe wrapper via CMAKE_VS_GLOBALS
#   - Xcode: Wrapper scripts via CMAKE_XCODE_ATTRIBUTE_CC/CXX
#
# Usage:
#   - CMake flag: -DCMAKE_C_COMPILER_LAUNCHER=ccache
#   - Environment variable: CMAKE_C_COMPILER_LAUNCHER=ccache
#   - CMake preset: "cacheVariables": { "CMAKE_C_COMPILER_LAUNCHER": "ccache" }
#
# This file must be included AFTER project() so that CMAKE_C_COMPILER
# and CMAKE_CXX_COMPILER are resolved (needed for Xcode wrapper generation).

include_guard(GLOBAL)

block()
    set(supported_languages C CXX)

    # CMake does not natively initialize CMAKE_<LANG>_COMPILER_LAUNCHER from
    # environment variables, so we do it here as a convenience.
    foreach (cl_lang IN ITEMS ${supported_languages})
        if (NOT DEFINED CMAKE_${cl_lang}_COMPILER_LAUNCHER
            AND DEFINED ENV{CMAKE_${cl_lang}_COMPILER_LAUNCHER}
            AND NOT "$ENV{CMAKE_${cl_lang}_COMPILER_LAUNCHER}" STREQUAL "")
            set(CMAKE_${cl_lang}_COMPILER_LAUNCHER "$ENV{CMAKE_${cl_lang}_COMPILER_LAUNCHER}" CACHE STRING "")
        endif ()
    endforeach ()

    # Early exit if no launcher is configured
    if (NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
        message(VERBOSE "CompilerLauncher: Not configured")
        return()
    endif ()

    # Resolve launcher paths
    # If the launcher value is not an absolute path to an existing file, resolve it via find_program.
    # This handles bare names like "ccache" or "sccache".
    foreach (cl_lang IN ITEMS ${supported_languages})
        if (CMAKE_${cl_lang}_COMPILER_LAUNCHER)
            set(cl_value "${CMAKE_${cl_lang}_COMPILER_LAUNCHER}")

            if (IS_ABSOLUTE "${cl_value}" AND EXISTS "${cl_value}")
                # Already a valid absolute path
                set(cl_${cl_lang}_resolved "${cl_value}")
            else ()
                find_program(cl_found_${cl_lang} "${cl_value}" NO_CACHE)
                if (cl_found_${cl_lang})
                    set(cl_${cl_lang}_resolved "${cl_found_${cl_lang}}")
                elseif (IS_ABSOLUTE "${cl_value}")
                    message(WARNING "CompilerLauncher: ${cl_lang} launcher path does not exist: ${cl_value}")
                else ()
                    # Keep the original value; the generator may still find it
                    set(cl_${cl_lang}_resolved "${cl_value}")
                    message(WARNING "CompilerLauncher: Could not resolve '${cl_value}' to an absolute path")
                endif ()
            endif ()

            if (cl_${cl_lang}_resolved)
                message(STATUS "CompilerLauncher: ${cl_lang} launcher: ${cl_${cl_lang}_resolved}")
            endif ()
        endif ()
    endforeach ()

    # Bail out if no valid launcher was resolved
    foreach (cl_lang IN ITEMS ${supported_languages})
        if (cl_${cl_lang}_resolved)
            set(cl_valid_launcher_found TRUE)
            break()
        endif ()
    endforeach ()

    if (NOT cl_valid_launcher_found)
        message(WARNING "CompilerLauncher: No valid compiler launcher found")
        return()
    endif ()

    # == MAKEFILE / NINJA == #
    if (CMAKE_GENERATOR MATCHES "Makefiles|Ninja|Ninja Multi-Config")
        foreach (cl_lang IN ITEMS ${supported_languages})
            if (cl_${cl_lang}_resolved)
                set(CMAKE_${cl_lang}_COMPILER_LAUNCHER "${cl_${cl_lang}_resolved}" CACHE STRING "" FORCE)
            endif ()
        endforeach ()

        message(STATUS "CompilerLauncher: Configured for ${CMAKE_GENERATOR} (native support)")
        return()
    endif ()

    # == VISUAL STUDIO == #
    # Visual Studio does not support CMAKE_<LANG>_COMPILER_LAUNCHER.
    # Instead, the launcher executable (ccache / sccache) is copied as cl.exe into the build directory,
    # and CMAKE_VS_GLOBALS redirects MSBuild to use it.
    # When invoked as cl.exe, ccache/sccache detect they are wrapping MSVC and proxy the call to the real cl.exe found in PATH.
    # Compiler launchers also require embedded debug information (/Z7 instead of /Zi) to work correctly with MSVC.
    # CMAKE_MSVC_DEBUG_INFORMATION_FORMAT is set here; platform config files should also add /Z7 flags as a fallback.
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        # Prefer the C++ launcher; fall back to C launcher
        set(cl_vs_launcher "${cl_CXX_resolved}")
        if (NOT cl_vs_launcher)
            set(cl_vs_launcher "${cl_C_resolved}")
        endif ()

        if (NOT cl_vs_launcher)
            message(WARNING "CompilerLauncher: No valid launcher for Visual Studio generator")
            return()
        endif ()

        # Copy launcher as cl.exe in the build directory
        file(COPY_FILE "${cl_vs_launcher}" "${CMAKE_BINARY_DIR}/cl.exe" ONLY_IF_DIFFERENT)

        list(APPEND CMAKE_VS_GLOBALS
            "CLToolExe=cl.exe"
            "CLToolPath=${CMAKE_BINARY_DIR}"
            "TrackFileAccess=false"
            "UseMultiToolTask=true"
        )
        set(CMAKE_VS_GLOBALS "${CMAKE_VS_GLOBALS}" PARENT_SCOPE)

        # Embedded debug info is required for compiler launcher compatibility
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded" CACHE STRING "" FORCE)

        message(STATUS "CompilerLauncher: Configured for Visual Studio via cl.exe wrapper")
        return()
    endif ()

    # == XCODE == #
    # Xcode does not support CMAKE_<LANG>_COMPILER_LAUNCHER.
    # Instead, we create thin wrapper scripts that invoke the launcher with the CMake-resolved compiler.
    # This uses CMAKE_C_COMPILER / CMAKE_CXX_COMPILER, so it respects any user overrides.
    # See: https://crascit.com/2016/04/09/using-ccache-with-cmake/
    if (CMAKE_GENERATOR MATCHES "Xcode")
        if (cl_C_resolved AND CMAKE_C_COMPILER)
            set(cl_launch_c "${CMAKE_BINARY_DIR}/launch-c")
            file(WRITE "${cl_launch_c}" "#!/usr/bin/env sh\nexec \"${cl_C_resolved}\" \"${CMAKE_C_COMPILER}\" \"$@\"\n")
            file(CHMOD "${cl_launch_c}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
            set(CMAKE_XCODE_ATTRIBUTE_CC "${cl_launch_c}" CACHE STRING "" FORCE)
            set(CMAKE_XCODE_ATTRIBUTE_LD "${cl_launch_c}" CACHE STRING "" FORCE)
        endif ()

        if (cl_CXX_resolved AND CMAKE_CXX_COMPILER)
            set(cl_launch_cxx "${CMAKE_BINARY_DIR}/launch-cxx")
            file(WRITE "${cl_launch_cxx}" "#!/usr/bin/env sh\nexec \"${cl_CXX_resolved}\" \"${CMAKE_CXX_COMPILER}\" \"$@\"\n")
            file(CHMOD "${cl_launch_cxx}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
            set(CMAKE_XCODE_ATTRIBUTE_CXX "${cl_launch_cxx}" CACHE STRING "" FORCE)
            set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${cl_launch_cxx}" CACHE STRING "" FORCE)
        endif ()

        message(STATUS "CompilerLauncher: Configured for Xcode via wrapper scripts")
        return()
    endif ()

    # == UNKNOWN == #
    # Leave CMAKE_<LANG>_COMPILER_LAUNCHER as-is
    message(STATUS "CompilerLauncher: Unsupported generator: ${CMAKE_GENERATOR}")
endblock()
