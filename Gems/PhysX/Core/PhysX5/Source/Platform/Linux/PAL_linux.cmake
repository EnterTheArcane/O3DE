#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
   ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-linux-x64 TARGETS PhysX5 PACKAGE_HASH 5855bb5e63aabb436bd7f60432872523a8b8945b09c86c04a72794da7179e928)
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-linux-arm TARGETS PhysX5 PACKAGE_HASH 5b6ca1a38a33dc4c893e02ac02e1ae0a65bfe27184ad19d0422a57b51be82c45)
else()
    message(FATAL_ERROR "Unsupported linux architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()
