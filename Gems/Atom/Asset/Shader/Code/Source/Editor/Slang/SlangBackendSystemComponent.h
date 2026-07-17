/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include <Slang/SlangBackend.h>

namespace AZ::ShaderBuilder
{
    //! Owns the Slang language backend and registers it with the shader builder through
    //! ShaderCompilerBackendBus. Everything Slang-specific about the shader build lives behind
    //! this component; the builders and the RHI have no knowledge of it.
    class SlangBackendSystemComponent final : public Component
    {
    public:
        AZ_COMPONENT(SlangBackendSystemComponent, "{9B1E63D0-4C7A-4E5F-A1B8-D2C3E4F5A6B7}");

        static void Reflect(ReflectContext* context);

        static void GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;

    private:
        SlangBackend m_backend;
    };
} // namespace AZ::ShaderBuilder
