#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT TARGET 3rdParty::zlib)
    set(ZLIB_FOUND FALSE)
    if(ZLIB_FIND_REQUIRED)
        message(FATAL_ERROR "O3DE's vendored 3rdParty::zlib target has not been configured")
    endif()
    return()
endif()

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
endif()

set(ZLIB_INCLUDE_DIR "${LY_ROOT_FOLDER}/Code/3rdParty/zlib/Include")
set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}")
set(ZLIB_LIBRARY ZLIB::ZLIB)
set(ZLIB_LIBRARIES ZLIB::ZLIB)
set(ZLIB_VERSION "1.3.1")
set(ZLIB_VERSION_STRING "1.3.1")
set(ZLIB_VERSION_MAJOR "1")
set(ZLIB_VERSION_MINOR "3")
set(ZLIB_VERSION_PATCH "1")
set(ZLIB_MAJOR_VERSION "1")
set(ZLIB_MINOR_VERSION "3")
set(ZLIB_PATCH_VERSION "1")
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB
    REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
    VERSION_VAR ZLIB_VERSION
)
