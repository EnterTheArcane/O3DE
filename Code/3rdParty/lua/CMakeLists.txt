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

ly_add_3rdparty_target(
    NAME lua STATIC
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

ly_create_alias(NAME Lua NAMESPACE 3rdParty TARGETS 3rdParty::lua)
