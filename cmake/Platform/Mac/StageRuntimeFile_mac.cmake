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
set(staged_file "${LY_STAGE_DESTINATION}.tmp")
file(COPY_FILE "${LY_STAGE_SOURCE}" "${staged_file}")
file(CHMOD "${staged_file}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(RENAME "${staged_file}" "${LY_STAGE_DESTINATION}")
