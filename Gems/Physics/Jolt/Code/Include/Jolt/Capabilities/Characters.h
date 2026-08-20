/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Character.h>
#include <Jolt/Configuration.h>
#include <Jolt/Query.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Characters
    {
    public:
        [[nodiscard]]
        static Characters* Get();

        [[nodiscard]]
        VirtualCharacterHandle CreateVirtualCharacter(
            WorldHandle worldHandle,
            const VirtualCharacterConfiguration& configuration);

        bool DestroyVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetVirtualCharacterState(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterState& state) const;

        [[nodiscard]]
        bool GetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetVirtualCharacterUserData(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            VirtualCharacterRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        QueryResult CheckVirtualCharacterCollision(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const;

        bool UpdateVirtualCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterRuntimeConfiguration& configuration);

        bool SetVirtualCharacterShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetVirtualCharacterInnerBodyShape(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle);

        bool SetVirtualCharacterTransform(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const WorldTransform& transform);

        bool SetVirtualCharacterVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        [[nodiscard]]
        bool CancelVirtualCharacterVelocityTowardsSteepSlopes(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity,
            AZ::Vector3& adjustedVelocity) const;

        bool BeginVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        bool EndVirtualCharacterContactTracking(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        bool SetVirtualCharacterContactCallbacks(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            IVirtualCharacterContactCallbacks* callbacks);

        [[nodiscard]]
        bool CanVirtualCharacterWalkStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity) const;

        bool WalkVirtualCharacterStairs(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter = nullptr);

        bool StickVirtualCharacterToFloor(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter = nullptr);

        bool RefreshVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const IQueryFilter* filter = nullptr);

        bool UpdateVirtualCharacterGroundVelocity(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        QueryResult GetVirtualCharacterContacts(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            AZStd::span<VirtualCharacterContact> contacts) const;

        [[nodiscard]]
        bool HasVirtualCharacterCollidedWith(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool HaveVirtualCharactersCollided(
            WorldHandle worldHandle,
            VirtualCharacterHandle firstCharacterHandle,
            VirtualCharacterHandle secondCharacterHandle) const;

        bool UpdateVirtualCharacter(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool EnableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool DisableVirtualCharacterAutoUpdate(
            WorldHandle worldHandle,
            VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        CharacterHandle CreateCharacter(
            WorldHandle worldHandle,
            const CharacterConfiguration& configuration);

        bool DestroyCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetCharacterState(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterState& state) const;

        [[nodiscard]]
        bool GetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetCharacterUserData(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        QueryResult CheckCharacterCollision(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter = nullptr) const;

        bool UpdateCharacterRuntimeConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterRuntimeConfiguration& configuration);

        bool SetCharacterShape(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetCharacterTransform(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetCharacterVelocity(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        bool AddCharacterImpulse(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& impulse);

    private:
        friend class Runtime;

        Characters() = default;
        ~Characters() = default;

        static AZStd::atomic<Characters*> s_instance;
    };
} // namespace Jolt
