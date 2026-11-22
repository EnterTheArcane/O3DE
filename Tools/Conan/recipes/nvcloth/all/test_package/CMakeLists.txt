#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 3.25)
project(TestNvCloth LANGUAGES CXX)

find_package(NvCloth CONFIG REQUIRED)
add_executable(test_nvcloth test.cpp)
target_compile_features(test_nvcloth PRIVATE cxx_std_17)
target_compile_definitions(test_nvcloth PRIVATE "$<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>")
target_link_libraries(test_nvcloth PRIVATE NvCloth::NvCloth)
