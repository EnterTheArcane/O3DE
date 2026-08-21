#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(mikktspace_system_libraries)
if(UNIX AND NOT APPLE)
    list(APPEND mikktspace_system_libraries PUBLIC m)
endif()

ly_add_3rdparty_target(
    NAME mikktspace STATIC
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Include/MikkTSpace
    BUILD_DEPENDENCIES
        ${mikktspace_system_libraries}
)

ly_create_alias(NAME MikkTSpace NAMESPACE 3rdParty TARGETS 3rdParty::mikktspace)
ly_create_alias(NAME mikkelsen NAMESPACE 3rdParty TARGETS 3rdParty::mikktspace)
