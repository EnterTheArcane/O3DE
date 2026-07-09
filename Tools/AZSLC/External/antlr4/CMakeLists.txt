#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(ANTLR_BUILD_CPP_TESTS OFF)
set(ANTLR_BUILD_SHARED OFF)
set(ANTLR_BUILD_STATIC ON)
set(TRACE_ATN OFF)

FetchContent_Declare(antlr4
    URL https://github.com/antlr/antlr4/archive/8e6fd9147b3c9d36b60e2b6656871a55227efb1b.tar.gz # 2026-01-01
    URL_HASH SHA256=c52fb2a90ae082ab5b5ec6b41f6c0e66f691962e4b4f772580e5da678fe51fed
    SOURCE_SUBDIR runtime/Cpp
    DOWNLOAD_NO_PROGRESS ON
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(antlr4)

add_library(External::antlr4 ALIAS antlr4_static)
set_target_properties(antlr4_static PROPERTIES
    FOLDER External
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# O3DE disables exceptions globally, but ANTLR's parsing and error-recovery API
# is exception-based. Keep the override local to the third-party runtime.
if(MSVC)
    target_compile_options(antlr4_static PRIVATE
        /EHsc
        /U_HAS_EXCEPTIONS
        /D_HAS_EXCEPTIONS=1
    )
else()
    target_compile_options(antlr4_static PRIVATE
        -fexceptions
        -Wno-language-extension-token
        -Wno-unused-parameter
    )
    if(WIN32)
        target_compile_options(antlr4_static PRIVATE
            -U_HAS_EXCEPTIONS
            -D_HAS_EXCEPTIONS=1
        )
    endif()
endif()
