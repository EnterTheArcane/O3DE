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

update_pip_requirements("${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt" azslc_test_requirements)

set(AZSLC_CTEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/ctest-runner.py")
include(azslc_test_files.cmake)

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

function(azslc_test_name out_var test_file)
    file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${test_file}")
    string(REGEX REPLACE "\\.[^.]*$" "" rel_path "${rel_path}")
    string(REPLACE "/" "." rel_path "${rel_path}")
    set(${out_var} "AZ::AZSLC.${rel_path}" PARENT_SCOPE)
endfunction()

function(ly_add_azslc_test)
    cmake_parse_arguments(arg "WIP" "NAME;CATEGORY;TIMEOUT" "RUNNER_ARGS" ${ARGN})

    set(labels "AZSLC.${arg_CATEGORY}")
    set(runner_args ${arg_RUNNER_ARGS})
    if(arg_WIP)
        list(APPEND labels "AZSLC.WIP")
        list(APPEND runner_args --wip)
    endif()

    ly_add_test(
        NAME "${arg_NAME}"
        TEST_SUITE main
        LABELS "${labels}"
        TIMEOUT ${arg_TIMEOUT}
        TEST_COMMAND "${LY_PYTHON_VENV_PATH}/${LY_PYTHON_VENV_PYTHON}" -s -B "${AZSLC_CTEST_RUNNER}" ${runner_args}
        RUNTIME_DEPENDENCIES AZ::AZSLC
        EXCLUDE_TEST_RUN_TARGET_FROM_IDE
    )

    # SKIP_RETURN_CODE and WORKING_DIRECTORY are not ly_add_test parameters, so set them
    # on the test ly_add_test just created (exported as LY_ADDED_TEST_NAME in this scope).
    set_tests_properties(${LY_ADDED_TEST_NAME} PROPERTIES
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

function(azslc_add_simple_tests category compiler_flag expect_fail)
    foreach(test_file IN LISTS ARGN)
        azslc_test_name(test_name "${test_file}")

        set(runner_args
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode simple
        )
        if(NOT "${compiler_flag}" STREQUAL "")
            list(APPEND runner_args "--compiler-flag=${compiler_flag}")
        endif()
        if(expect_fail)
            list(APPEND runner_args --expect-fail)
        endif()

        get_filename_component(stem "${test_file}" NAME_WE)
        set(wip_option "")
        if(stem MATCHES "^Wip")
            set(wip_option WIP)
        endif()

        ly_add_azslc_test(
            NAME "${test_name}"
            CATEGORY "${category}"
            TIMEOUT 60
            ${wip_option}
            RUNNER_ARGS ${runner_args}
        )
    endforeach()
endfunction()

foreach(test_file IN LISTS AZSLC_ADVANCED_TEST_FILES)
    azslc_test_name(test_name "${test_file}")

    ly_add_azslc_test(
        NAME "${test_name}"
        CATEGORY Advanced
        TIMEOUT 240
        RUNNER_ARGS
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode advanced
    )
endforeach()

foreach(test_file IN LISTS AZSLC_EMISSION_TEST_FILES)
    azslc_test_name(test_name "${test_file}")

    ly_add_azslc_test(
        NAME "${test_name}"
        CATEGORY Emission
        TIMEOUT 60
        RUNNER_ARGS
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode emission
    )
endforeach()

foreach(test_file IN LISTS AZSLC_EMISSION_ERROR_TEST_FILES)
    azslc_test_name(test_name "${test_file}")

    ly_add_azslc_test(
        NAME "${test_name}"
        CATEGORY Emission
        TIMEOUT 60
        RUNNER_ARGS
            --compiler "$<TARGET_FILE:AZ::AZSLC>"
            --file "${test_file}"
            --mode emission-error
    )
endforeach()

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
