#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

from archive_deployer import deploy_packages


def deploy(graph, output_folder, **_):
    deploy_packages(graph, output_folder)
