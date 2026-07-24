/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Fnv.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(Fnv32, "Fnv32", "{AF2AE713-1B8B-428A-A976-3087CC3D2612}")

    void Fnv32::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Fnv32>()
                ->Field("Value", &Fnv32::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(Fnv64, "Fnv64", "{6A691207-691F-4D94-B18C-216492A07E23}")

    void Fnv64::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Fnv64>()
                ->Field("Value", &Fnv64::m_value);
        }
    }
} // namespace AZ::Hash
