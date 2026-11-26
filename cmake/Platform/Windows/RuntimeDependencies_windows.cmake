#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(LY_RUNTIME_DEPENDENCIES_TEMPLATE ${LY_ROOT_FOLDER}/cmake/Platform/Common/runtime_dependencies_common.cmake.in)
include(cmake/Platform/Common/RuntimeDependencies_common.cmake)

function(ly_configure_runtime_staging_target target)
    if(CMAKE_GENERATOR STREQUAL "Visual Studio 18 2026")
        # The compiler launcher disables tracking for compilation. MSBuild needs it to skip clean copies.
        set_target_properties("${target}" PROPERTIES
            VS_GLOBAL_TrackFileAccess true
            VS_PROJECT_IMPORT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RuntimeFileStaging.props")
    endif()
endfunction()
