#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(volk
    VERSION "1.4.350"
    LICENSE "MIT"
    URL "https://github.com/zeux/volk/archive/refs/tags/1.4.350.tar.gz"
    URL_HASH "e47c6efe5294bb03729a976b385864bba5daf46ec60f0bdea11e1d1446345f9a"
    GIT "https://github.com/zeux/volk.git"
    GIT_HASH "3ca312a4f38baa63d8006b6905abbeeb89c8087d"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(volk)

file(
    COPY "${volk_SOURCE_DIR}/volk.h"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

o3de_disable_warnings()

ly_add_target(
    NAME volk STATIC
    NAMESPACE 3rdParty
    NO_UNITY
    FILES_CMAKE files.cmake
    COMPILE_DEFINITIONS
        PUBLIC
            VK_NO_PROTOTYPES
)

target_include_directories(volk SYSTEM
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

ly_target_link_libraries(volk PUBLIC 3rdParty::vulkan-headers)

if(PAL_PLATFORM_NAME STREQUAL "Windows")
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(PAL_PLATFORM_NAME STREQUAL "Android")
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
elseif(PAL_PLATFORM_NAME STREQUAL "Mac")
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_MACOS_MVK)
elseif(PAL_PLATFORM_NAME STREQUAL "Linux")
    if(PAL_TRAIT_LINUX_WINDOW_MANAGER_XCB)
        target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_XCB_KHR)
    endif()
    if(PAL_TRAIT_LINUX_WINDOW_MANAGER_WAYLAND)
        target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)
    endif()
endif()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/volk
)
