# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT

cmake_minimum_required(VERSION 3.25)
project(PhysXTestPackage LANGUAGES CXX)

find_package(PhysX 5.13.0 EXACT CONFIG REQUIRED)
if(PHYSX_TEST_CUDA AND NOT TARGET PhysX::PhysXGpu)
    message(FATAL_ERROR "The CUDA-enabled package must export PhysX::PhysXGpu")
endif()
add_executable(test_package test.cpp)
target_compile_features(test_package PRIVATE cxx_std_20)
target_compile_definitions(test_package PRIVATE "$<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>")
target_link_libraries(test_package PRIVATE PhysX::physx_lib)
