/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>

namespace AzFramework
{
    namespace Components
    {
        // Make Init() Optional

        template<typename T>
        void TryInit(T& controller)
        {
            if constexpr (requires(T& type) { type.Init(); })
            {
                controller.Init();
            }
        }

        template<typename T>
        void TryActivate(T& controller, const AZ::EntityComponentIdPair& entityComponentIdPair)
        {
            if constexpr (requires(T& type, const AZ::EntityComponentIdPair& pair) { type.Activate(pair); })
            {
                controller.Activate(entityComponentIdPair);
            }
            else if constexpr (requires(T& type, AZ::EntityId id) { type.Activate(id); })
            {
                controller.Activate(entityComponentIdPair.GetEntityId());
            }
        }

        // Make GetProvidedServices, GetDependentServicesHelper, GetRequiredServices and GetIncompatibleServices optional.

        template<typename T>
        void GetProvidedServicesHelper(AZ::ComponentDescriptor::DependencyArrayType&, const AZStd::false_type&) {}

        template<typename T>
        void GetProvidedServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services, const AZStd::true_type&)
        {
            T::GetProvidedServices(services);
        }

        template<typename T>
        void GetDependentServicesHelper(AZ::ComponentDescriptor::DependencyArrayType&, const AZStd::false_type&) {}

        template<typename T>
        void GetDependentServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services, const AZStd::true_type&)
        {
            T::GetDependentServices(services);
        }

        template<typename T>
        void GetRequiredServicesHelper(AZ::ComponentDescriptor::DependencyArrayType&, const AZStd::false_type&) {}

        template<typename T>
        void GetRequiredServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services, const AZStd::true_type&)
        {
            T::GetRequiredServices(services);
        }

        template<typename T>
        void GetIncompatibleServicesHelper(AZ::ComponentDescriptor::DependencyArrayType&, const AZStd::false_type&) {}

        template<typename T>
        void GetIncompatibleServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services, const AZStd::true_type&)
        {
            T::GetIncompatibleServices(services);
        }
    } // namespace Components
} // namespace AzFramework
