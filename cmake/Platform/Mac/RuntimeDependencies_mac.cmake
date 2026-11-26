#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(LY_BUILD_FIXUP_BUNDLE TRUE CACHE BOOL "Fix bundles on build (deploys frameworks and calls fixup_bundle)")

set(LY_RUNTIME_DEPENDENCIES_TEMPLATE ${LY_ROOT_FOLDER}/cmake/Platform/Mac/runtime_dependencies_mac.cmake.in)
set(LY_RUNTIME_FILE_COPY_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/StageRuntimeFile_mac.cmake")
include(cmake/Platform/Common/RuntimeDependencies_common.cmake)

function(ly_add_runtime_staging_build_event target)
    # Bundle fixup needs the linked executable. Flat targets can also have framework deployments.
    set(script "$<GENEX_EVAL:$<TARGET_PROPERTY:${target},LY_RUNTIME_DEPENDENCY_SCRIPT>>")
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND "$<$<BOOL:${script}>:${CMAKE_COMMAND};-P;${script}>"
        COMMAND_EXPAND_LISTS VERBATIM)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${script}")
endfunction()
