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
set(kernels "${source}/kernel.ispc" "${source}/kernel_astc.ispc")
if(ISPC_TEXCOMP_IOS_SIMULATOR)
    # ISPC targets iOS devices. Let Apple's assembler set the simulator platform.
    enable_language(ASM)
    separate_arguments(ispc_flags NATIVE_COMMAND "${CMAKE_ISPC_FLAGS}")
    set(kernels)
    foreach(kernel IN ITEMS kernel kernel_astc)
        set(input "${source}/${kernel}.ispc")
        set(assembly "${CMAKE_CURRENT_BINARY_DIR}/${kernel}.s")
        set(header "${CMAKE_CURRENT_BINARY_DIR}/${kernel}_ispc.h")
        add_custom_command(
            OUTPUT "${assembly}" "${header}"
            COMMAND "${CMAKE_ISPC_COMPILER}" ${ispc_flags} "--target=${ISPC_TEXCOMP_TARGETS}"
                --emit-asm --opt=fast-math "$<IF:$<CONFIG:Debug>,-O0,-O2>"
                "$<$<CONFIG:Debug,RelWithDebInfo>:-g>" -h "${header}" -o "${assembly}" "${input}"
            COMMAND /usr/bin/sed -i.bak "/^[[:space:]]*\\.ios_version_min[[:space:]]/d" "${assembly}"
            DEPENDS "${input}" "${CMAKE_ISPC_COMPILER}"
            VERBATIM)
        list(APPEND kernels "${assembly}" "${header}")
    endforeach()
endif()

add_library(ispc_texcomp SHARED
    "${source}/ispc_texcomp.cpp"
    "${source}/ispc_texcomp_astc.cpp"
    ${kernels})
set_target_properties(ispc_texcomp PROPERTIES
    ISPC_INSTRUCTION_SETS "${ISPC_TEXCOMP_TARGETS}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(ispc_texcomp PRIVATE "${source}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(ispc_texcomp PRIVATE "$<$<COMPILE_LANGUAGE:ISPC>:--opt=fast-math>")
install(TARGETS ispc_texcomp RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES "${source}/ispc_texcomp.h" DESTINATION include/ISPC)
