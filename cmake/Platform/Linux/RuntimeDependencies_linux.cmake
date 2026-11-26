#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(LY_RUNTIME_DEPENDENCIES_TEMPLATE ${LY_ROOT_FOLDER}/cmake/Platform/Linux/runtime_dependencies_linux.cmake.in)
include(cmake/Platform/Common/RuntimeDependencies_common.cmake)

function(ly_runtime_file_copy_action source link copy_script)
    cmake_path(GET source FILENAME filename)
    if(LY_PYTHON_SHARED_LIB AND filename STREQUAL LY_PYTHON_SHARED_LIB)
        # Keep dynamically loaded Python on the same library handle.
        set(${link} "${PYTHON_PACKAGES_ROOT_PATH}/${LY_PYTHON_PACKAGE_NAME}/${LY_PYTHON_LIB_PATH}/${LY_PYTHON_SHARED_LIB}"
            PARENT_SCOPE)
    elseif(source MATCHES "/qt/plugins/.*\\.so$")
        set(${copy_script} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StageQtPlugin_linux.cmake" PARENT_SCOPE)
    endif()
endfunction()
