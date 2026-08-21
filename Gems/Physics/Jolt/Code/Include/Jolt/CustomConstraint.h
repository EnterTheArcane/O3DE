/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/limits.h>

namespace Jolt
{
    struct CustomConstraintBodyState final
    {
        AZ_TYPE_INFO(CustomConstraintBodyState, CustomConstraintBodyStateTypeId);

        WorldTransform m_centerOfMassTransform;
    };

    struct CustomConstraintContext final
    {
        AZ_TYPE_INFO(CustomConstraintContext, CustomConstraintContextTypeId);

        CustomConstraintBodyState m_firstBody;
        CustomConstraintBodyState m_secondBody;
        WorldTransform m_firstFrame;
        WorldTransform m_secondFrame;
        float m_baumgarte = 0.0f;
        float m_deltaTime = 0.0f;
    };

    //! One scalar Jacobian row. The native adapter calculates effective mass and applies impulses.
    struct CustomConstraintRow final
    {
        AZ_TYPE_INFO(CustomConstraintRow, CustomConstraintRowTypeId);

        AZ::Vector3 m_firstAngular = AZ::Vector3::CreateZero();
        AZ::Vector3 m_firstLinear = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondAngular = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondLinear = AZ::Vector3::CreateZero();
        float m_bias = 0.0f;
        float m_error = 0.0f;
        float m_maximumImpulse = AZStd::numeric_limits<float>::max();
        float m_minimumImpulse = -AZStd::numeric_limits<float>::max();
    };

    struct CustomConstraintInfo final
    {
        AZ_TYPE_INFO(CustomConstraintInfo, CustomConstraintInfoTypeId);

        AZ::TypeId m_providerId = AZ::TypeId::CreateNull();
        AZ::u64 m_providerVersion = 0;
        AZ::u32 m_maximumRowCount = 0;
        AZ::u32 m_stateByteCount = 0;
    };

    //! Generates bounded row batches while the native adapter owns warm-start and iterative solving.
    //! Velocity rows solve J * v + bias = 0. Position rows solve error = 0 using the supplied Jacobian.
    //! Callbacks may run concurrently and must not call runtime capabilities or mutate world state.
    class ICustomConstraintProvider
    {
    public:
        AZ_RTTI(ICustomConstraintProvider, ICustomConstraintProviderTypeId);

        virtual ~ICustomConstraintProvider() = default;

        [[nodiscard]]
        virtual AZ::TypeId GetId() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetVersion() const = 0;

        //! Called once when a constraint is created. Zero rejects the configuration.
        [[nodiscard]]
        virtual AZ::u32 GetMaximumRowCount(
            AZStd::span<const AZ::u8> data) const = 0;

        //! Called once at creation. Mutable state is included in deterministic snapshots.
        [[nodiscard]]
        virtual AZ::u32 GetStateByteCount(
            [[maybe_unused]] AZStd::span<const AZ::u8> data) const
        {
            return 0;
        }

        [[nodiscard]]
        virtual bool InitializeState(
            [[maybe_unused]] AZStd::span<const AZ::u8> data,
            const AZStd::span<AZ::u8> state) const
        {
            return state.empty();
        }

        //! Called once per position iteration. Implementations must be deterministic, thread-safe, and allocation-free.
        [[nodiscard]]
        virtual AZ::u32 PreparePositionRows(
            const CustomConstraintContext& context,
            AZStd::span<const AZ::u8> data,
            AZStd::span<AZ::u8> state,
            AZStd::span<CustomConstraintRow> rows) const = 0;

        //! Called once per simulation step. Iterative velocity solves remain entirely native.
        [[nodiscard]]
        virtual AZ::u32 PrepareVelocityRows(
            const CustomConstraintContext& context,
            AZStd::span<const AZ::u8> data,
            AZStd::span<AZ::u8> state,
            AZStd::span<CustomConstraintRow> rows) const = 0;
    };
} // namespace Jolt
