#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_target(
    NAME rapidxml HEADERONLY
    NAMESPACE 3rdParty
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES INTERFACE Include
)

ly_create_alias(NAME RapidXML NAMESPACE 3rdParty TARGETS 3rdParty::rapidxml)

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
