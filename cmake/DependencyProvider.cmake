#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

include_guard()

function(o3de_dependency_provider method package_name)
    if(NOT method STREQUAL "FIND_PACKAGE")
        return()
    endif()

    if(package_name MATCHES "^ThirdParty::")
        string(REPLACE "ThirdParty::" "" bare_name "${package_name}")
        o3de_thirdparty_provide("${bare_name}" ${ARGN})
        if(${bare_name}_FOUND)
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
        endif()
        return()
    endif()
endfunction()

cmake_language(SET_DEPENDENCY_PROVIDER o3de_dependency_provider SUPPORTED_METHODS FIND_PACKAGE)
