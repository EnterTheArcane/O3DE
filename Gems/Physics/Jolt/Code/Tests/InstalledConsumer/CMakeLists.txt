#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT O3DE_ENGINE_ROOT)
    message(FATAL_ERROR "O3DE_ENGINE_ROOT must identify the source or installed engine root")
endif()

if(NOT PROJECT_NAME)
    cmake_minimum_required(VERSION 3.24)

    include(${O3DE_ENGINE_ROOT}/cmake/o3deConfigVersion.cmake)
    list(APPEND CMAKE_MODULE_PATH ${O3DE_ENGINE_ROOT}/cmake)
    include(${O3DE_ENGINE_ROOT}/cmake/Findo3de.cmake)

    project(JoltInstalledConsumer
        LANGUAGES C CXX
        VERSION 1.0.0.0
    )
    o3de_initialize()
    return()
endif()

ly_add_target(
    NAME JoltInstalledConsumer EXECUTABLE
    NAMESPACE Qualification
    FILES_CMAKE
        jolt_installed_consumer_files.cmake
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzCore
            Gem::Jolt.API
)
