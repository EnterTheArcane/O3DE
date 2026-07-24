/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Xxh.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(Xxh32, "Xxh32", "{7F3A1C8D-2E5B-4A96-B0D4-3C6E9F1A2B85}")

    void Xxh32::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Xxh32>()
                ->Field("Value", &Xxh32::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(Xxh64, "Xxh64", "{8A4B2D9E-3F6C-4BA7-C1E5-4D7F0A2B3C96}")

    void Xxh64::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Xxh64>()
                ->Field("Value", &Xxh64::m_value);
        }
    }
} // namespace AZ::Hash
