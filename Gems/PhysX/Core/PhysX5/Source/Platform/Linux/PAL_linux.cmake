#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-linux-x64 TARGETS PhysX5 PACKAGE_HASH 90c063157b1b2eeac6b213ba558a494f4e7c8ef5f73663fceebf10bdb3e776cd)
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-linux-aarch64 TARGETS PhysX5 PACKAGE_HASH d712744e5b8a3f74e00b154b08b9d6a8ba996ae21488312476bcb9c5b341cacc)
else()
    message(FATAL_ERROR "Unsupported linux architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()
