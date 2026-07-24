/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Crc.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(Crc32, "Crc32", "{E8A4C1D7-9F63-4B2A-A50E-7C1D3B8F6A24}")

    void Crc32::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Crc32>()
                ->Field("Value", &Crc32::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(Crc64, "Crc64", "{F9B5D2E8-A074-4C3B-B61F-8D2E4C9A7B35}")

    void Crc64::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Crc64>()
                ->Field("Value", &Crc64::m_value);
        }
    }
} // namespace AZ::Hash
