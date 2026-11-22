#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

cmake_minimum_required(VERSION 3.25)
project(TestPackage LANGUAGES CXX)

find_package(OpenSSL CONFIG REQUIRED)
add_executable(test_package test.cpp)
target_link_libraries(test_package PRIVATE OpenSSL::SSL)
