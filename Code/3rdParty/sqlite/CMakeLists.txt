#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(sqlite_system_libraries)
set(sqlite_feature_definitions)

include(CheckSymbolExists)
check_symbol_exists(fdatasync "unistd.h" SQLITE_HAVE_FDATASYNC)
check_symbol_exists(gmtime_r "time.h" SQLITE_HAVE_GMTIME_R)
check_symbol_exists(localtime_r "time.h" SQLITE_HAVE_LOCALTIME_R)
check_symbol_exists(posix_fallocate "fcntl.h" SQLITE_HAVE_POSIX_FALLOCATE)
check_symbol_exists(strerror_r "string.h" SQLITE_HAVE_STRERROR_R)
check_symbol_exists(usleep "unistd.h" SQLITE_HAVE_USLEEP)

foreach(sqlite_feature FDATASYNC GMTIME_R LOCALTIME_R POSIX_FALLOCATE STRERROR_R USLEEP)
    if(SQLITE_HAVE_${sqlite_feature})
        list(APPEND sqlite_feature_definitions HAVE_${sqlite_feature}=1)
    endif()
endforeach()

if(CMAKE_DL_LIBS)
    list(APPEND sqlite_system_libraries PUBLIC ${CMAKE_DL_LIBS})
endif()
if(UNIX)
    find_package(Threads REQUIRED)
    if(CMAKE_THREAD_LIBS_INIT)
        if(NOT sqlite_system_libraries)
            list(APPEND sqlite_system_libraries PUBLIC)
        endif()
        list(APPEND sqlite_system_libraries ${CMAKE_THREAD_LIBS_INIT})
    endif()
    if(NOT APPLE)
        if(NOT sqlite_system_libraries)
            list(APPEND sqlite_system_libraries PUBLIC)
        endif()
        list(APPEND sqlite_system_libraries m)
    endif()
endif()

ly_add_target(
    NAME sqlite STATIC
    NAMESPACE 3rdParty
    OUTPUT_NAME sqlite3
    NO_UNITY
    FILES_CMAKE files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include
    COMPILE_DEFINITIONS
        PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_ENABLE_COLUMN_METADATA
            SQLITE_ENABLE_RTREE
            SQLITE_ENABLE_MATH_FUNCTIONS
            SQLITE_ENABLE_UNLOCK_NOTIFY
            ${sqlite_feature_definitions}
    BUILD_DEPENDENCIES ${sqlite_system_libraries}
    TARGET_PROPERTIES C_VISIBILITY_PRESET hidden
)

target_compile_options(sqlite PRIVATE ${O3DE_COMPILE_OPTION_DISABLE_WARNINGS})

ly_create_alias(NAME SQLite NAMESPACE 3rdParty TARGETS 3rdParty::sqlite)

ly_get_engine_relative_source_dir(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    third_party_install_destination
)

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
        README.md
    DESTINATION
        "${third_party_install_destination}"
)
