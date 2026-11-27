#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    message(FATAL_ERROR "PhysX 5.13 supports only macOS ARM64. Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-mac-arm64 TARGETS PhysX5 PACKAGE_HASH 4d7ab350bb6bd0f1cea29fc8ad94f759580a522341ffcc32b05f8060f704fe1a)
