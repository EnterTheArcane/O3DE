/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include <Atom/RHI.Edit/ShaderPlatformInterfaceBus.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include "ShaderAssetBuilder.h"
#include "ShaderCompilerBackendBus.h"
#include "ShaderVariantAssetBuilder.h"
#include "PrecompiledShaderBuilder.h"
#include "ShaderPlatformInterfaceRequest.h"
#include "ShaderVariantListBuilder.h"

namespace AZ
{
    namespace ShaderBuilder
    {
        //! Hosts the language-independent shader build machinery: the asset builders, the
        //! registered RHI ShaderPlatformInterfaces, and the registered shader language backends
        //! (ShaderCompilerBackendBus). This component has no language knowledge: every language
        //! flavor — in-gem (AzslcBackendSystemComponent) or in another gem — owns its backend in
        //! its own component and registers it on the bus.
        class ShaderBuilderSystemComponent
            : public Component
            , public RHI::ShaderPlatformInterfaceRegisterBus::Handler
            , public ShaderPlatformInterfaceRequestBus::Handler
            , public ShaderCompilerBackendBus::Handler
        {
        public:
            AZ_COMPONENT(ShaderBuilderSystemComponent, "{56B5B944-8AF4-4478-A047-8DFDE38DA681}");

            static void Reflect(ReflectContext* context);

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
            static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        protected:
            ////////////////////////////////////////////////////////////////////////
            // AZ::Component interface implementation
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            ////////////////////////////////////////////////////////////////////////

            // ShaderPlatformInterfaceRegisterBus interface overrides ...
            void RegisterShaderPlatformHandler(RHI::ShaderPlatformInterface* shaderPlatformInterface) override;
            void UnregisterShaderPlatformHandler(RHI::ShaderPlatformInterface* shaderPlatformInterface) override;

            // ShaderPlatformInterfaceRequestBus interface overrides ...
            AZStd::vector<RHI::ShaderPlatformInterface*> GetShaderPlatformInterface(const AssetBuilderSDK::PlatformInfo& platformInfo) override;

            // ShaderCompilerBackendBus interface overrides ...
            void RegisterShaderCompilerBackend(IShaderCompilerBackend* backend) override;
            void UnregisterShaderCompilerBackend(IShaderCompilerBackend* backend) override;
            IShaderCompilerBackend* FindShaderCompilerBackendForSourceExtension(AZStd::string_view extensionWithDot) override;

        private:
            //! Storage behind ShaderCompilerBackendBus: the registered language backends.
            AZStd::vector<IShaderCompilerBackend*> m_shaderCompilerBackends;

            ShaderAssetBuilder m_shaderAssetBuilder;

            // The ShaderVariantAssetBuilder can be disabled with this registry key.
            // By default it is enabled. A user might want to disable it when doing look development
            // work with shaders or doing lots of iterative changes to shaders. In these cases
            // GPU performance doesn't matter at all so it is important to not waste time
            // building ShaderVariantAssets (Other than the Root ShaderVariantAsset, of course.).
            static constexpr char EnableShaderVariantAssetBuilderRegistryKey[] = "/O3DE/Atom/Shaders/BuildVariants";
            bool m_enableShaderVariantAssetBuilder = true;

            ShaderVariantAssetBuilder m_shaderVariantAssetBuilder;

            PrecompiledShaderBuilder m_precompiledShaderBuilder;

            ShaderVariantListBuilder m_shaderVariantListBuilder;

            /// Contains the ShaderPlatformInterface for all registered RHIs
            AZStd::unordered_map<RHI::APIType, RHI::ShaderPlatformInterface*> m_shaderPlatformInterfaces;
        };

    } // ShaderBuilder
} //AZ
