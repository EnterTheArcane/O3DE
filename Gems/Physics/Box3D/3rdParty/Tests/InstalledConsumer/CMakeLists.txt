#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 3.25)
project(Box3DInstalledConsumer LANGUAGES C)

if(NOT LY_ROOT_FOLDER)
    message(FATAL_ERROR "LY_ROOT_FOLDER must identify the staged installed engine.")
endif()

if(NOT PAL_PLATFORM_NAME)
    message(FATAL_ERROR "PAL_PLATFORM_NAME must identify the installed library folder.")
endif()

function(ly_target_include_system_directories)
    cmake_parse_arguments(include "" "TARGET" "INTERFACE;PUBLIC;PRIVATE" ${ARGN})
    foreach(scope IN ITEMS INTERFACE PUBLIC PRIVATE)
        if(include_${scope})
            target_include_directories(${include_TARGET} SYSTEM ${scope} ${include_${scope}})
        endif()
    endforeach()
endfunction()

if(MSVC)
    set(O3DE_COMPILE_OPTION_DISABLE_FAST_MATH /fp:precise)
else()
    set(O3DE_COMPILE_OPTION_DISABLE_FAST_MATH -fno-fast-math)
endif()

list(APPEND CMAKE_MODULE_PATH "${LY_ROOT_FOLDER}/cmake/3rdParty")
find_package(Box3D REQUIRED MODULE)

add_executable(Box3DInstalledConsumer main.c)
target_link_libraries(Box3DInstalledConsumer PRIVATE 3rdParty::Box3D)
