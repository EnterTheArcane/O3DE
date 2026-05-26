#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

include_guard()

find_program(O3DE_THIRDPARTY_PROGRAM
    NAMES thirdparty
    HINTS
    "${LY_ROOT_FOLDER}/Tools/ThirdParty/.venv/Scripts"
    "${LY_ROOT_FOLDER}/Tools/ThirdParty/.venv/bin"
    NO_DEFAULT_PATH
)

function(o3de_thirdparty_provide package_name)
    if(NOT O3DE_THIRDPARTY_PROGRAM)
        return()
    endif()

    get_property(_already GLOBAL PROPERTY O3DE_THIRDPARTY_${package_name}_PROVIDED)
    if(_already)
        return()
    endif()
    set_property(GLOBAL PROPERTY O3DE_THIRDPARTY_${package_name}_PROVIDED TRUE)

    execute_process(
        COMMAND "${O3DE_THIRDPARTY_PROGRAM}" provide "${package_name}"
        WORKING_DIRECTORY "${LY_ROOT_FOLDER}/Tools/ThirdParty"
        OUTPUT_VARIABLE provide_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE provide_error
        RESULT_VARIABLE provide_result
    )

    if(NOT provide_result EQUAL 0)
        message(WARNING
            "ThirdParty dependency provider: 'thirdparty provide ${package_name}' exited ${provide_result}.\n"
            "${provide_error}")
        return()
    endif()

    if(NOT provide_output STREQUAL "")
        find_package(${package_name}
            CONFIG
            BYPASS_PROVIDER
            GLOBAL
            CONFIGS "${package_name}-config.cmake"
            PATHS "${provide_output}"
            NO_DEFAULT_PATH
        )
        if(${package_name}_FOUND)
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
        endif()
    endif()
endfunction()
