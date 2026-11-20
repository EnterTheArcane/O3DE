#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(brotli
    VERSION "1.2.1"
    LICENSE "MIT"
    URL "https://github.com/google/brotli/archive/refs/tags/v1.2.1.tar.gz"
    URL_HASH "98d35d11a78dc453aebb7d10c3748f3f73ff78c8267c0dc697b8631f67bf7e8f"
    GIT "https://github.com/google/brotli.git"
    GIT_HASH "db24a98aed38a16de78cc05ca15cdd8dc2a0d92d"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(brotli)

file(
    COPY "${brotli_SOURCE_DIR}/c/include/brotli"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

file(COPY
    "${brotli_SOURCE_DIR}/c/common"
    "${brotli_SOURCE_DIR}/c/dec"
    "${brotli_SOURCE_DIR}/c/enc"
    "${brotli_SOURCE_DIR}/c/include"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include/brotli/c"
    FILES_MATCHING PATTERN "*.h"
)

o3de_disable_warnings()

ly_add_target(
    NAME brotli STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

set_target_properties(brotli PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SYSTEM TRUE
)

include(CheckLibraryExists)
check_library_exists(m log "" BROTLI_HAS_LIBM)
if(BROTLI_HAS_LIBM)
    ly_target_link_libraries(brotli PRIVATE m)
endif()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/brotli
)
