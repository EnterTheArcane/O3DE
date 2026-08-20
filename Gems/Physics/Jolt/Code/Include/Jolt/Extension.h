/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>

#include <AzCore/Math/Uuid.h>
#include <AzCore/base.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    enum class ExtensionKind : AZ::u8
    {
        None = 0,
        BodyPairCollider,
        ContactCallbacks,
        CustomConstraintProvider,
        CustomConvexShapeProvider,
        CustomPathProvider,
        CustomShapeProvider,
        GroupFilter,
        SimulationShapeFilter,
        SoftBodyContactCallbacks,
        StepListener,
        VehicleCallbacks,
        VehicleCollisionFilter,
        VirtualCharacterContactCallbacks,
    };

    enum class ExtensionRegistrationStatus : AZ::u8
    {
        None = 0,
        AlreadyRegistered,
        CapacityExhausted,
        InUse,
        Invalid,
        NotRegistered,
        Success,
    };

    //! Keeps the code and storage behind an extension alive until unregistration succeeds.
    //! An empty lease explicitly declares that the extension has process lifetime. A dynamic-module
    //! lease and its shared ownership state must be created by code that outlives the retained module.
    class ExtensionHostLease final
    {
    public:
        ExtensionHostLease() = default;

        explicit ExtensionHostLease(AZStd::shared_ptr<void> host)
            : m_host(AZStd::move(host))
        {
        }

        [[nodiscard]]
        explicit operator bool() const
        {
            return static_cast<bool>(m_host);
        }

    private:
        AZStd::shared_ptr<void> m_host;
    };

    struct ExtensionRegistrationResult final
    {
        [[nodiscard]]
        explicit operator bool() const
        {
            return m_status == ExtensionRegistrationStatus::Success;
        }

        ExtensionHandle m_handle;
        ExtensionRegistrationStatus m_status = ExtensionRegistrationStatus::None;
    };

    struct ExtensionInformation final
    {
        AZ::TypeId m_id = AZ::TypeId::CreateNull();
        AZ::u64 m_version = 0;
        AZ::u32 m_dependentCount = 0;
        ExtensionKind m_kind = ExtensionKind::None;
        bool m_hasHostLease = false;
    };
} // namespace Jolt
