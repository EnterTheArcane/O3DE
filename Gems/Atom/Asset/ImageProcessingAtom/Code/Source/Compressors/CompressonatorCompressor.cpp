/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzCore/std/algorithm.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <Compressors/CompressonatorCompressor.h>
#include <Processing/PixelFormatInfo.h>

#include <cmp_core.h>
#include <cstring>

namespace ImageProcessingAtom
{
    namespace
    {
        constexpr AZ::u32 BlockWidth = 4;
        constexpr AZ::u32 BlockBytes = 16;
        constexpr AZ::u32 Rgba8PixelBytes = 4;
        constexpr AZ::u32 Rgba16fPixelBytes = 8;
        constexpr AZ::u32 RgbChannels = 3;

        constexpr float GetCodecQuality(ICompressor::EQuality quality)
        {
            switch (quality)
            {
            case ICompressor::eQuality_Preview:
                return 0.0f;
            case ICompressor::eQuality_Fast:
                return 0.1f;
            case ICompressor::eQuality_Slow:
                return 0.6f;
            default:
                return 0.3f;
            }
        }

        using CreateOptionsFn = AZ::s32 (CMP_CDECL*)(void** options);
        using DestroyOptionsFn = AZ::s32 (CMP_CDECL*)(void* options);
        using SetQualityFn = AZ::s32 (CMP_CDECL*)(void* options, float quality);
        using CompressRgbaBlockFn = AZ::s32 (CMP_CDECL*)(const AZ::u8* pixels, AZ::u32 rowPitch, AZ::u8* encoded, const void* options);
        using CodecOptions = AZStd::unique_ptr<void, DestroyOptionsFn>;

        CodecOptions CreateOptions(
            CreateOptionsFn create,
            DestroyOptionsFn destroy,
            SetQualityFn setQuality,
            float quality)
        {
            void* handle = nullptr;
            const AZ::s32 result = create(&handle);
            CodecOptions options(handle, destroy);
            if (result != 0 || !options)
            {
                return CodecOptions(nullptr, destroy);
            }
            if (setQuality(handle, quality))
            {
                options.reset();
            }
            return options;
        }

        CodecOptions CreateCodecOptions(EPixelFormat format, const ICompressor::CompressOption& settings)
        {
            const float quality = GetCodecQuality(settings.compressQuality);
            switch (format)
            {
            case ePixelFormat_BC3:
                return CreateOptions(CreateOptionsBC3, DestroyOptionsBC3, SetQualityBC3, quality);
            case ePixelFormat_BC6UH:
                if (CodecOptions options = CreateOptions(CreateOptionsBC6, DestroyOptionsBC6, SetQualityBC6, quality))
                {
                    if (SetSignedBC6(options.get(), false))
                    {
                        options.reset();
                    }
                    return options;
                }
                break;
            case ePixelFormat_BC7:
            case ePixelFormat_BC7t:
                if (CodecOptions options = CreateOptions(CreateOptionsBC7, DestroyOptionsBC7, SetQualityBC7, quality))
                {
                    if (SetAlphaOptionsBC7(options.get(), !settings.discardAlpha, false, false))
                    {
                        options.reset();
                    }
                    return options;
                }
                break;
            default:
                break;
            }
            return CodecOptions(nullptr, nullptr);
        }

        struct SourceBlock
        {
            const AZ::u8* pixels;
            AZ::u32 rowPitch;
            AZ::u32 width;
            AZ::u32 height;

            void CopyPaddedPixels(void* output, AZ::u32 sourcePixelBytes, AZ::u32 outputPixelBytes) const
            {
                AZ::u8* destination = static_cast<AZ::u8*>(output);
                for (AZ::u32 y = 0; y < BlockWidth; ++y)
                {
                    const AZ::u8* row = pixels + static_cast<size_t>(AZStd::min(y, height - 1)) * rowPitch;
                    for (AZ::u32 x = 0; x < BlockWidth; ++x)
                    {
                        const AZ::u8* pixel = row + AZStd::min(x, width - 1) * sourcePixelBytes;
                        std::memcpy(destination, pixel, outputPixelBytes);
                        destination += outputPixelBytes;
                    }
                }
            }
            
            bool Encode(EPixelFormat format, const void* options, bool discardAlpha, AZ::u8* encoded) const
            {
                if (format == ePixelFormat_BC6UH)
                {
                    AZ::u16 rgb[BlockWidth * BlockWidth * RgbChannels];
                    CopyPaddedPixels(rgb, Rgba16fPixelBytes, RgbChannels * sizeof(AZ::u16));
                    return CompressBlockBC6(rgb, BlockWidth * RgbChannels, encoded, options) == 0;
                }

                CompressRgbaBlockFn compress = &CompressBlockBC7;
                if (format == ePixelFormat_BC3)
                {
                    compress = &CompressBlockBC3;
                    discardAlpha = false;
                }
                if (width == BlockWidth && height == BlockWidth && !discardAlpha)
                {
                    return compress(pixels, rowPitch, encoded, options) == 0;
                }

                AZ::u8 rgba[BlockWidth * BlockWidth][Rgba8PixelBytes];
                CopyPaddedPixels(rgba, Rgba8PixelBytes, Rgba8PixelBytes);
                if (discardAlpha)
                {
                    for (AZ::u8* pixel : rgba)
                    {
                        pixel[3] = 255;
                    }
                }
                return compress(rgba[0], BlockWidth * Rgba8PixelBytes, encoded, options) == 0;
            }
        };

        bool CompressMip(
            const IImageObject& source,
            IImageObject& destination,
            AZ::u32 mip,
            const void* options,
            bool discardAlpha)
        {
            AZ::u8* sourceData = nullptr;
            AZ::u8* destinationData = nullptr;
            AZ::u32 sourcePitch = 0;
            AZ::u32 destinationPitch = 0;
            source.GetImagePointer(mip, sourceData, sourcePitch);
            destination.GetImagePointer(mip, destinationData, destinationPitch);

            const AZ::u32 width = source.GetWidth(mip);
            const AZ::u32 height = source.GetHeight(mip);
            const AZ::u32 depth = source.GetDepth(mip);
            if (!sourceData || !destinationData || width == 0 || height == 0 || depth == 0)
            {
                return false;
            }

            const EPixelFormat format = destination.GetPixelFormat();
            const AZ::u32 pixelBytes = CPixelFormats::GetInstance().GetPixelFormatInfo(source.GetPixelFormat())->bitsPerBlock / 8;
            const AZ::u32 blockColumns = 1 + (width - 1) / BlockWidth;
            const AZ::u32 blockRows = 1 + (height - 1) / BlockWidth;
            const size_t sourceSliceBytes = static_cast<size_t>(sourcePitch) * height;
            const size_t destinationSliceBytes = static_cast<size_t>(destinationPitch) * blockRows;
            if (sourcePitch / pixelBytes < width
                || destinationPitch / BlockBytes < blockColumns
                || sourceSliceBytes > source.GetMipBufSize(mip) / depth
                || destinationSliceBytes > destination.GetMipBufSize(mip) / depth)
            {
                return false;
            }

            for (AZ::u32 slice = 0; slice < depth; ++slice)
            {
                for (AZ::u32 blockY = 0; blockY < blockRows; ++blockY)
                {
                    const AZ::u32 y = blockY * BlockWidth;
                    const AZ::u8* sourceRow = sourceData + slice * sourceSliceBytes + static_cast<size_t>(y) * sourcePitch;
                    AZ::u8* destinationRow = destinationData + slice * destinationSliceBytes + static_cast<size_t>(blockY) * destinationPitch;
                    for (AZ::u32 blockX = 0; blockX < blockColumns; ++blockX)
                    {
                        const AZ::u32 x = blockX * BlockWidth;
                        const SourceBlock block{
                            sourceRow + static_cast<size_t>(x) * pixelBytes,
                            sourcePitch,
                            AZStd::min(BlockWidth, width - x),
                            AZStd::min(BlockWidth, height - y)
                        };
                        // The BC3 encoder writes through a 32-bit pointer.
                        AZ::u32 encodedWords[BlockBytes / sizeof(AZ::u32)]{};
                        AZ::u8* encoded = reinterpret_cast<AZ::u8*>(encodedWords);
                        if (!block.Encode(format, options, discardAlpha, encoded))
                        {
                            return false;
                        }
                        std::memcpy(destinationRow + static_cast<size_t>(blockX) * BlockBytes, encoded, BlockBytes);
                    }
                }
            }
            return true;
        }
    }

    bool CompressonatorCompressor::IsCompressedPixelFormatSupported(EPixelFormat format)
    {
        return format == ePixelFormat_BC3
            || format == ePixelFormat_BC6UH
            || format == ePixelFormat_BC7
            || format == ePixelFormat_BC7t;
    }

    bool CompressonatorCompressor::IsSourceColorSpaceSupported(ColorSpace colorSpace, EPixelFormat destinationFormat)
    {
        return IsCompressedPixelFormatSupported(destinationFormat)
            && (destinationFormat != ePixelFormat_BC3 || colorSpace != ColorSpace::linear);
    }

    ColorSpace CompressonatorCompressor::GetSupportedColorSpace(EPixelFormat format) const
    {
        if (format == ePixelFormat_BC3)
        {
            return ColorSpace::sRGB;
        }
        return ColorSpace::autoSelect;
    }

    IImageObjectPtr CompressonatorCompressor::CompressImage(
        IImageObjectPtr sourceImage,
        EPixelFormat destinationFormat,
        const CompressOption* compressOption) const
    {
        if (!sourceImage || !IsCompressedPixelFormatSupported(destinationFormat) || sourceImage->GetMipCount() == 0)
        {
            return nullptr;
        }
        const EPixelFormat sourceFormat = sourceImage->GetPixelFormat();
        if (sourceFormat != GetSuggestedUncompressedFormat(destinationFormat, sourceFormat)
            || !CPixelFormats::GetInstance().IsImageSizeValid(destinationFormat, sourceImage->GetWidth(0), sourceImage->GetHeight(0), false))
        {
            return nullptr;
        }

        CompressOption settings;
        if (compressOption)
        {
            settings = *compressOption;
        }
        const CodecOptions options = CreateCodecOptions(destinationFormat, settings);
        if (!options)
        {
            return nullptr;
        }

        IImageObjectPtr destinationImage(sourceImage->AllocateImage(destinationFormat));
        if (!destinationImage || destinationImage->GetMipCount() == 0)
        {
            return nullptr;
        }
        for (AZ::u32 mip = 0; mip < destinationImage->GetMipCount(); ++mip)
        {
            if (!CompressMip(*sourceImage, *destinationImage, mip, options.get(), settings.discardAlpha))
            {
                return nullptr;
            }
        }
        return destinationImage;
    }

    IImageObjectPtr CompressonatorCompressor::DecompressImage(IImageObjectPtr, EPixelFormat) const
    {
        return nullptr;
    }

    EPixelFormat CompressonatorCompressor::GetSuggestedUncompressedFormat(EPixelFormat compressedFormat, EPixelFormat) const
    {
        if (compressedFormat == ePixelFormat_BC6UH)
        {
            return ePixelFormat_R16G16B16A16F;
        }
        return ePixelFormat_R8G8B8A8;
    }

    const char* CompressonatorCompressor::GetName() const
    {
        return "CompressonatorCompressor";
    }
}
