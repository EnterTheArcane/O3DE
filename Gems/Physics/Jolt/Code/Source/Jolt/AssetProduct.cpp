/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/AssetProduct.h>

#include <Jolt/NativeRuntime.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/PlatformId/PlatformDefaults.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace Jolt
{
    namespace
    {
        constexpr AZStd::array<AZ::u8, 8> ProductMagic = {'J', 'O', 'L', 'T', 'A', 'S', 'S', 'T'};
        constexpr AZ::u32 ProductFormatVersion = 2;
        constexpr size_t VersionOffset = ProductMagic.size();
        constexpr size_t PayloadSizeOffset = VersionOffset + sizeof(AZ::u32);
        constexpr size_t PayloadHashOffset = PayloadSizeOffset + sizeof(AZ::u64);
        constexpr size_t ProductHeaderSize = PayloadHashOffset + sizeof(AZ::u64);

        template<class Integer>
        void EncodeLittleEndian(
            AZ::u8* destination,
            Integer value)
        {
            for (size_t byteIndex = 0; byteIndex < sizeof(Integer); ++byteIndex)
            {
                destination[byteIndex] = static_cast<AZ::u8>(value >> (byteIndex * 8));
            }
        }

        template<class Integer>
        [[nodiscard]]
        Integer DecodeLittleEndian(
            const AZ::u8* source)
        {
            Integer value = 0;
            for (size_t byteIndex = 0; byteIndex < sizeof(Integer); ++byteIndex)
            {
                value |= static_cast<Integer>(source[byteIndex]) << (byteIndex * 8);
            }

            return value;
        }
    } // namespace

    AZStd::string_view GetNativeAssetPlatform()
    {
        return AZ::OSPlatformToDefaultAssetPlatform(AZ_TRAIT_OS_PLATFORM_CODENAME);
    }

    bool IsNativeAssetCacheCompatible(
        const AZStd::string_view platform,
        const AZ::u64 buildFingerprint)
    {
        return platform == GetNativeAssetPlatform()
            && buildFingerprint == GetNativeBuildFingerprint();
    }

    bool SaveAssetProduct(
        const AZStd::string& path,
        const void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext& serializeContext)
    {
        if (!asset)
        {
            return false;
        }

        AZStd::vector<AZ::u8> payload;
        AZ::IO::ByteContainerStream payloadStream(&payload);
        if (!AZ::Utils::SaveObjectToStream(
                payloadStream,
                AZ::DataStream::ST_BINARY,
                asset,
                assetType,
                &serializeContext)
            || payload.empty())
        {
            return false;
        }

        AZStd::array<AZ::u8, ProductHeaderSize> header{};
        for (size_t byteIndex = 0; byteIndex < ProductMagic.size(); ++byteIndex)
        {
            header[byteIndex] = ProductMagic[byteIndex];
        }
        EncodeLittleEndian(header.data() + VersionOffset, ProductFormatVersion);
        EncodeLittleEndian(header.data() + PayloadSizeOffset, aznumeric_cast<AZ::u64>(payload.size()));
        EncodeLittleEndian(
            header.data() + PayloadHashOffset,
            static_cast<AZ::u64>(AZ::TypeHash64(payload.data(), payload.size())));

        AZ::IO::SystemFileStream productStream(
            path.c_str(),
            AZ::IO::OpenMode::ModeWrite
                | AZ::IO::OpenMode::ModeBinary
                | AZ::IO::OpenMode::ModeCreatePath);
        if (!productStream.IsOpen()
            || productStream.Write(header.size(), header.data()) != header.size()
            || productStream.Write(payload.size(), payload.data()) != payload.size())
        {
            return false;
        }

        return true;
    }

    bool LoadAssetProduct(
        AZ::IO::GenericStream& stream,
        void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext* serializeContext)
    {
        if (!asset
            || !stream.IsOpen()
            || !stream.CanRead()
            || stream.GetLength() < ProductHeaderSize)
        {
            return false;
        }

        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::array<AZ::u8, ProductHeaderSize> header{};
        if (stream.Read(header.size(), header.data()) != header.size())
        {
            return false;
        }

        for (size_t byteIndex = 0; byteIndex < ProductMagic.size(); ++byteIndex)
        {
            if (header[byteIndex] != ProductMagic[byteIndex])
            {
                return false;
            }
        }

        if (DecodeLittleEndian<AZ::u32>(header.data() + VersionOffset) != ProductFormatVersion)
        {
            return false;
        }

        const AZ::u64 payloadSize = DecodeLittleEndian<AZ::u64>(header.data() + PayloadSizeOffset);
        if (payloadSize == 0
            || payloadSize > AZStd::numeric_limits<size_t>::max()
            || payloadSize != stream.GetLength() - ProductHeaderSize)
        {
            return false;
        }

        AZStd::vector<AZ::u8> payload(aznumeric_cast<size_t>(payloadSize));
        if (stream.Read(payloadSize, payload.data()) != payloadSize)
        {
            return false;
        }

        const AZ::u64 expectedHash = DecodeLittleEndian<AZ::u64>(header.data() + PayloadHashOffset);
        const AZ::u64 actualHash = static_cast<AZ::u64>(AZ::TypeHash64(payload.data(), payload.size()));
        if (actualHash != expectedHash)
        {
            return false;
        }

        AZ::IO::ByteContainerStream payloadStream(&payload);
        return AZ::Utils::LoadObjectFromStreamInPlace(
            payloadStream,
            serializeContext,
            assetType,
            asset);
    }

    bool LoadAssetProductFile(
        const AZStd::string& path,
        void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext* serializeContext)
    {
        AZ::IO::SystemFileStream stream(
            path.c_str(),
            AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
        return LoadAssetProduct(stream, asset, assetType, serializeContext);
    }
} // namespace Jolt
