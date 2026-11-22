#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

cmake_minimum_required(VERSION 3.25)
project(ISPCTexComp LANGUAGES CXX ISPC)

set(source "${ISPC_TEXCOMP_SOURCE}/ispc_texcomp")
add_library(ispc_texcomp SHARED
    "${source}/ispc_texcomp.cpp"
    "${source}/ispc_texcomp_astc.cpp"
    "${source}/kernel.ispc"
    "${source}/kernel_astc.ispc")
set_target_properties(ispc_texcomp PROPERTIES
    ISPC_INSTRUCTION_SETS "${ISPC_TEXCOMP_TARGETS}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(ispc_texcomp PRIVATE "${source}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(ispc_texcomp PRIVATE "$<$<COMPILE_LANGUAGE:ISPC>:--opt=fast-math>")
install(TARGETS ispc_texcomp RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES "${source}/ispc_texcomp.h" DESTINATION include/ISPC)
