#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(WITH_DEMO OFF)

FetchContent_Declare(cli11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.6.1.tar.gz
    URL_HASH SHA256=377691f3fac2b340f12a2f79f523c780564578ba3d6eaf5238e9f35895d5ba95
    DOWNLOAD_NO_PROGRESS ON
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(cli11)

add_library(3rdParty::cli11 ALIAS CLI11)
set_target_properties(CLI11 PROPERTIES FOLDER 3rdParty)
