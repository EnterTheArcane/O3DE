#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# Detect the ios SDK Path and set the SYSROOT
find_program(XCRUN_PROG "xcrun")
execute_process(COMMAND ${XCRUN_PROG} --sdk iphoneos --show-sdk-path
                OUTPUT_VARIABLE LY_IOS_SDK_PATH
                RESULT_VARIABLE GET_IOS_SDK_RESULT)
if (NOT GET_IOS_SDK_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to determine the iOS SDK path")
endif()
string(STRIP ${LY_IOS_SDK_PATH} LY_IOS_SDK_PATH)

set(CMAKE_SYSROOT "${LY_IOS_SDK_PATH}")

set(SDKROOT "iphoneos")

set(DEVROOT "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer")

# Ninja and other command-line generators need the resolved SDK path here.
# Passing the Xcode SDK name causes Clang to receive "-isysroot iphoneos",
# which takes precedence over CMAKE_SYSROOT and prevents standard headers from
# being found when compiling source-based third-party libraries.
set(CMAKE_OSX_SYSROOT "${LY_IOS_SDK_PATH}" CACHE PATH "iOS SDK sysroot" FORCE)
