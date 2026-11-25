#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

cmake_minimum_required(VERSION 3.25)
project(TestPackage LANGUAGES CXX)

find_package(OpenImageIO CONFIG REQUIRED)
add_executable(test_package test.cpp)
target_compile_definitions(test_package PRIVATE TEST_OPENIMAGEIO_FORMATS)
if(NOT ANDROID AND NOT EMSCRIPTEN)
    target_compile_definitions(test_package PRIVATE TEST_OPENIMAGEIO_DICOM)
endif()
if(NOT EMSCRIPTEN)
    target_compile_definitions(test_package PRIVATE TEST_OPENIMAGEIO_OPENVDB)
endif()
if(EMSCRIPTEN)
    target_compile_options(test_package PRIVATE -fPIC)
    target_link_options(test_package PRIVATE "-sALLOW_MEMORY_GROWTH=1")
endif()
target_link_libraries(test_package PRIVATE OpenImageIO::OpenImageIO)
