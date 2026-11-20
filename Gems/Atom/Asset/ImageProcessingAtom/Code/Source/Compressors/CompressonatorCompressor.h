/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Compressors/Compressor.h>

namespace ImageProcessingAtom
{
    class CompressonatorCompressor final
        : public ICompressor
    {
    public:
        static bool IsCompressedPixelFormatSupported(EPixelFormat format);

        static bool IsSourceColorSpaceSupported(ColorSpace colorSpace, EPixelFormat destinationFormat);

        ColorSpace GetSupportedColorSpace(EPixelFormat format) const override;

        IImageObjectPtr CompressImage(IImageObjectPtr sourceImage,EPixelFormat destinationFormat, const CompressOption* compressOption) const override;

        IImageObjectPtr DecompressImage(IImageObjectPtr sourceImage, EPixelFormat destinationFormat) const override;

        EPixelFormat GetSuggestedUncompressedFormat(EPixelFormat compressedFormat, EPixelFormat uncompressedFormat) const override;

        const char* GetName() const override;
    };
}
