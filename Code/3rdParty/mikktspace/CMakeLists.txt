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

ly_add_target(
    NAME mikktspace STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Include/MikkTSpace
    BUILD_DEPENDENCIES ${mikktspace_system_libraries}
)

target_compile_options(mikktspace PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

ly_create_alias(NAME MikkTSpace NAMESPACE 3rdParty TARGETS 3rdParty::mikktspace)
ly_create_alias(NAME mikkelsen NAMESPACE 3rdParty TARGETS 3rdParty::mikktspace)

ly_get_engine_relative_source_dir(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    third_party_install_destination
)

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
        README.md
    DESTINATION
        "${third_party_install_destination}"
)
