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
        //////////////////////////////////////////////////////////////////////////
        // Helper functions to make certain functions optional in the class wrapped by
        // EditorComponentAdapter and ComponentAdapter.

        // Make Init() Optional

        template<typename T>
        struct ComponentInitHelper
        {
            static void Init(T& controller)
            {
                if constexpr (requires { controller.Init(); })
                {
                    controller.Init();
                }
            }
        };

        template<typename T>
        struct ComponentActivateHelper
        {
            static void Activate(T& controller, const AZ::EntityComponentIdPair& entityComponentIdPair)
            {
                if constexpr (requires { controller.Activate(entityComponentIdPair); })
                {
                    controller.Activate(entityComponentIdPair);
                }
                else if constexpr (requires { controller.Activate(entityComponentIdPair.GetEntityId()); })
                {
                    controller.Activate(entityComponentIdPair.GetEntityId());
                }
            }
        };

        // Make GetProvidedServices, GetDependentServicesHelper, GetRequiredServices and GetIncompatibleServices optional.

        template<typename T>
        void GetProvidedServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            if constexpr (AZ::HasComponentProvidedServices<T>::value)
            {
                T::GetProvidedServices(services);
            }
        }

        template<typename T>
        void GetDependentServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            if constexpr (AZ::HasComponentDependentServices<T>::value)
            {
                T::GetDependentServices(services);
            }
        }

        template<typename T>
        void GetRequiredServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            if constexpr (AZ::HasComponentRequiredServices<T>::value)
            {
                T::GetRequiredServices(services);
            }
        }

        template<typename T>
        void GetIncompatibleServicesHelper(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            if constexpr (AZ::HasComponentIncompatibleServices<T>::value)
            {
                T::GetIncompatibleServices(services);
            }
        }
    } // namespace Components
} // namespace AzFramework
