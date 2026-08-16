#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(TARGET 3rdParty::Box3D)
    return()
endif()

option(LY_BOX3D_DOUBLE_PRECISION "Build Box3D with double-precision native simulation" OFF)

block()
    o3de_fetch_content(Box3D
        VERSION "v0.1.0"
        LICENSE "MIT"
        URL "https://github.com/erincatto/box3d/archive/refs/tags/v0.1.0.tar.gz"
        URL_HASH "df232c7618c0d0d3927b798044559ee56eabadeb9d8ff9dc526d4b384d7b415d"
        GIT "https://github.com/erincatto/box3d.git"
        GIT_HASH "8441b4a06d6d09dcfb0b0f704df4d847d1437b92"
        PATCH_COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=<SOURCE_DIR>"
            "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/box3d-0.1.0-o3de.patch"
            "-DPATCH_MARKER_FILE=o3de-patch-version"
            "-DPATCH_MARKER_TEXT=box3d-v0.1.0-o3de-2"
            -P "${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake"
        SOURCE_SUBDIR src
        EXCLUDE_FROM_ALL
    )

    set(CMAKE_MESSAGE_LOG_LEVEL ${O3DE_FETCHCONTENT_MESSAGE_LEVEL})
    set(BUILD_SHARED_LIBS OFF)
    set(BOX3D_COMPILE_WARNING_AS_ERROR OFF)
    set(BOX3D_DISABLE_SIMD OFF)
    set(BOX3D_DOUBLE_PRECISION ${LY_BOX3D_DOUBLE_PRECISION})
    set(BOX3D_PROFILE OFF)
    set(BOX3D_VALIDATE OFF)

    FetchContent_MakeAvailable(Box3D)
endblock()

set_target_properties(box3d PROPERTIES DEBUG_POSTFIX "")
set_property(TARGET box3d PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_link_options(box3d INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_C_LINK_OPTIONS_IPO}>"
    )
else()
    target_link_options(box3d INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_C_COMPILE_OPTIONS_IPO}>"
        "$<$<CONFIG:Release>:${CMAKE_C_LINK_OPTIONS_IPO}>"
    )
endif()
target_compile_definitions(box3d PRIVATE $<$<CONFIG:Debug>:BOX3D_VALIDATE>)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    if(MSVC_VERSION LESS 1930)
        target_compile_options(box3d PUBLIC /fp:strict)
    else()
        target_compile_options(box3d PUBLIC ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH})
    endif()
elseif(MSVC AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(box3d PUBLIC
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        /clang:-ffp-contract=off
        -Wno-overriding-option
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(box3d PUBLIC
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        -ffp-contract=off
    )
else()
    message(FATAL_ERROR
        "Box3D deterministic floating-point settings are not defined for ${CMAKE_C_COMPILER_ID}."
    )
endif()

add_library(3rdParty::Box3D ALIAS box3d)

get_property(this_gem_root GLOBAL PROPERTY "@GEMROOT:${gem_name}@")
ly_get_engine_relative_source_dir(${this_gem_root} relative_this_gem_root)
o3de_fixup_fetchcontent_targets(
    IDE_FOLDER "${relative_this_gem_root}/External"
    TARGETS box3d
)

FetchContent_GetProperties(Box3D SOURCE_DIR box3d_source_dir)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/Installer/Box3DPackage.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/Box3DPackage.cmake
    @ONLY
)
ly_install(FILES ${CMAKE_CURRENT_LIST_DIR}/Installer/FindBox3D.cmake DESTINATION cmake/3rdParty)
ly_install(DIRECTORY ${box3d_source_dir}/include/box3d DESTINATION include COMPONENT CORE)
ly_install(FILES
    ${box3d_source_dir}/LICENSE
    ${CMAKE_CURRENT_BINARY_DIR}/Box3DPackage.cmake
    DESTINATION include/box3d
    COMPONENT CORE
)

set(Box3D_FOUND TRUE PARENT_SCOPE)
