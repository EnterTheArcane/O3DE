#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if(TARGET 3rdParty::ANTLR4)
    set(ANTLR4_FOUND TRUE)
    return()
endif()

block(SCOPE_FOR VARIABLES)
    include(FetchContent)

    set(ANTLR_BUILD_CPP_TESTS OFF)
    set(ANTLR_BUILD_SHARED OFF)
    set(ANTLR_BUILD_STATIC ON)
    set(TRACE_ATN OFF)

    get_property(_generator_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_generator_is_multi_config AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    FetchContent_Declare(ANTLR4
        URL https://github.com/antlr/antlr4/archive/8e6fd9147b3c9d36b60e2b6656871a55227efb1b.tar.gz # 2026-01-01
        URL_HASH SHA256=c52fb2a90ae082ab5b5ec6b41f6c0e66f691962e4b4f772580e5da678fe51fed
        SOURCE_SUBDIR runtime/Cpp
        DOWNLOAD_NO_PROGRESS ON
        EXCLUDE_FROM_ALL
    )

    if(MSVC)
        foreach(flag_variable IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
            string(REGEX REPLACE "(^| )/we[0-9]+($| )" " " ${flag_variable} "${${flag_variable}}")
            string(REGEX REPLACE " +" " " ${flag_variable} "${${flag_variable}}")
        endforeach()
    endif()

    FetchContent_MakeAvailable(ANTLR4)

    add_library(3rdParty::ANTLR4 ALIAS antlr4_static)

    target_compile_options(antlr4_static PRIVATE
        ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS}
        ${O3DE_COMPILE_OPTION_ENABLE_EXCEPTIONS}
    )
    
    target_compile_options(antlr4_static PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/wd4266>
    )

    cmake_path(RELATIVE_PATH CMAKE_CURRENT_LIST_DIR BASE_DIRECTORY "${LY_ROOT_FOLDER}" OUTPUT_VARIABLE relative_source_root)
    set_target_properties(antlr4_static PROPERTIES
        FOLDER "${relative_source_root}/External"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endblock()

set(ANTLR4_FOUND TRUE)
