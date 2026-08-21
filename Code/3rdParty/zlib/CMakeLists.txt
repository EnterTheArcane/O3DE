#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_3rdparty_target(
    NAME zlib STATIC
    OUTPUT_NAME z
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Source
    COMPILE_DEFINITIONS
        PRIVATE
            $<$<NOT:$<PLATFORM_ID:Windows>>:Z_HAVE_UNISTD_H>
            $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_DEPRECATE>
            $<$<PLATFORM_ID:Windows>:_CRT_NONSTDC_NO_DEPRECATE>
)

ly_create_alias(NAME ZLIB NAMESPACE 3rdParty TARGETS 3rdParty::zlib)
find_package(ZLIB 1.3.1 REQUIRED MODULE)

ly_install(FILES ${LY_ROOT_FOLDER}/cmake/3rdParty/FindZLIB.cmake DESTINATION cmake/3rdParty)
