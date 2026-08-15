#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_gem_setup("Jolt")

# Jolt is built as an implementation detail of this Gem rather than being published
# as a 3rdParty:: package, so it is included directly instead of being placed on
# CMAKE_MODULE_PATH for find_package() to discover.
include(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/JoltNative.cmake)

add_subdirectory(Code)
