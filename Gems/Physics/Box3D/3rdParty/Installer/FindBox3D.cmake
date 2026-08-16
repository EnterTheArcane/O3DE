#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(TARGET 3rdParty::Box3D)
    return()
endif()

set(Box3D_GIT_REPOSITORY "https://github.com/erincatto/box3d.git")
set(Box3D_GIT_TAG "v0.1.0")
message(STATUS "Box3D Gem uses ${Box3D_GIT_REPOSITORY} ${Box3D_GIT_TAG} (MIT)")

include("${LY_ROOT_FOLDER}/include/box3d/Box3DPackage.cmake")
if(DEFINED LY_BOX3D_DOUBLE_PRECISION AND
    NOT LY_BOX3D_DOUBLE_PRECISION STREQUAL Box3D_PACKAGE_DOUBLE_PRECISION)
    message(FATAL_ERROR
        "LY_BOX3D_DOUBLE_PRECISION=${LY_BOX3D_DOUBLE_PRECISION} does not match the installed Box3D library "
        "(${Box3D_PACKAGE_DOUBLE_PRECISION})."
    )
endif()
set(LY_BOX3D_DOUBLE_PRECISION ${Box3D_PACKAGE_DOUBLE_PRECISION})
if(NOT CMAKE_C_COMPILER_ID STREQUAL Box3D_PACKAGE_C_COMPILER_ID OR
    NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL Box3D_PACKAGE_C_COMPILER_VERSION)
    message(FATAL_ERROR
        "Box3D was built with ${Box3D_PACKAGE_C_COMPILER_ID} ${Box3D_PACKAGE_C_COMPILER_VERSION}, but the current "
        "toolchain is ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}. Release IPO archives require the installed "
        "engine toolchain."
    )
endif()

set(Box3D_LIBRARY_FOLDER "${LY_ROOT_FOLDER}/lib/${PAL_PLATFORM_NAME}")

add_library(Box3DNative STATIC IMPORTED GLOBAL)
set_target_properties(Box3DNative PROPERTIES
    IMPORTED_LOCATION "${Box3D_LIBRARY_FOLDER}/profile/${CMAKE_STATIC_LIBRARY_PREFIX}box3d${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_DEBUG "${Box3D_LIBRARY_FOLDER}/debug/${CMAKE_STATIC_LIBRARY_PREFIX}box3d${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_RELEASE "${Box3D_LIBRARY_FOLDER}/release/${CMAKE_STATIC_LIBRARY_PREFIX}box3d${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_link_options(Box3DNative INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_C_LINK_OPTIONS_IPO}>"
    )
else()
    target_link_options(Box3DNative INTERFACE
        "$<$<CONFIG:Release>:${CMAKE_C_COMPILE_OPTIONS_IPO}>"
        "$<$<CONFIG:Release>:${CMAKE_C_LINK_OPTIONS_IPO}>"
    )
endif()

ly_target_include_system_directories(TARGET Box3DNative INTERFACE "${LY_ROOT_FOLDER}/include")

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    if(MSVC_VERSION LESS 1930)
        target_compile_options(Box3DNative INTERFACE /fp:strict)
    else()
        target_compile_options(Box3DNative INTERFACE ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH})
    endif()
elseif(MSVC AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Box3DNative INTERFACE
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        /clang:-ffp-contract=off
        -Wno-overriding-option
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(Box3DNative INTERFACE
        ${O3DE_COMPILE_OPTION_DISABLE_FAST_MATH}
        -ffp-contract=off
    )
else()
    message(FATAL_ERROR
        "Box3D deterministic floating-point settings are not defined for ${CMAKE_C_COMPILER_ID}."
    )
endif()

if(Box3D_PACKAGE_DOUBLE_PRECISION)
    target_compile_definitions(Box3DNative INTERFACE BOX3D_DOUBLE_PRECISION)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(Box3DNative INTERFACE m)
endif()

add_library(3rdParty::Box3D ALIAS Box3DNative)
set(Box3D_FOUND TRUE)
