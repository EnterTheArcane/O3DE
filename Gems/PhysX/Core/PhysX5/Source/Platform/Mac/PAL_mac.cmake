#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    message(FATAL_ERROR "PhysX 5.12 supports only macOS ARM64. Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

ly_associate_package(PACKAGE_NAME PhysX-5.12.0-rev1-mac-arm64 TARGETS PhysX5 PACKAGE_HASH 2e94382290857a60e488ac4115dfcb2e58834accfed50112414e6e88a507e23d)
