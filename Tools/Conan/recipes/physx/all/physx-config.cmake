# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT

if(TARGET PhysX::physx_lib)
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/release/lib/cmake/PhysX/physx-config.cmake")
block(SCOPE_FOR VARIABLES)
    foreach(config IN ITEMS debug checked profile)
        set(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_DIR}/${config}")
        include("${_IMPORT_PREFIX}/lib/cmake/PhysX/PhysXTargets-${config}.cmake")
    endforeach()
endblock()
