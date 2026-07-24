/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/City.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(City32, "City32", "{9E2C1A7F-3B4D-4E58-A6C9-1D0F8B2E5A47}")

    void City32::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<City32>()
                ->Field("Value", &City32::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(City64, "City64", "{A1F5D3B8-6C2E-4079-9B41-3E7A0C5D2F68}")

    void City64::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<City64>()
                ->Field("Value", &City64::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(City128, "City128", "{C7B4E9A2-5D18-4F3C-8A06-2B9F1E4D7C53}")

    void City128::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // TODO: Expose "Value" when we have a proper 128-bit type to reflect.
            serializeContext->Class<City128>()
                ->Field("Low", &City128::m_low)
                ->Field("High", &City128::m_high);
        }
    }
} // namespace AZ::Hash
