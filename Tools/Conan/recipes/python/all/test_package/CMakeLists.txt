#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 3.28)
project(TestCPython LANGUAGES C)

find_package(Python REQUIRED COMPONENTS Development)
find_package(Python3 MODULE REQUIRED COMPONENTS Development.Embed)
if(NOT TARGET Python::Module OR NOT Python_LIBRARY STREQUAL Python3_LIBRARY)
    message(FATAL_ERROR "Python and Python3 must expose the same SDK")
endif()
add_library(test_python SHARED embed.c)
target_link_libraries(test_python PRIVATE Python3::Python)
target_link_options(test_python PRIVATE "-Wl,--no-undefined")
