/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Effects.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>

namespace Box3D
{
    //! Triggers the explosion effect owned by an entity.
    class ExplosionRequests
        : public AZ::EBusTraits
    {
    public:
        using BusIdType = AZ::EntityId;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        [[nodiscard]]
        virtual bool Explode() = 0;

        [[nodiscard]]
        virtual ExplosionConfiguration GetConfiguration() const = 0;

        virtual void UpdateConfiguration(const ExplosionConfiguration& configuration) = 0;
    };

    using ExplosionRequestBus = AZ::EBus<ExplosionRequests>;

    //! Controls the wind effect owned by an entity.
    class WindRequests
        : public AZ::EBusTraits
    {
    public:
        using BusIdType = AZ::EntityId;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual void ApplyWind() = 0;

        [[nodiscard]]
        virtual WindConfiguration GetConfiguration() const = 0;

        virtual void UpdateConfiguration(const WindConfiguration& configuration) = 0;

        virtual void SetEnabled(bool enabled) = 0;

        [[nodiscard]]
        virtual bool IsEnabled() const = 0;
    };

    using WindRequestBus = AZ::EBus<WindRequests>;
} // namespace Box3D
