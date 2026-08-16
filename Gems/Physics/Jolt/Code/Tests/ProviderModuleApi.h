/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomConstraint.h>
#include <Jolt/CustomShape.h>
#include <Jolt/Path.h>

#include <AzCore/base.h>

namespace Jolt::Tests
{
    inline constexpr const char* GetCustomConstraintProviderFunctionName =
        "JoltGetCustomConstraintProvider";
    inline constexpr const char* GetCustomPathProviderFunctionName =
        "JoltGetCustomPathProvider";
    inline constexpr const char* GetCustomShapeProviderFunctionName =
        "JoltGetCustomShapeProvider";
    inline constexpr const char* GetProviderEvidenceFunctionName =
        "JoltGetProviderEvidence";
    inline constexpr const char* ResetProviderEvidenceFunctionName =
        "JoltResetProviderEvidence";

    struct ProviderEvidence final
    {
        AZ::u32 m_castCount = 0;
        AZ::u32 m_collisionCount = 0;
        AZ::u32 m_cookCount = 0;
        AZ::u32 m_pathSampleCount = 0;
        AZ::u32 m_positionConstraintCount = 0;
        AZ::u32 m_velocityConstraintCount = 0;
    };

    using GetCustomConstraintProviderFunction = ICustomConstraintProvider* (*)();
    using GetCustomPathProviderFunction = ICustomPathProvider* (*)();
    using GetCustomShapeProviderFunction = ICustomShapeProvider* (*)();
    using GetProviderEvidenceFunction = bool (*)(ProviderEvidence* evidence);
    using ResetProviderEvidenceFunction = void (*)();
} // namespace Jolt::Tests
