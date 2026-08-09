#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "Box3D source directory does not exist: ${SOURCE_DIR}")
endif()

if(NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "Box3D patch does not exist: ${PATCH_FILE}")
endif()

if(DEFINED PATCH_MARKER_FILE AND DEFINED PATCH_MARKER_TEXT)
    set(marker_path "${SOURCE_DIR}/${PATCH_MARKER_FILE}")
    if(EXISTS "${marker_path}")
        file(READ "${marker_path}" marker_contents)
        string(FIND "${marker_contents}" "${PATCH_MARKER_TEXT}" marker_position)
        if(NOT marker_position EQUAL -1)
            return()
        endif()
    endif()
endif()

find_package(Git REQUIRED)

execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${SOURCE_DIR}" rev-parse --show-toplevel
    RESULT_VARIABLE source_is_in_work_tree
    OUTPUT_VARIABLE work_tree
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

set(apply_directory "${SOURCE_DIR}")
set(directory_argument)
if(source_is_in_work_tree EQUAL 0)
    file(RELATIVE_PATH source_prefix "${work_tree}" "${SOURCE_DIR}")
    set(apply_directory "${work_tree}")
    set(directory_argument "--directory=${source_prefix}")
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${apply_directory}" apply --check --reverse ${directory_argument} "${PATCH_FILE}"
    RESULT_VARIABLE patch_is_applied
    OUTPUT_QUIET
    ERROR_QUIET
)
if(patch_is_applied EQUAL 0)
    if(DEFINED marker_path)
        file(WRITE "${marker_path}" "${PATCH_MARKER_TEXT}\n")
    endif()
    return()
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${apply_directory}" apply --check ${directory_argument} "${PATCH_FILE}"
    RESULT_VARIABLE patch_check_result
)
if(NOT patch_check_result EQUAL 0)
    message(FATAL_ERROR "Box3D patch does not apply cleanly: ${PATCH_FILE}")
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${apply_directory}" apply ${directory_argument} "${PATCH_FILE}"
    RESULT_VARIABLE patch_result
)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to apply Box3D patch: ${PATCH_FILE}")
endif()

if(DEFINED marker_path)
    file(WRITE "${marker_path}" "${PATCH_MARKER_TEXT}\n")
endif()
