#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ly_add_3rdparty_target(
    NAME pybind11 HEADERONLY
)

target_compile_options(pybind11 INTERFACE $<$<CXX_COMPILER_ID:Clang>:-fsized-deallocation>)

ly_target_include_system_directories(
    TARGET pybind11
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)
