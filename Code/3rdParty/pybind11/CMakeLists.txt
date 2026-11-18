#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_fetch_content(pybind11
    VERSION "2.10.0"
    LICENSE "BSD-3-Clause"
    URL "https://github.com/pybind/pybind11/archive/refs/tags/v2.10.0.tar.gz"
    URL_HASH "eacf582fa8f696227988d08cfc46121770823839fe9e301a20fbce67e7cd70ec"
    GIT "https://github.com/pybind/pybind11.git"
    GIT_HASH "aa304c9c7d725ffb9d10af08a3b34cb372307020"
    PATCH_FILES "${CMAKE_CURRENT_LIST_DIR}/pybind11.patch"
    SOURCE_SUBDIR "no-add-subdir"
)
FetchContent_MakeAvailable(pybind11)

file(COPY "${pybind11_SOURCE_DIR}/include/pybind11" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Include")

ly_add_target(
    NAME pybind11 HEADERONLY
    NAMESPACE 3rdParty
)

target_include_directories(pybind11 SYSTEM INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/Include")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(pybind11 INTERFACE -fsized-deallocation)
endif()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/pybind11
)
