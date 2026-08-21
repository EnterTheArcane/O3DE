#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_3rdparty_target(
    NAME lua STATIC
    INCLUDE_DIRECTORIES
        PUBLIC Include
        PRIVATE Include/Lua
    COMPILE_DEFINITIONS
        PRIVATE
            $<$<PLATFORM_ID:Linux>:LUA_USE_LINUX>
            $<$<PLATFORM_ID:Darwin>:LUA_USE_MACOSX>
            $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    BUILD_DEPENDENCIES
        PUBLIC
            ${CMAKE_DL_LIBS}
            $<$<BOOL:${O3DE_HAVE_LIBM}>:m>
)

ly_create_alias(NAME Lua NAMESPACE 3rdParty TARGETS 3rdParty::lua)
