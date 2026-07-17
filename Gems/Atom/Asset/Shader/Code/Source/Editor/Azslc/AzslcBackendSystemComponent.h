/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include "AzslcBackend.h"

namespace AZ::ShaderBuilder
{
    //! Owns the AZSLC language backend and registers it on ShaderCompilerBackendBus for the
    //! lifetime of the component. This is the registration shape every language backend uses —
    //! in-gem flavors like this one and backends living in other gems alike — keeping the
    //! generic ShaderBuilderSystemComponent free of any language knowledge.
    class AzslcBackendSystemComponent final
        : public Component
    {
    public:
        AZ_COMPONENT(AzslcBackendSystemComponent, "{B7E097DE-2E22-4F92-A4A7-3D0E63D8136B}");

        static void Reflect(ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        AzslcBackend m_backend;
    };
} // namespace AZ::ShaderBuilder
