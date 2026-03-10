/*
* Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/Fnv.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(Fnv32, "Fnv32", "{AF2AE713-1B8B-428A-A976-3087CC3D2612}")
    AZ_TYPE_INFO_WITH_NAME_IMPL(Fnv64, "Fnv64", "{6A691207-691F-4D94-B18C-216492A07E23}")
}

void AZ::Hash::Fnv32::Reflect(AZ::SerializeContext& context)
{
    using Self = Fnv32;
    context.Class<Self>()
        ->Field("Value", &Self::m_value);
}

void AZ::Hash::Fnv64::Reflect(AZ::SerializeContext& context)
{
    using Self = Fnv64;
    context.Class<Self>()
        ->Field("Value", &Self::m_value);
}
