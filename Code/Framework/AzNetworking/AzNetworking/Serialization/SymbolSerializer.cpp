/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/ISerializer.h>

#include <AzNetworking/Serialization/Internal/SymbolAdmissionPolicy.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AzNetworking
{
    bool SerializeObjectHelper<AZ::Symbol>::SerializeObject(
        ISerializer& serializer,
        AZ::Symbol& symbol)
    {
        AZStd::array<AZ::u8, AZ::Symbol::MaxLength> spellingBuffer{};
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
        if (AZ::Internal::ValidateSymbolSpelling(spelling) != AZ::Internal::SymbolValidationError::None)
        {
            serializer.Invalidate();
            return false;
        }

        AZ::Symbol decodedSymbol;
        const Internal::SymbolSerializationContext& context = serializer.GetSymbolSerializationContext();
        switch (context.m_admission)
        {
        case SymbolAdmission::TrustedLocal:
            decodedSymbol = AZ::Internal::InternValidatedSymbol(spelling);
            break;
        case SymbolAdmission::NetworkOrigin:
            if (!context.m_policy || !Internal::AdmitNetworkOriginSymbol(spelling, *context.m_policy, decodedSymbol))
            {
                serializer.Invalidate();
                return false;
            }
            break;
        case SymbolAdmission::ExistingOnly:
            if (!AZ::Internal::FindSymbol(decodedSymbol, spelling))
            {
                serializer.Invalidate();
                return false;
            }
            break;
        }

        symbol = decodedSymbol;
        return true;
    }
} // namespace AzNetworking
