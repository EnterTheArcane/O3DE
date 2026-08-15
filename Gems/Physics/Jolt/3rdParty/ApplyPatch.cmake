#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "Jolt source directory does not exist: ${SOURCE_DIR}")
endif()

if(NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "Jolt patch does not exist: ${PATCH_FILE}")
endif()

if(DEFINED PATCH_MARKER_FILE AND DEFINED PATCH_MARKER_TEXT)
    set(marker_path "${SOURCE_DIR}/${PATCH_MARKER_FILE}")
    if(EXISTS "${marker_path}")
        file(READ "${marker_path}" marker_contents)
        string(FIND "${marker_contents}" "${PATCH_MARKER_TEXT}" marker_position)
        if(NOT marker_position EQUAL -1)
            return()
        endif()

        if(EXISTS "${SOURCE_ARCHIVE}")
            get_filename_component(source_parent "${SOURCE_DIR}" DIRECTORY)
            set(refresh_directory "${source_parent}/joltphysics-refresh")
            file(REMOVE_RECURSE "${refresh_directory}")
            file(MAKE_DIRECTORY "${refresh_directory}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${SOURCE_ARCHIVE}"
                WORKING_DIRECTORY "${refresh_directory}"
                RESULT_VARIABLE extract_result
            )
            if(NOT extract_result EQUAL 0)
                file(REMOVE_RECURSE "${refresh_directory}")
                message(FATAL_ERROR "Failed to extract pristine Jolt source: ${SOURCE_ARCHIVE}")
            endif()

            file(GLOB extracted_contents "${refresh_directory}/*")
            list(LENGTH extracted_contents extracted_content_count)
            if(extracted_content_count EQUAL 1 AND IS_DIRECTORY "${extracted_contents}")
                set(extracted_source "${extracted_contents}")
            else()
                set(extracted_source "${refresh_directory}")
            endif()

            file(REMOVE_RECURSE "${SOURCE_DIR}")
            file(RENAME "${extracted_source}" "${SOURCE_DIR}")
            file(REMOVE_RECURSE "${refresh_directory}")
        elseif(EXISTS "${SOURCE_DIR}/.git" AND DEFINED SOURCE_REVISION)
            find_package(Git REQUIRED)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} -C "${SOURCE_DIR}" reset --hard "${SOURCE_REVISION}"
                RESULT_VARIABLE reset_result
            )
            if(NOT reset_result EQUAL 0)
                message(FATAL_ERROR "Failed to restore pristine Jolt source: ${SOURCE_DIR}")
            endif()
        else()
            message(FATAL_ERROR
                "The fetched Jolt source uses an outdated patch and cannot be refreshed. "
                "Remove ${SOURCE_DIR} and configure again.")
        endif()
    endif()
endif()

find_package(Git REQUIRED)

set(git_command ${CMAKE_COMMAND} -E env)
if(DEFINED REPOSITORY_ROOT)
    list(APPEND git_command "GIT_CEILING_DIRECTORIES=${REPOSITORY_ROOT}")
endif()
list(APPEND git_command ${GIT_EXECUTABLE})

execute_process(
    COMMAND ${git_command} -C "${SOURCE_DIR}" rev-parse --show-toplevel
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
    COMMAND ${git_command} -C "${apply_directory}" apply --check --reverse ${directory_argument} "${PATCH_FILE}"
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
    COMMAND ${git_command} -C "${apply_directory}" apply --check ${directory_argument} "${PATCH_FILE}"
    RESULT_VARIABLE patch_check_result
)
if(NOT patch_check_result EQUAL 0)
    message(FATAL_ERROR "Jolt patch does not apply cleanly: ${PATCH_FILE}")
endif()

execute_process(
    COMMAND ${git_command} -C "${apply_directory}" apply ${directory_argument} "${PATCH_FILE}"
    RESULT_VARIABLE patch_result
)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to apply Jolt patch: ${PATCH_FILE}")
endif()

if(DEFINED marker_path)
    file(WRITE "${marker_path}" "${PATCH_MARKER_TEXT}\n")
endif()
