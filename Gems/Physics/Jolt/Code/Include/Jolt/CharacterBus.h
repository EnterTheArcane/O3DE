/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Character.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class ICharacterRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual CharacterHandle GetCharacterHandle() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual WorldTransform GetCenterOfMassTransform() const = 0;

        [[nodiscard]]
        virtual CharacterState GetState() const = 0;

        [[nodiscard]]
        virtual CharacterRuntimeConfiguration GetRuntimeConfiguration() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<CharacterCollisionHit> CheckCollision(
            const CharacterCollisionRequest& request) const = 0;

        virtual bool UpdateRuntimeConfiguration(
            const CharacterRuntimeConfiguration& configuration) = 0;

        virtual bool SetTransform(
            const WorldTransform& transform,
            bool activate) = 0;

        virtual bool SetVelocity(const AZ::Vector3& velocity) = 0;

        virtual bool AddImpulse(const AZ::Vector3& impulse) = 0;
    };

    using CharacterRequestBus = AZ::EBus<ICharacterRequests>;
} // namespace Jolt
