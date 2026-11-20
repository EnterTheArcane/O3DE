#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(FILES
    "${brotli_SOURCE_DIR}/c/common/constants.c"
    "${brotli_SOURCE_DIR}/c/common/context.c"
    "${brotli_SOURCE_DIR}/c/common/dictionary.c"
    "${brotli_SOURCE_DIR}/c/common/platform.c"
    "${brotli_SOURCE_DIR}/c/common/shared_dictionary.c"
    "${brotli_SOURCE_DIR}/c/common/transform.c"
    "${brotli_SOURCE_DIR}/c/dec/bit_reader.c"
    "${brotli_SOURCE_DIR}/c/dec/decode.c"
    "${brotli_SOURCE_DIR}/c/dec/huffman.c"
    "${brotli_SOURCE_DIR}/c/dec/prefix.c"
    "${brotli_SOURCE_DIR}/c/dec/state.c"
    "${brotli_SOURCE_DIR}/c/dec/static_init.c"
    "${brotli_SOURCE_DIR}/c/enc/backward_references_hq.c"
    "${brotli_SOURCE_DIR}/c/enc/backward_references.c"
    "${brotli_SOURCE_DIR}/c/enc/bit_cost.c"
    "${brotli_SOURCE_DIR}/c/enc/block_splitter.c"
    "${brotli_SOURCE_DIR}/c/enc/brotli_bit_stream.c"
    "${brotli_SOURCE_DIR}/c/enc/cluster.c"
    "${brotli_SOURCE_DIR}/c/enc/command.c"
    "${brotli_SOURCE_DIR}/c/enc/compound_dictionary.c"
    "${brotli_SOURCE_DIR}/c/enc/compress_fragment_two_pass.c"
    "${brotli_SOURCE_DIR}/c/enc/compress_fragment.c"
    "${brotli_SOURCE_DIR}/c/enc/dictionary_hash.c"
    "${brotli_SOURCE_DIR}/c/enc/encode.c"
    "${brotli_SOURCE_DIR}/c/enc/encoder_dict.c"
    "${brotli_SOURCE_DIR}/c/enc/entropy_encode.c"
    "${brotli_SOURCE_DIR}/c/enc/fast_log.c"
    "${brotli_SOURCE_DIR}/c/enc/histogram.c"
    "${brotli_SOURCE_DIR}/c/enc/literal_cost.c"
    "${brotli_SOURCE_DIR}/c/enc/memory.c"
    "${brotli_SOURCE_DIR}/c/enc/metablock.c"
    "${brotli_SOURCE_DIR}/c/enc/static_dict.c"
    "${brotli_SOURCE_DIR}/c/enc/static_dict_lut.c"
    "${brotli_SOURCE_DIR}/c/enc/static_init.c"
    "${brotli_SOURCE_DIR}/c/enc/utf8_util.c"
)
