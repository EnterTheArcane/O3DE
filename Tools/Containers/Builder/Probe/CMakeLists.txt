#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 3.25)
project(WindowsProbe LANGUAGES CXX RC)

add_executable(WindowsProbe main.cpp WindowsProbe.rc)
target_compile_features(WindowsProbe PRIVATE cxx_std_20)
set_target_properties(WindowsProbe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)
target_compile_options(WindowsProbe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/Z7>)
target_link_options(WindowsProbe PRIVATE /DEBUG)
