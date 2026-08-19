/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Jolt.h>
#include <Jolt/Core/Profiler.h>

#ifdef JPH_EXTERNAL_PROFILE

#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/createdestroy.h>

namespace JPH
{
    ExternalProfileMeasurement::ExternalProfileMeasurement(
        const char* name,
        [[maybe_unused]] const uint32 color)
    {
        using ProfileScope = AZ::Debug::ProfileScope;
        static_assert(sizeof(ProfileScope) <= sizeof(mUserData));
        static_assert(alignof(ExternalProfileMeasurement) >= alignof(ProfileScope));

        AZStd::construct_at(
            reinterpret_cast<ProfileScope*>(mUserData),
            AZ_BUDGET_GETTER(Physics)(),
            "%s",
            name);
    }

    ExternalProfileMeasurement::~ExternalProfileMeasurement()
    {
        AZStd::destroy_at(reinterpret_cast<AZ::Debug::ProfileScope*>(mUserData));
    }
} // namespace JPH

#endif
