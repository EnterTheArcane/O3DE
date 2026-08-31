#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

# xxHash v0.8.3, upstream commit e626a72bc2321cd320e953a0ccf1584cad60f363.
ly_add_target(
    NAME xxhash STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE
        files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
)

ly_target_include_system_directories(
    TARGET xxhash
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

ly_install_files(
    FILES
        LICENSE
        PackageInfo.json
    DESTINATION
        Code/3rdParty/xxhash
)
