#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
   ly_associate_package(PACKAGE_NAME PhysX-5.12.0-rev1-linux-x64 TARGETS PhysX5 PACKAGE_HASH 2f0882b4a34fb28a3ca62a78db5a47d1adcc0f0a74b2b8af849f2cb861309c61)
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    ly_associate_package(PACKAGE_NAME PhysX-5.12.0-rev1-linux-arm TARGETS PhysX5 PACKAGE_HASH ec0b3ff4e1e0298a0b3d12a27677ab2d8bc769947510b630945e36ee2c082393)
else()
    message(FATAL_ERROR "Unsupported linux architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()
