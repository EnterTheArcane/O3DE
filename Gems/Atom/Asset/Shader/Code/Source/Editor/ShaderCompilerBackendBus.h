/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string_view.h>

namespace AZ::ShaderBuilder
{
    class IShaderCompilerBackend;

    //! Registration and lookup of shader language backends, mirroring how RHI backends register
    //! through RHI::ShaderPlatformInterfaceRegisterBus: providers broadcast their backend during
    //! component activation, the shader builder system component is the single handler that stores
    //! the registered backends, and the builders look backends up by source extension at job time.
    //! A language backend living in another gem only needs this header to participate.
    class ShaderCompilerBackendRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~ShaderCompilerBackendRequests() = default;

        //! Registers @backend. Duplicate extension claims are a deterministic hard error
        //! (the registration is rejected), never last-writer-wins.
        virtual void RegisterShaderCompilerBackend(IShaderCompilerBackend* backend) = 0;

        virtual void UnregisterShaderCompilerBackend(IShaderCompilerBackend* backend) = 0;

        //! @param extensionWithDot Source file extension including the dot, e.g. ".azsl".
        //! @returns the backend claiming @extensionWithDot, or nullptr.
        virtual IShaderCompilerBackend* FindShaderCompilerBackendForSourceExtension(AZStd::string_view extensionWithDot) = 0;
    };
    using ShaderCompilerBackendBus = AZ::EBus<ShaderCompilerBackendRequests>;
} // namespace AZ::ShaderBuilder
