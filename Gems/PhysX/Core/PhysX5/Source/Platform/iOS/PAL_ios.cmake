#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_PHYSX_SUPPORTED TRUE)

if(CMAKE_OSX_SYSROOT MATCHES "[Ii][Pp]hone[Ss]imulator")
    ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-ios-simulator-arm64 TARGETS PhysX5 PACKAGE_HASH 1cd61c82a4042812f8ebc137a588be25c3a8d287dbfd0418f3ca00a9e77669f8)
else()
    ly_associate_package(PACKAGE_NAME PhysX-5.13.0-rev1-ios TARGETS PhysX5 PACKAGE_HASH 4eef29b4d02dff5956acddec55d5ecbcc38598ccebc694a11c9425c063ed1ec3)
endif()
