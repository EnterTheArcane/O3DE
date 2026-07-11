#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if(TARGET 3rdParty::CLI11)
    set(CLI11_FOUND TRUE)
    return()
endif()

block(SCOPE_FOR VARIABLES)
    include(FetchContent)

    set(WITH_DEMO OFF)

    FetchContent_Declare(CLI11
        URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.6.1.tar.gz
        URL_HASH SHA256=377691f3fac2b340f12a2f79f523c780564578ba3d6eaf5238e9f35895d5ba95
        DOWNLOAD_NO_PROGRESS ON
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(CLI11)
    
    add_library(3rdParty::CLI11 ALIAS CLI11)

    cmake_path(RELATIVE_PATH CMAKE_CURRENT_LIST_DIR BASE_DIRECTORY "${LY_ROOT_FOLDER}" OUTPUT_VARIABLE relative_source_root)
    set_property(TARGET CLI11 PROPERTY FOLDER "${relative_source_root}/External")
endblock()

set(CLI11_FOUND TRUE)
