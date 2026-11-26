#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

o3de_pal_dir(pal_dir ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Platform/${PAL_PLATFORM_NAME} "${O3DE_ENGINE_RESTRICTED_PATH}" "${LY_ROOT_FOLDER}")
include(${pal_dir}/RuntimeDependencies_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

set(runtime_staging_platforms Windows Linux Mac)
if(PAL_PLATFORM_NAME IN_LIST runtime_staging_platforms
    AND (CMAKE_GENERATOR MATCHES "^Ninja( Multi-Config)?$"
        OR (PAL_PLATFORM_NAME STREQUAL "Windows" AND CMAKE_GENERATOR STREQUAL "Visual Studio 18 2026")))
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.5")
        set(LY_RUNTIME_FILE_STAGING TRUE)
        include(cmake/Platform/Common/RuntimeFileStaging.cmake)
    elseif(PAL_PLATFORM_NAME STREQUAL "Windows")
        message(FATAL_ERROR "Windows runtime file staging with ${CMAKE_GENERATOR} requires CMake 4.5 or newer")
    endif()
endif()

