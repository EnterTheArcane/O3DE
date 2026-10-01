#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(vulkan-headers
    VERSION "1.4.363.0"
    LICENSE "Apache-2.0 OR MIT"
    URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz"
    URL_HASH "4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e"
    GIT "https://github.com/KhronosGroup/Vulkan-Headers.git"
    GIT_HASH "6802bb4733b63ed5efd3adb308a6c885ef180ea1"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(vulkan-headers)

file(
    COPY "${vulkan-headers_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

ly_add_target(
    NAME vulkan-headers HEADERONLY
    NAMESPACE 3rdParty
)

target_include_directories(vulkan-headers SYSTEM
    INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/Include"
)

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/vulkan-headers
)
