#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if(NOT PAL_TRAIT_BUILD_TESTS_SUPPORTED OR NOT BUILD_TESTING)
    return()
endif()

update_pip_requirements(
    "${CMAKE_CURRENT_SOURCE_DIR}/../requirements.txt"
    azslc_test_requirements
)

set(AZSLC_CTEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/ctest-runner.py")
include(azslc_test_files.cmake)

# CTest launches executables directly and cannot invoke O3DE's .cmd/.sh
# convenience wrapper consistently on every host. The venv path is the same
# managed interpreter selected and validated by LYPython during configure.
set(AZSLC_PYTHON_COMMAND
    "${LY_PYTHON_VENV_PATH}/${LY_PYTHON_VENV_PYTHON}"
    -s
    -B
)

set(azslc_test_file_lists
    AZSLC_ADVANCED_TEST_FILES
    AZSLC_EMISSION_TEST_FILES
    AZSLC_EMISSION_ERROR_TEST_FILES
    AZSLC_SAMPLE_TEST_FILES
    AZSLC_SAMPLE_ERROR_TEST_FILES
    AZSLC_SEMANTIC_TEST_FILES
    AZSLC_SEMANTIC_ERROR_TEST_FILES
    AZSLC_SYNTAX_TEST_FILES
    AZSLC_SYNTAX_ERROR_TEST_FILES
)
foreach(test_file_list IN LISTS azslc_test_file_lists)
    list(TRANSFORM ${test_file_list} PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
endforeach()

function(azslc_add_simple_tests label compiler_flag expect_fail)
    foreach(test_file IN LISTS ARGN)
        file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${test_file}")
        # Strip extension to form the test name  (e.g. "Syntax/empty")
        string(REGEX REPLACE "\\.[^.]*$" "" test_name "${rel_path}")

        set(cmd_args
            "${AZSLC_CTEST_RUNNER}"
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode simple
        )
        if(NOT "${compiler_flag}" STREQUAL "")
            list(APPEND cmd_args "--compiler-flag=${compiler_flag}")
        endif()
        if(expect_fail)
            list(APPEND cmd_args --expect-fail)
        endif()

        get_filename_component(stem "${test_file}" NAME_WE)
        if(stem MATCHES "^wip-")
            list(APPEND cmd_args --wip)
        endif()

        add_test(
            NAME "${test_name}"
            COMMAND ${AZSLC_PYTHON_COMMAND} ${cmd_args}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        set(labels "${label}")
        if(stem MATCHES "^wip-")
            list(APPEND labels "WIP")
        endif()

        set_tests_properties("${test_name}" PROPERTIES
            LABELS "${labels}"
            SKIP_RETURN_CODE 77
            TIMEOUT 60
        )
    endforeach()
endfunction()

block() # Advanced
    foreach(test_file IN LISTS AZSLC_ADVANCED_TEST_FILES)
        get_filename_component(stem "${test_file}" NAME_WE)
        file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${test_file}")
        string(REGEX REPLACE "\\.[^.]*$" "" test_name "${rel_path}")

        add_test(
            NAME "${test_name}"
            COMMAND ${AZSLC_PYTHON_COMMAND} "${AZSLC_CTEST_RUNNER}"
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode advanced
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        set_tests_properties("${test_name}" PROPERTIES
            LABELS "Advanced"
            SKIP_RETURN_CODE 77
            TIMEOUT 240
        )
    endforeach()
endblock()

block() # Emission
    foreach(test_file IN LISTS AZSLC_EMISSION_TEST_FILES)
        file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${test_file}")
        string(REGEX REPLACE "\\.[^.]*$" "" test_name "${rel_path}")

        add_test(
            NAME "${test_name}"
            COMMAND ${AZSLC_PYTHON_COMMAND} "${AZSLC_CTEST_RUNNER}"
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode emission
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        set_tests_properties("${test_name}" PROPERTIES
            LABELS "Emission"
            SKIP_RETURN_CODE 77
            TIMEOUT 60
        )
    endforeach()
endblock()

block() # Emission Error
    foreach(test_file IN LISTS AZSLC_EMISSION_ERROR_TEST_FILES)
        file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${test_file}")
        string(REGEX REPLACE "\\.[^.]*$" "" test_name "${rel_path}")

        add_test(
            NAME "${test_name}"
            COMMAND ${AZSLC_PYTHON_COMMAND} "${AZSLC_CTEST_RUNNER}"
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode emission-error
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        set_tests_properties("${test_name}" PROPERTIES
            LABELS "Emission"
            SKIP_RETURN_CODE 77
            TIMEOUT 60
        )
    endforeach()
endblock()

block() # Samples
    azslc_add_simple_tests(Samples "" FALSE ${AZSLC_SAMPLE_TEST_FILES})
    azslc_add_simple_tests(Samples "" TRUE ${AZSLC_SAMPLE_ERROR_TEST_FILES})
endblock()

block() # Semantic
    azslc_add_simple_tests(Semantic "--semantic" FALSE ${AZSLC_SEMANTIC_TEST_FILES})
    azslc_add_simple_tests(Semantic "--semantic" TRUE ${AZSLC_SEMANTIC_ERROR_TEST_FILES})
endblock()

block() # Syntax
    azslc_add_simple_tests(Syntax "--syntax" FALSE ${AZSLC_SYNTAX_TEST_FILES})
    azslc_add_simple_tests(Syntax "--syntax" TRUE ${AZSLC_SYNTAX_ERROR_TEST_FILES})
endblock()
