/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/Serialization/ISerializer.h>

#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/containers/array.h>

#include <cstring>

namespace AzNetworking
{
    namespace
    {
#if AZ_COMPILER_MSVC
        __declspec(noinline)
#else
        __attribute__((noinline))
#endif
        bool DeserializeSymbol(ISerializer& serializer, AZ::Symbol& symbol)
        {
            AZStd::array<AZ::u8, AZ::Symbol::MaxStringSize> valueBuffer;
            const AZStd::string_view currentValue = symbol.GetStringView();
            AZ::u32 valueSize = static_cast<AZ::u32>(currentValue.size());
            if (!currentValue.empty())
            {
                std::memcpy(valueBuffer.data(), currentValue.data(), currentValue.size());
            }

            if (!serializer.SerializeBytes(
                    valueBuffer.data(),
                    static_cast<AZ::u32>(valueBuffer.size()),
                    true,
                    valueSize,
                    "Value"))
            {
                return false;
            }

            if (!serializer.IsValid())
            {
                return false;
            }

            if (valueSize > static_cast<AZ::u32>(valueBuffer.size()))
            {
                serializer.Invalidate();
                return false;
            }

            if (valueSize == currentValue.size())
            {
                if (valueSize == 0 || std::memcmp(valueBuffer.data(), currentValue.data(), valueSize) == 0)
                {
                    return true;
                }
            }

            const AZStd::string_view value{reinterpret_cast<const char*>(valueBuffer.data()), valueSize};
            const AZStd::optional<AZ::Symbol> decodedSymbol = AZ::Symbol::TryCreate(value);
            if (!decodedSymbol)
            {
                serializer.Invalidate();
                return false;
            }

            symbol = *decodedSymbol;
            return true;
        }
    } // namespace

    bool SerializeObjectHelper<AZ::Symbol>::SerializeObject(
        ISerializer& serializer,
        AZ::Symbol& symbol)
    {
        if (serializer.GetSerializerMode() != SerializerMode::ReadFromObject)
        {
            return DeserializeSymbol(serializer, symbol);
        }

        const AZStd::string_view currentValue = symbol.GetStringView();
        AZ::u32 valueSize = static_cast<AZ::u32>(currentValue.size());
        AZ::u8* valueData = const_cast<AZ::u8*>(reinterpret_cast<const AZ::u8*>(symbol.GetCStr()));
        return serializer.SerializeBytes(
            valueData,
            static_cast<AZ::u32>(AZ::Symbol::MaxStringSize),
            true,
            valueSize,
            "Value");
    }
} // namespace AzNetworking
