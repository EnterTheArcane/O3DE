#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(lua_system_libraries)
if(CMAKE_DL_LIBS)
    list(APPEND lua_system_libraries PUBLIC ${CMAKE_DL_LIBS})
endif()
if(UNIX AND NOT APPLE)
    if(NOT lua_system_libraries)
        list(APPEND lua_system_libraries PUBLIC)
    endif()
    list(APPEND lua_system_libraries m)
endif()

ly_add_target(
    NAME lua STATIC
    NAMESPACE 3rdParty
    OUTPUT_NAME lua
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Include/Lua
    COMPILE_DEFINITIONS
        PUBLIC
            $<$<PLATFORM_ID:Linux,Darwin>:LUA_USE_POSIX>
            $<$<PLATFORM_ID:Linux,Darwin>:LUA_USE_DLOPEN>
        PRIVATE
            $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    BUILD_DEPENDENCIES ${lua_system_libraries}
)

target_compile_options(lua PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

ly_create_alias(NAME Lua NAMESPACE 3rdParty TARGETS 3rdParty::lua)

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
