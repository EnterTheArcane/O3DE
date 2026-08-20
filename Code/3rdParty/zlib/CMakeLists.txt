#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_target(
    NAME zlib STATIC
    NAMESPACE 3rdParty
    OUTPUT_NAME z
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Source
    COMPILE_DEFINITIONS
        PRIVATE
            $<$<NOT:$<PLATFORM_ID:Windows>>:Z_HAVE_UNISTD_H>
            $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_DEPRECATE>
            $<$<PLATFORM_ID:Windows>:_CRT_NONSTDC_NO_DEPRECATE>
)

target_compile_options(zlib PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

ly_create_alias(NAME ZLIB NAMESPACE 3rdParty TARGETS 3rdParty::zlib)
find_package(ZLIB 1.3.1 REQUIRED MODULE)

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

ly_install(FILES ${LY_ROOT_FOLDER}/cmake/3rdParty/FindZLIB.cmake DESTINATION cmake/3rdParty)
