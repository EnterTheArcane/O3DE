#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(FILES
    "${brotlig_SOURCE_DIR}/src/BrotligDecoder.cpp"
    "${brotlig_SOURCE_DIR}/src/BrotligEncoder.cpp"
    "${brotlig_SOURCE_DIR}/src/common/BrotligDataConditioner.cpp"
    "${brotlig_SOURCE_DIR}/src/common/BrotligSwizzler.cpp"
    "${brotlig_SOURCE_DIR}/src/common/BrotligUtils.cpp"
    "${brotlig_SOURCE_DIR}/src/DataStream.cpp"
    "${brotlig_SOURCE_DIR}/src/decoder/BrotligHuffmanTable.cpp"
    "${brotlig_SOURCE_DIR}/src/decoder/PageDecoder.cpp"
    "${brotlig_SOURCE_DIR}/src/encoder/BrotligHuffman.cpp"
    "${brotlig_SOURCE_DIR}/src/encoder/PageEncoder.cpp"
)
