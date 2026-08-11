/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <PlatformEmitter.h>

#include <string>

namespace AZ::ShaderCompiler
{
    // PlatformEmitter is not a Backend by design. It's a supplement to CodeEmitter, not a replacement
    class PlatformEmitter_DX12 : public PlatformEmitter
    {
    public:
        //! This method will be called once and only once when the platform emitter registers itself to the system.
        //! Returns a singleton object of this class.
        static const PlatformEmitter* RegisterPlatformEmitter() noexcept(false);

        [[nodiscard]]
        std::string GetRootSig(
            const CodeEmitter& codeEmitter,
            const RootSigDesc& rootSig,
            const Options& options,
            BindingPair::Set signatureQuery) const final;

        bool RequiresUniqueSpaceForUnboundedArrays() const override
        {
            return true;
        }

        [[nodiscard]]
        std::string GetSpecializationConstant(
            const CodeEmitter& codeEmitter,
            const IdentifierUID& symbol,
            const Options& options) const override;

    private:
        PlatformEmitter_DX12()
            : PlatformEmitter{}
        {
        };
    };
}
