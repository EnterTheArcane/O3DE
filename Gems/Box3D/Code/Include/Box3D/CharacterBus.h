/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/CharacterConfiguration.h>

#include <AzCore/Component/ComponentBus.h>

namespace Box3D
{
    class CharacterRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;
        virtual bool DisableSimulation() = 0;
        [[nodiscard]] virtual bool IsSimulationEnabled() const = 0;
        [[nodiscard]] virtual WorldHandle GetWorldHandle() const = 0;
        [[nodiscard]] virtual CharacterHandle GetCharacterHandle() const = 0;
        [[nodiscard]] virtual CharacterConfiguration GetConfiguration() const = 0;
        virtual bool UpdateConfiguration(const CharacterConfiguration& configuration) = 0;
        [[nodiscard]] virtual CharacterState GetState() const = 0;
        virtual bool Move(const AZ::Vector3& velocity, float fixedTimeStep) = 0;
    };

    using CharacterRequestBus = AZ::EBus<CharacterRequests>;

    class CharacterNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnCharacterMoved([[maybe_unused]] const CharacterState& state)
        {
        }
    };

    using CharacterNotificationBus = AZ::EBus<CharacterNotifications>;
} // namespace Box3D
