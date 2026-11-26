#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

cmake_minimum_required(VERSION 4.5)
cmake_path(GET LY_STAGE_DESTINATION PARENT_PATH destination_directory)
file(MAKE_DIRECTORY "${destination_directory}")
# Publish the copy after the RPATH has been adjusted for the plugin subdirectory.
set(staged_plugin "${LY_STAGE_DESTINATION}.tmp")
file(COPY_FILE "${LY_STAGE_SOURCE}" "${staged_plugin}")
file(RPATH_CHANGE FILE "${staged_plugin}" OLD_RPATH "$ORIGIN/../../lib" NEW_RPATH "$ORIGIN/..")
file(RENAME "${staged_plugin}" "${LY_STAGE_DESTINATION}")
