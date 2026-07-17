/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Editor/ShaderCompilerBackend.h>

namespace AZ::ShaderBuilder
{
    //! The legacy AZSL pipeline behind the language-backend seam: per-API header prepend,
    //! MCPP preprocess, AZSLC transpile to HLSL + reflection JSONs, and per-entry stage
    //! compilation through ShaderPlatformInterface::CompilePlatformInternal (DXC and friends).
    //! This is a mechanical extraction of the pre-seam ShaderAssetBuilder/ShaderVariantAssetBuilder
    //! code; products and behavior are unchanged.
    class AzslcBackend final : public IShaderCompilerBackend
    {
    public:
        AZStd::string_view GetName() const override;

        AZStd::span<const AZStd::string_view> GetSourceExtensions() const override;

        bool CanCompileTarget(const RHI::ShaderTargetDescriptor& targetDescriptor) const override;

        AZ::Outcome<FrontendResult, AZStd::string> CompileFrontend(const FrontendInput& input) override;

        AZ::Outcome<StageResult, AZStd::string> CompileStage(const StageInput& input) override;
    };
} // namespace AZ::ShaderBuilder
