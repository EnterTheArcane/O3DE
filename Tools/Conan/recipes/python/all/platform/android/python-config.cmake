#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(Python_VERSION 3.15.2)
set(Python_FOUND TRUE)
set(Python_Development_FOUND TRUE)
set(Python_HOME "${CMAKE_CURRENT_LIST_DIR}/python")
set(Python_INCLUDE_DIR "${Python_HOME}/include/python3.15")
set(Python_LIBRARY "${Python_HOME}/lib/libpython3.15.so")
set(Python_PATHS
    "${Python_HOME}/lib/python3.15"
    "${Python_HOME}/lib/python3.15/lib-dynload")

if(NOT TARGET Python::Python)
    add_library(Python::Python SHARED IMPORTED)
    set_target_properties(Python::Python PROPERTIES
        IMPORTED_LOCATION "${Python_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Python_INCLUDE_DIR}")
endif()
