#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(JSONCPP_WITH_TESTS OFF)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF)
set(JSONCPP_WITH_CMAKE_PACKAGE OFF)
set(JSONCPP_WITH_EXAMPLE OFF)
set(JSONCPP_STATIC_WINDOWS_RUNTIME ON)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_OBJECT_LIBS OFF)

FetchContent_Declare(jsoncpp
    URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.tar.gz
    URL_HASH SHA256=f93b6dd7ce796b13d02c108bc9f79812245a82e577581c4c9aabe57075c90ea2
    DOWNLOAD_NO_PROGRESS ON
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(jsoncpp)

add_library(3rdParty::jsoncpp ALIAS jsoncpp_static)
set_target_properties(jsoncpp_static PROPERTIES FOLDER 3rdParty)

# JsonCpp throws its public exception types when operations fail.
if(MSVC)
    target_compile_options(jsoncpp_static PRIVATE
        /EHsc
        /U_HAS_EXCEPTIONS
        /D_HAS_EXCEPTIONS=1
    )
else()
    target_compile_options(jsoncpp_static PRIVATE -fexceptions)
    if(WIN32)
        target_compile_options(jsoncpp_static PRIVATE
            -U_HAS_EXCEPTIONS
            -D_HAS_EXCEPTIONS=1
        )
    endif()
endif()
