#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# Fetches the prebuilt Slang shader compiler SDK (https://github.com/shader-slang/slang)
# and exposes it as the imported target 3rdParty::Slang.
#
# Slang is a host-tools dependency of the Atom shader builder gem only. Runtime targets
# must never link against it.

set(SLANG_TARGET Slang)

if(TARGET 3rdParty::${SLANG_TARGET})
    return()
endif()

set(SLANG_VERSION "2026.13.1")

# Select the prebuilt archive matching the host OS and architecture.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_slang_os "windows")
    set(_slang_ext "zip")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(_slang_os "linux")
    set(_slang_ext "tar.gz")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_slang_os "macos")
    set(_slang_ext "tar.gz")
else()
    message(FATAL_ERROR "Slang: unsupported host OS '${CMAKE_HOST_SYSTEM_NAME}'.")
endif()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _slang_host_arch)
if(_slang_host_arch MATCHES "^(amd64|x86_64|x64)$")
    set(_slang_arch "x86_64")
elseif(_slang_host_arch MATCHES "^(arm64|aarch64)$")
    set(_slang_arch "aarch64")
else()
    message(FATAL_ERROR "Slang: unsupported host architecture '${CMAKE_HOST_SYSTEM_PROCESSOR}'.")
endif()

# sha256 digests as published by the GitHub release API for v${SLANG_VERSION}.
set(_slang_hash_windows_x86_64  "fa1c9bcab2cdcd3626f7a1e250dd35d606c1b84745b64627f1dd63fca3746a70")
set(_slang_hash_windows_aarch64 "d34469404f092b8ac9fd6b11fb6e1bd653ab03b9a8e5cbfb694707b3f08e7f75")
set(_slang_hash_linux_x86_64    "c1ed948af94c6fd2034cc0f82f6892b8b287d5362939758cb4955676f26d893e")
set(_slang_hash_linux_aarch64   "26381afaa9bd5a41620b5bde63a175682e1fdc740efbd7b6a72c7351b4526464")
set(_slang_hash_macos_x86_64    "986fdccfb0a2f4ed811666b378df7d88978e932eba6764fc63138316e7338acf")
set(_slang_hash_macos_aarch64   "cf58b42ba87f66f58e0de297da57f4a5c92d00b7e7f38a708d2a4244abd8d003")

set(_slang_archive "slang-${SLANG_VERSION}-${_slang_os}-${_slang_arch}.${_slang_ext}")
set(_slang_hash "${_slang_hash_${_slang_os}_${_slang_arch}}")

block()
    o3de_fetch_content(${SLANG_TARGET}
        VERSION "v${SLANG_VERSION}"
        LICENSE "Apache-2.0 WITH LLVM-exception"
        URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_slang_archive}"
        URL_HASH "${_slang_hash}"
    )
    FetchContent_MakeAvailable(${SLANG_TARGET})
endblock()

# FetchContent keys its properties on the lower-cased content name.
FetchContent_GetProperties(${SLANG_TARGET})
set(_slang_root "${slang_SOURCE_DIR}")

if(NOT EXISTS "${_slang_root}/include/slang.h")
    message(FATAL_ERROR
        "Slang: fetched SDK at '${_slang_root}' does not contain include/slang.h. "
        "The archive layout of ${_slang_archive} is not what this module expects.")
endif()

# slang.dll / libslang.so is a thin legacy compatibility proxy scheduled for removal;
# the real compiler library is slang-compiler.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_slang_shared_library "${_slang_root}/bin/slang-compiler.dll")
    set(_slang_import_library "${_slang_root}/lib/slang-compiler.lib")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(_slang_shared_library "${_slang_root}/lib/libslang-compiler.so")
    set(_slang_import_library "")
else() # Darwin
    set(_slang_shared_library "${_slang_root}/lib/libslang-compiler.dylib")
    set(_slang_import_library "")
endif()

if(NOT EXISTS "${_slang_shared_library}")
    message(FATAL_ERROR
        "Slang: expected compiler library '${_slang_shared_library}' was not found in the fetched SDK. "
        "Inspect '${_slang_root}' for the actual library name/location and update FindSlang.cmake.")
endif()

add_library(${SLANG_TARGET} SHARED IMPORTED GLOBAL)
set_target_properties(${SLANG_TARGET} PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_slang_root}/include"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_slang_root}/include"
    IMPORTED_LOCATION "${_slang_shared_library}"
)
if(_slang_import_library)
    set_target_properties(${SLANG_TARGET} PROPERTIES IMPORTED_IMPLIB "${_slang_import_library}")
endif()

add_library(3rdParty::${SLANG_TARGET} ALIAS ${SLANG_TARGET})

set(${SLANG_TARGET}_FOUND TRUE PARENT_SCOPE)
