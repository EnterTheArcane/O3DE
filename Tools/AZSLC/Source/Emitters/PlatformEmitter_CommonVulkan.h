/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include "PlatformEmitter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace AZ::ShaderCompiler
{
    // PlatformEmitter is not a Backend by design. It's a supplement to CodeEmitter, not a replacement
    struct PlatformEmitter_CommonVulkan : PlatformEmitter
    {
    public:
        [[nodiscard]]
        std::string GetSpecializationConstant(const CodeEmitter& codeEmitter, const IdentifierUID& symbol, const Options& options) const override;

        [[nodiscard]]
        std::pair<std::string, std::string> GetDataViewHeaderFooter(
            const CodeEmitter& codeEmitter,
            const IdentifierUID& symbol,
            uint32_t bindInfoRegisterIndex,
            std::string_view registerTypeLetter,
            std::optional<std::string> stringifiedLogicalSpace,
            const Options& options) const override;

    protected:
        PlatformEmitter_CommonVulkan() : PlatformEmitter {} {};
    };
}
