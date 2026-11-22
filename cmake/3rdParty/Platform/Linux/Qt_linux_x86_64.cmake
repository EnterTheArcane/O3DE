#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

function(ly_associate_linux_x86_64_qt target)
    ly_associate_package(
        PACKAGE_NAME qt-6.12.0-rev4-linux
        TARGETS ${target}
        PACKAGE_HASH 0057a242154417c9e99d7162b46eb8a27a02fb786ed96e6410acf3b2294e0aa0
    )
endfunction()
