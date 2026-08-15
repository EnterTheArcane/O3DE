/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Character.h>
#include <Jolt/Event.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/base.h>

namespace Jolt
{
    class IVirtualCharacterRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        using BusIdType = AZ::EntityId;

        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual VirtualCharacterHandle GetCharacterHandle() const = 0;

        [[nodiscard]]
        virtual VirtualCharacterState GetState() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetUserData() const = 0;

        virtual bool SetUserData(AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual VirtualCharacterRuntimeConfiguration GetRuntimeConfiguration() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<CharacterCollisionHit> CheckCollision(
            const CharacterCollisionRequest& request) const = 0;

        virtual bool UpdateRuntimeConfiguration(
            const VirtualCharacterRuntimeConfiguration& configuration) = 0;

        virtual bool SetTransform(const WorldTransform& transform) = 0;

        virtual bool SetVelocity(const AZ::Vector3& velocity) = 0;

        [[nodiscard]]
        virtual AZ::Vector3 CancelVelocityTowardsSteepSlopes(
            const AZ::Vector3& desiredVelocity) const = 0;

        virtual bool BeginContactTracking() = 0;

        virtual bool EndContactTracking() = 0;

        [[nodiscard]]
        virtual bool CanWalkStairs(const AZ::Vector3& desiredVelocity) const = 0;

        virtual bool WalkStairs(
            const VirtualCharacterStairConfiguration& configuration) = 0;

        virtual bool StickToFloor(const AZ::Vector3& stepDown) = 0;

        virtual bool RefreshContacts() = 0;

        virtual bool UpdateGroundVelocity() = 0;

        [[nodiscard]]
        virtual AZStd::vector<VirtualCharacterContact> GetContacts() const = 0;

        [[nodiscard]]
        virtual bool HasCollidedWithBody(BodyHandle bodyHandle) const = 0;

        [[nodiscard]]
        virtual bool HasCollidedWithCharacter(
            VirtualCharacterHandle characterHandle) const = 0;

        virtual bool UpdateConfiguration(
            const VirtualCharacterUpdateConfiguration& configuration) = 0;
    };

    using VirtualCharacterRequestBus = AZ::EBus<IVirtualCharacterRequests>;

    class IVirtualCharacterNotifications
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        using BusIdType = AZ::EntityId;

        virtual void OnCharacterCreated(VirtualCharacterHandle characterHandle)
        {
            AZ_UNUSED(characterHandle);
        }

        virtual void OnCharacterDestroyed(VirtualCharacterHandle characterHandle)
        {
            AZ_UNUSED(characterHandle);
        }

        virtual void OnCharacterDestroying(VirtualCharacterHandle characterHandle)
        {
            AZ_UNUSED(characterHandle);
        }

        virtual void OnCharacterMoved(const VirtualCharacterMoveEvent& event)
        {
            AZ_UNUSED(event);
        }
    };

    using VirtualCharacterNotificationBus = AZ::EBus<IVirtualCharacterNotifications>;
} // namespace Jolt
