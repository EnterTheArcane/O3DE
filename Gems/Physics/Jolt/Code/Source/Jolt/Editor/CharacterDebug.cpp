/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/CharacterDebug.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Viewport/ViewportColors.h>

#include <cmath>

namespace Jolt::Editor
{
    namespace
    {
        struct GuideLine final
        {
            AZ::Vector3 m_start = AZ::Vector3::CreateZero();
            AZ::Vector3 m_end = AZ::Vector3::CreateZero();
        };

        struct CharacterGuides final
        {
            static constexpr AZ::u32 MaximumLineCount = 16;

            void AddLine(
                const AZ::Vector3& start,
                const AZ::Vector3& end)
            {
                if (m_lineCount < m_lines.size())
                {
                    m_lines[m_lineCount] = {
                        .m_start = start,
                        .m_end = end,
                    };
                    ++m_lineCount;
                }
            }

            AZStd::array<GuideLine, MaximumLineCount> m_lines;
            AZ::u32 m_lineCount = 0;
        };

        [[nodiscard]]
        CharacterGuides BuildCharacterGuides(const CharacterGuideConfiguration& configuration)
        {
            CharacterGuides guides;
            const auto addLocalLine = [&configuration, &guides](
                const AZ::Vector3& start,
                const AZ::Vector3& end)
            {
                guides.AddLine(
                    configuration.m_entityTransform.TransformPoint(start),
                    configuration.m_entityTransform.TransformPoint(end));
            };

            if (configuration.m_up.IsFinite() && !configuration.m_up.IsZero())
            {
                const AZ::Vector3 up = configuration.m_up.GetNormalized();
                addLocalLine(AZ::Vector3::CreateZero(), up);

                if (AZ::IsFiniteFloat(configuration.m_maximumSlopeAngle)
                    && configuration.m_maximumSlopeAngle >= 0.0f
                    && configuration.m_maximumSlopeAngle <= AZ::Constants::HalfPi)
                {
                    const AZ::Vector3 tangent = up.GetOrthogonalVector().GetNormalized();
                    const AZ::Vector3 bitangent = up.Cross(tangent).GetNormalized();
                    const float vertical = std::cos(configuration.m_maximumSlopeAngle);
                    const float horizontal = std::sin(configuration.m_maximumSlopeAngle);
                    addLocalLine(AZ::Vector3::CreateZero(), up * vertical + tangent * horizontal);
                    addLocalLine(AZ::Vector3::CreateZero(), up * vertical - tangent * horizontal);
                    addLocalLine(AZ::Vector3::CreateZero(), up * vertical + bitangent * horizontal);
                    addLocalLine(AZ::Vector3::CreateZero(), up * vertical - bitangent * horizontal);
                }
            }

            if (configuration.m_supportingPlaneNormal.IsFinite()
                && !configuration.m_supportingPlaneNormal.IsZero()
                && AZ::IsFiniteFloat(configuration.m_supportingPlaneDistance)
                && std::abs(configuration.m_supportingPlaneDistance) < 10'000.0f)
            {
                constexpr float planeHalfExtent = 0.5f;
                const AZ::Vector3 normal = configuration.m_supportingPlaneNormal.GetNormalized();
                const AZ::Vector3 tangent = normal.GetOrthogonalVector().GetNormalized();
                const AZ::Vector3 bitangent = normal.Cross(tangent).GetNormalized();
                const AZ::Vector3 center = -configuration.m_supportingPlaneDistance * normal;
                addLocalLine(center - planeHalfExtent * tangent, center + planeHalfExtent * tangent);
                addLocalLine(center - planeHalfExtent * bitangent, center + planeHalfExtent * bitangent);
            }

            if (configuration.m_drawShapeOffset && configuration.m_shapeOffset.IsFinite())
            {
                constexpr float markerHalfExtent = 0.05f;
                addLocalLine(AZ::Vector3::CreateZero(), configuration.m_shapeOffset);
                addLocalLine(
                    configuration.m_shapeOffset - AZ::Vector3::CreateAxisX(markerHalfExtent),
                    configuration.m_shapeOffset + AZ::Vector3::CreateAxisX(markerHalfExtent));
                addLocalLine(
                    configuration.m_shapeOffset - AZ::Vector3::CreateAxisY(markerHalfExtent),
                    configuration.m_shapeOffset + AZ::Vector3::CreateAxisY(markerHalfExtent));
                addLocalLine(
                    configuration.m_shapeOffset - AZ::Vector3::CreateAxisZ(markerHalfExtent),
                    configuration.m_shapeOffset + AZ::Vector3::CreateAxisZ(markerHalfExtent));
            }

            if (configuration.m_drawStepVectors && configuration.m_shapeOffset.IsFinite())
            {
                if (configuration.m_stairsStepDown.IsFinite())
                {
                    addLocalLine(
                        configuration.m_shapeOffset,
                        configuration.m_shapeOffset + configuration.m_stairsStepDown);
                }
                if (configuration.m_stairsStepUp.IsFinite())
                {
                    addLocalLine(
                        configuration.m_shapeOffset,
                        configuration.m_shapeOffset + configuration.m_stairsStepUp);
                }
                if (configuration.m_stickToFloorStepDown.IsFinite())
                {
                    addLocalLine(
                        configuration.m_shapeOffset,
                        configuration.m_shapeOffset + configuration.m_stickToFloorStepDown);
                }
            }

            return guides;
        }
    } // namespace

    void DrawCharacterGuides(
        AzFramework::DebugDisplayRequests& debugDisplay,
        const CharacterGuideConfiguration& configuration)
    {
        const CharacterGuides guides = BuildCharacterGuides(configuration);
        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        for (AZ::u32 lineIndex = 0; lineIndex < guides.m_lineCount; ++lineIndex)
        {
            debugDisplay.DrawLine(
                guides.m_lines[lineIndex].m_start,
                guides.m_lines[lineIndex].m_end);
        }
    }

    AZ::Aabb CalculateCharacterGuideBounds(const CharacterGuideConfiguration& configuration)
    {
        const CharacterGuides guides = BuildCharacterGuides(configuration);
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        for (AZ::u32 lineIndex = 0; lineIndex < guides.m_lineCount; ++lineIndex)
        {
            bounds.AddPoint(guides.m_lines[lineIndex].m_start);
            bounds.AddPoint(guides.m_lines[lineIndex].m_end);
        }
        if (bounds.IsValid())
        {
            bounds.Expand(AZ::Vector3(0.05f));
        }
        return bounds;
    }
} // namespace Jolt::Editor
