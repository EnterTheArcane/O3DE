/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Math/Vector3.h>

namespace AzFramework
{
    class DebugDisplayRequests;
} // namespace AzFramework

namespace Jolt::Editor
{
    struct CharacterGuideConfiguration final
    {
        AZ::Matrix3x4 m_entityTransform = AZ::Matrix3x4::CreateIdentity();

        AZ::Vector3 m_shapeOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stairsStepDown = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stairsStepUp = AZ::Vector3::CreateZero();
        AZ::Vector3 m_stickToFloorStepDown = AZ::Vector3::CreateZero();
        AZ::Vector3 m_supportingPlaneNormal = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 m_up = AZ::Vector3::CreateAxisZ();

        float m_maximumSlopeAngle = 0.0f;
        float m_supportingPlaneDistance = -1.0e10f;

        bool m_drawShapeOffset = false;
        bool m_drawStepVectors = false;
    };

    void DrawCharacterGuides(
        AzFramework::DebugDisplayRequests& debugDisplay,
        const CharacterGuideConfiguration& configuration);

    [[nodiscard]]
    AZ::Aabb CalculateCharacterGuideBounds(const CharacterGuideConfiguration& configuration);
} // namespace Jolt::Editor
