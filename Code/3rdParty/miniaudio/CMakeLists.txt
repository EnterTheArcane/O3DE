#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

o3de_register_3rdparty_root("${LY_ROOT_FOLDER}/Gems/MiniAudio/3rdParty")
find_package(vorbis REQUIRED MODULE)

o3de_fetch_content(miniaudio
    VERSION "0.11.26"
    LICENSE "MIT No Attribution"
    URL "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz"
    URL_HASH "838ba55a21c9142adf7b76bdc4351e5cd8b7ae128f46b25ec25f94ae9969c600"
    GIT "https://github.com/mackron/miniaudio.git"
    GIT_HASH "877ea4a7c2601c7b0f2a44e83dfac42e6470fea6"
)

set(MINIAUDIO_BUILD_EXAMPLES OFF)
set(MINIAUDIO_BUILD_TESTS OFF)
set(MINIAUDIO_INSTALL OFF)
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_FRAMEWORK OFF)

FetchContent_MakeAvailable(miniaudio)

if(WIN32)
    # Match the STA initialization used by O3DE on the main thread.
    target_compile_definitions(miniaudio PRIVATE MA_COINIT_VALUE=2)
endif()

if(APPLE)
    # The CoreAudio and AVFoundation backends use Objective-C.
    set_source_files_properties(
        "${miniaudio_SOURCE_DIR}/miniaudio.c"
        TARGET_DIRECTORY miniaudio
        PROPERTIES COMPILE_OPTIONS -xobjective-c
    )
endif()

o3de_fixup_fetchcontent_targets(TARGETS
    miniaudio
    miniaudio_channel_combiner_node
    miniaudio_channel_separator_node
    miniaudio_ltrim_node
    miniaudio_reverb_node
    miniaudio_vocoder_node
    miniaudio_libvorbis
)

add_dependencies(miniaudio_libvorbis vorbisfile vorbisenc)

ly_install(
    FILES "${miniaudio_SOURCE_DIR}/miniaudio.h" "${miniaudio_SOURCE_DIR}/LICENSE"
    DESTINATION include/miniaudio
    COMPONENT CORE
)

foreach(node_plugin ma_channel_combiner_node ma_channel_separator_node ma_ltrim_node ma_reverb_node ma_vocoder_node)
    ly_install(
        FILES "${miniaudio_SOURCE_DIR}/extras/nodes/${node_plugin}/${node_plugin}.h"
        DESTINATION "include/miniaudio/extras/nodes/${node_plugin}"
        COMPONENT CORE
    )
endforeach()

foreach(decoder_plugin libopus libvorbis)
    ly_install(
        FILES "${miniaudio_SOURCE_DIR}/extras/decoders/${decoder_plugin}/miniaudio_${decoder_plugin}.h"
        DESTINATION "include/miniaudio/extras/decoders/${decoder_plugin}"
        COMPONENT CORE
    )
endforeach()

ly_install_files(
    FILES
        LICENSE.txt
        PackageInfo.json
    DESTINATION Code/3rdParty/miniaudio
)
