/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/ISerializer.h>

#include <AzNetworking/Serialization/Internal/DecodeContext.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AzNetworking
{
    namespace
    {
        constexpr AZ::u32 MaxNewExternalSymbolsPerConnection = 1024;
        constexpr AZ::u32 MaxExternalSymbols = 262144;
        constexpr AZ::u64 MaxExternalSymbolBytes = 64 * 1024 * 1024;
        constexpr AZ::u64 ExternalSymbolEntryStorageBytes = 64;
    } // namespace

    bool SerializeObjectHelper<AZ::Symbol>::SerializeObject(
        ISerializer& serializer,
        AZ::Symbol& symbol)
    {
        AZStd::array<AZ::u8, AZ::Symbol::MaxLength> spellingBuffer;
        AZ::u32 spellingSize = 0;

        const AZStd::string_view currentSpelling = symbol.GetStringView();
        spellingSize = static_cast<AZ::u32>(currentSpelling.size());
        if (!currentSpelling.empty())
        {
            std::memcpy(spellingBuffer.data(), currentSpelling.data(), currentSpelling.size());
        }

        if (!serializer.SerializeBytes(spellingBuffer.data(), static_cast<AZ::u32>(spellingBuffer.size()), true, spellingSize, "Spelling"))
        {
            return false;
        }

        if (serializer.GetSerializerMode() == SerializerMode::ReadFromObject)
        {
            return true;
        }

        const AZStd::string_view spelling{reinterpret_cast<const char*>(spellingBuffer.data()), spellingSize};
        AZ::Symbol decodedSymbol;
        if (Internal::DecodeContext* context = Internal::DecodeAccess::Get(serializer))
        {
            constexpr AZ::Internal::ExternalSymbolAdmissionLimits processLimits{
                MaxExternalSymbols,
                MaxExternalSymbolBytes,
                ExternalSymbolEntryStorageBytes,
            };
            if (!AZ::Internal::AdmitExternalSymbol(
                    decodedSymbol,
                    spelling,
                    context->GetPermanentAdmissionCount(),
                    MaxNewExternalSymbolsPerConnection,
                    processLimits))
            {
                AZ_WarningOnce(
                    "AzNetworking",
                    false,
                    "Rejected an invalid or over-budget external-origin Symbol spelling");
                serializer.Invalidate();
                return false;
            }
        }
        else
        {
            if (!AZ::Internal::FindSymbol(decodedSymbol, spelling))
            {
                serializer.Invalidate();
                return false;
            }
        }

        symbol = decodedSymbol;
        return true;
    }
} // namespace AzNetworking
