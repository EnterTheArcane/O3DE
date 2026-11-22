#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

get_target_property(physx_include_dirs 3rdParty::PhysX5 INTERFACE_INCLUDE_DIRECTORIES)
list(GET physx_include_dirs 0 physx_include_dir)

# Keep the verified package intact. Quoted includes in PhysX require the full header tree.
set(physx_clang_include_dir "${CMAKE_CURRENT_BINARY_DIR}/PhysXClang/include")
set(physx_patch_dir "${CMAKE_CURRENT_BINARY_DIR}/PhysXClang/patch")
set(physx_windows_header "foundation/windows/PxWindowsInlineAoS.h")
set(physx_clang_patch "${CMAKE_CURRENT_LIST_DIR}/PhysXClang.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${physx_clang_patch}" "${physx_include_dir}/${physx_windows_header}")
file(COPY "${physx_include_dir}/" DESTINATION "${physx_clang_include_dir}"
    PATTERN "PxWindowsInlineAoS.h" EXCLUDE)
file(MAKE_DIRECTORY "${physx_patch_dir}/foundation/windows")
file(COPY_FILE "${physx_include_dir}/${physx_windows_header}" "${physx_patch_dir}/${physx_windows_header}")
execute_process(
    COMMAND ${LY_PYTHON_CMD} "${LY_ROOT_FOLDER}/cmake/Tools/apply_patch.py" "${physx_clang_patch}"
    WORKING_DIRECTORY "${physx_patch_dir}"
    COMMAND_ERROR_IS_FATAL ANY
)
file(COPY_FILE "${physx_patch_dir}/${physx_windows_header}"
    "${physx_clang_include_dir}/${physx_windows_header}" ONLY_IF_DIFFERENT)
target_include_directories(3rdParty::PhysX5 SYSTEM BEFORE INTERFACE
    "${physx_clang_include_dir}"
    "${physx_clang_include_dir}/foundation"
    "${physx_clang_include_dir}/geometry"
)
