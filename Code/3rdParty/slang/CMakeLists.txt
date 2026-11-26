#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(slang_version "2026.22.1")

set(slang_architecture "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    set(slang_architecture "${CMAKE_OSX_ARCHITECTURES}")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    set(slang_architecture "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()
string(TOLOWER "${slang_architecture}" slang_architecture)
if(slang_architecture MATCHES "^(amd64|x64)$")
    set(slang_architecture x86_64)
elseif(slang_architecture STREQUAL "arm64")
    set(slang_architecture aarch64)
endif()

set(slang_archive)
set(slang_targets_file "lib/cmake/slang/slangTargets.cmake")
set(slang_runtime_dir lib)

if(WIN32)
    set(slang_targets_file "cmake/slangTargets.cmake")
    set(slang_runtime_dir bin)

    if(slang_architecture STREQUAL "x86_64")
        set(slang_archive "slang-${slang_version}-windows-x86_64.zip")
        set(slang_archive_hash "e72344f7b0323cec8b039c534caaf4680e7ba629489fc796ea4c7f7adb7deb68")
    elseif(slang_architecture STREQUAL "aarch64")
        set(slang_archive "slang-${slang_version}-windows-aarch64.zip")
        set(slang_archive_hash "7c50b36af8428533f524604d10cf84936dbd94d8536940d0611cb754ad4bea7c")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(slang_architecture STREQUAL "x86_64")
        set(slang_archive "slang-${slang_version}-linux-x86_64.tar.gz")
        set(slang_archive_hash "37156099ebaabf62cd92522eba55eb93b8663e65ea3be618ce015d9a8edda6bc")
    elseif(slang_architecture STREQUAL "aarch64")
        set(slang_archive "slang-${slang_version}-linux-aarch64-glibc-2.28.tar.gz")
        set(slang_archive_hash "fb603ad10e8558874a298b2a701d212ed135f6e6236cf9a7011563f44de7312f")
        set(slang_targets_file "lib64/cmake/slang/slangTargets.cmake")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(slang_architecture STREQUAL "x86_64")
        set(slang_archive "slang-${slang_version}-macos-x86_64.tar.gz")
        set(slang_archive_hash "4288c32f577d2444b211f6d40f490bfe73276c48ffc5bb8ce90e4ff29b209d2b")
    elseif(slang_architecture STREQUAL "aarch64")
        set(slang_archive "slang-${slang_version}-macos-aarch64.tar.gz")
        set(slang_archive_hash "1d5370a07be409b94685ea4bd84b7bd191313a6b1fae8795f451009bc0dc3fd7")
    endif()
endif()

if(NOT slang_archive)
    message(FATAL_ERROR "Slang ${slang_version} has no prebuilt package for ${CMAKE_SYSTEM_NAME} (${slang_architecture})")
endif()

o3de_fetch_content(slang
    VERSION "${slang_version}"
    LICENSE "Apache-2.0 WITH LLVM-exception"
    URL "https://github.com/shader-slang/slang/releases/download/v${slang_version}/${slang_archive}"
    URL_HASH "${slang_archive_hash}"
    NO_ADD_SUBDIR
)
FetchContent_MakeAvailable(slang)

# The upstream package config also searches the host for slangc.
include("${slang_SOURCE_DIR}/${slang_targets_file}")
get_property(slang_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
set_target_properties(${slang_targets} PROPERTIES IMPORTED_GLOBAL TRUE)

# O3DE's runtime staging needs an explicit mapping for each configuration.
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(${slang_targets} PROPERTIES MAP_IMPORTED_CONFIG_${config_upper} RELEASE)
endforeach()

# The archive ships the runtime library separately from its CMake exports.
add_library(slang::slang-rt SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(slang::slang-rt PROPERTIES
        IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang-rt.dll"
        IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang-rt.lib"
    )
elseif(APPLE)
    set(slang_rt_filename "libslang-rt.0.${slang_version}.dylib")
    set_target_properties(slang::slang-rt PROPERTIES
        IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/${slang_rt_filename}"
        IMPORTED_SONAME "@rpath/${slang_rt_filename}"
    )
else()
    set(slang_rt_filename "libslang-rt.so.0.${slang_version}")
    set_target_properties(slang::slang-rt PROPERTIES
        IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/${slang_rt_filename}"
        IMPORTED_SONAME "${slang_rt_filename}"
    )
endif()

add_library(3rdParty::slang ALIAS slang::slang)
add_library(3rdParty::slang::slang ALIAS slang::slang)
add_library(3rdParty::slang::gfx ALIAS slang::gfx)
add_library(3rdParty::slang::slang-rt ALIAS slang::slang-rt)
add_library(3rdParty::slang::slang-glslang ALIAS slang::slang-glslang)
add_library(3rdParty::slang::slang-glsl-module ALIAS slang::slang-glsl-module)
if(TARGET slang::slang-llvm)
    add_library(3rdParty::slang::slang-llvm ALIAS slang::slang-llvm)
endif()

add_executable(3rdParty::slang::slangc ALIAS slang::slangc)
add_executable(3rdParty::slang::slangd ALIAS slang::slangd)
add_executable(3rdParty::slang::slangi ALIAS slang::slangi)
add_executable(3rdParty::slang::slang-dispatcher ALIAS slang::slang-dispatcher)

target_compile_features(slang::slang INTERFACE cxx_std_17)

# O3DE follows interface links when staging runtime dependencies.
target_link_libraries(slang::gfx INTERFACE slang::slang)

foreach(slang_plugin slang::slang-glslang slang::slang-glsl-module slang::slang-llvm)
    if(TARGET ${slang_plugin})
        get_target_property(slang_plugin_file ${slang_plugin} IMPORTED_LOCATION_RELEASE)
        ly_add_target_files(
            TARGETS slang::slang
            FILES "${slang_plugin_file}"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE slang_module_files
    RELATIVE "${slang_SOURCE_DIR}/${slang_runtime_dir}"
    "${slang_SOURCE_DIR}/${slang_runtime_dir}/slang-standard-module-${slang_version}/*"
)
list(FILTER slang_module_files EXCLUDE REGEX "/[.]_")
foreach(slang_module_file IN LISTS slang_module_files)
    cmake_path(GET slang_module_file PARENT_PATH slang_module_subdirectory)
    ly_add_target_files(
        TARGETS slang::slang
        FILES "${slang_SOURCE_DIR}/${slang_runtime_dir}/${slang_module_file}"
        OUTPUT_SUBDIRECTORY "${slang_module_subdirectory}"
    )
endforeach()

ly_install_files(
    FILES
        CMakeLists.txt
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/slang
)

ly_install_directory(
    DIRECTORIES
        "${slang_SOURCE_DIR}/LICENSES"
        "${slang_SOURCE_DIR}/third-party-notices"
    DESTINATION Code/3rdParty/slang
)
