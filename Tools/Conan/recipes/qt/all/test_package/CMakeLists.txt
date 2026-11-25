#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

cmake_minimum_required(VERSION 3.25)
project(TestPackage LANGUAGES CXX)

if(CMAKE_CROSSCOMPILING AND Qt6_ROOT)
    list(PREPEND CMAKE_FIND_ROOT_PATH "${Qt6_ROOT}")
endif()
if(EMSCRIPTEN)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets Svg)
    qt_add_executable(test_package test.cpp)
    target_compile_definitions(test_package PRIVATE QT_TEST_POINTER_SIZE=${QT_TEST_POINTER_SIZE})
    target_link_libraries(test_package PRIVATE Qt6::Widgets Qt6::Svg)
    target_link_options(test_package PRIVATE -sEXIT_RUNTIME=1)
else()
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
    if(ANDROID)
        add_library(test_package SHARED test.cpp)
    elseif(IOS)
        qt_add_executable(test_package MACOSX_BUNDLE test.cpp)
    else()
        find_package(Qt6 CONFIG REQUIRED COMPONENTS LanguageServerPrivate WebSockets)
        add_executable(test_package test.cpp)
        target_link_libraries(test_package PRIVATE Qt6::LanguageServerPrivate Qt6::WebSockets)
    endif()
endif()
get_target_property(qt_core_type Qt6::Core TYPE)
if(NOT qt_core_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "The Qt SDK must provide shared libraries")
endif()
target_link_libraries(test_package PRIVATE Qt6::Core)
