/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Collision.h>
#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace AZ::RHI
{
    class Buffer;
    class FrameGraphBuilder;
} // namespace AZ::RHI

namespace Jolt
{
    inline constexpr size_t HairBendMultiplierCount = 4;

    struct HairGradient final
    {
        AZ_TYPE_INFO(HairGradient, HairGradientTypeId);

        float m_minimum = 0.0f;
        float m_maximum = 1.0f;
        float m_minimumFraction = 0.0f;
        float m_maximumFraction = 1.0f;
    };

    struct HairMaterialConfiguration final
    {
        AZ_TYPE_INFO(HairMaterialConfiguration, HairMaterialConfigurationTypeId);

        HairGradient m_globalPose{.m_minimum = 0.01f, .m_maximum = 0.0f, .m_maximumFraction = 0.3f};
        HairGradient m_gravityFactor{
            .m_minimum = 0.1f,
            .m_maximum = 1.0f,
            .m_minimumFraction = 0.2f,
            .m_maximumFraction = 0.8f,
        };
        HairGradient m_gridVelocityFactor{.m_minimum = 0.05f, .m_maximum = 0.01f};
        HairGradient m_radius{.m_minimum = 0.001f, .m_maximum = 0.001f};
        HairGradient m_skinGlobalPose{.m_minimum = 1.0f, .m_maximum = 0.0f, .m_maximumFraction = 0.1f};
        HairGradient m_worldTransformInfluence;

        AZStd::array<float, HairBendMultiplierCount> m_bendComplianceMultipliers{1.0f, 100.0f, 100.0f, 1.0f};

        float m_angularDamping = 2.0f;
        float m_bendCompliance = 1.0e-7f;
        float m_friction = 0.2f;
        float m_gravityPreloadFactor = 0.0f;
        float m_gridDensityForceFactor = 0.0f;
        float m_inertiaMultiplier = 10.0f;
        float m_linearDamping = 2.0f;
        float m_maximumAngularVelocity = 50.0f;
        float m_maximumLinearVelocity = 10.0f;
        float m_simulationStrandFraction = 0.1f;
        float m_stretchCompliance = 1.0e-8f;

        bool m_enableCollision = true;
        bool m_enableLongRangeAttachments = true;
    };

    struct HairVertex final
    {
        AZ_TYPE_INFO(HairVertex, HairVertexTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        float m_inverseMass = 1.0f;
    };

    struct HairStrand final
    {
        AZ_TYPE_INFO(HairStrand, HairStrandTypeId);

        AZ::u32 m_beginVertex = 0;
        AZ::u32 m_endVertex = 0;
        AZ::u32 m_materialIndex = 0;
    };

    struct HairTriangle final
    {
        AZ_TYPE_INFO(HairTriangle, HairTriangleTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
    };

    struct HairSkinWeight final
    {
        AZ_TYPE_INFO(HairSkinWeight, HairSkinWeightTypeId);

        AZ::u32 m_jointIndex = 0;
        float m_weight = 0.0f;
    };

    struct HairDefinitionConfiguration final
    {
        AZ_TYPE_INFO(HairDefinitionConfiguration, HairDefinitionConfigurationTypeId);

        AZStd::vector<HairVertex> m_vertices;
        AZStd::vector<HairStrand> m_strands;
        AZStd::vector<HairMaterialConfiguration> m_materials;

        AZStd::vector<AZ::Vector3> m_scalpVertices;
        AZStd::vector<HairTriangle> m_scalpTriangles;
        AZStd::vector<AZ::Transform> m_scalpInverseBindPoses;
        AZStd::vector<HairSkinWeight> m_scalpSkinWeights;

        AZ::Vector3 m_initialGravity{0.0f, -9.81f, 0.0f};
        AZ::Vector3 m_simulationBoundsPadding = AZ::Vector3::CreateOne() * 0.1f;
        AZ::u32 m_gridSizeX = 32;
        AZ::u32 m_gridSizeY = 32;
        AZ::u32 m_gridSizeZ = 32;
        AZ::u32 m_iterationsPerSecond = 360;
        AZ::u32 m_scalpSkinWeightsPerVertex = 0;
        AZ::u32 m_verticesPerStrand = 0;
        float m_maximumDeltaTime = 1.0f / 30.0f;
    };

    struct HairConfiguration final
    {
        AZ_TYPE_INFO(HairConfiguration, HairConfigurationTypeId);

        WorldTransform m_worldTransform;
        AZ::Transform m_scalpToHeadTransform = AZ::Transform::CreateIdentity();
        HairDefinitionHandle m_definitionHandle;
        ObjectLayer m_objectLayer = ObjectLayer::Invalid;
    };

    struct HairDefinitionState final
    {
        AZ_TYPE_INFO(HairDefinitionState, HairDefinitionStateTypeId);

        AZ::Aabb m_simulationBounds = AZ::Aabb::CreateNull();
        AZ::u32 m_gridCellCount = 0;
        AZ::u32 m_jointCount = 0;
        AZ::u32 m_maximumVerticesPerStrand = 0;
        AZ::u32 m_paddedSimulationVertexCount = 0;
        AZ::u32 m_renderVertexCount = 0;
        AZ::u32 m_scalpVertexCount = 0;
        AZ::u32 m_simulationVertexCount = 0;
        float m_densityScale = 0.0f;
        float m_maximumHairToScalpDistanceSquared = 0.0f;
    };

    struct HairState final
    {
        AZ_TYPE_INFO(HairState, HairStateTypeId);

        WorldTransform m_worldTransform;
        AZ::Transform m_scalpToHeadTransform = AZ::Transform::CreateIdentity();
        AZ::u32 m_renderVertexCount = 0;
        AZ::u32 m_simulationVertexCount = 0;
        AZ::u32 m_scalpVertexCount = 0;

        AZ::u32 m_gridCellCount = 0;
        AZ::u32 m_gridSizeX = 0;
        AZ::u32 m_gridSizeY = 0;
        AZ::u32 m_gridSizeZ = 0;

        bool m_teleported = false;
    };

    struct HairVertexState final
    {
        AZ_TYPE_INFO(HairVertexState, HairVertexStateTypeId);

        AZ::Vector3 m_localPosition = AZ::Vector3::CreateZero();
        AZ::Quaternion m_localRotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_localLinearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_localAngularVelocity = AZ::Vector3::CreateZero();
    };

    struct HairGridCellState final
    {
        AZ_TYPE_INFO(HairGridCellState, HairGridCellStateTypeId);

        [[nodiscard]]
        AZ::Vector3 GetVelocity() const
        {
            return AZ::Vector3(m_velocityAndDensity);
        }

        [[nodiscard]]
        float GetDensity() const
        {
            return m_velocityAndDensity.GetW();
        }

        AZ::Vector4 m_velocityAndDensity = AZ::Vector4::CreateZero();
    };

    struct HairReadbackBuffers final
    {
        AZ_TYPE_INFO(HairReadbackBuffers, HairReadbackBuffersTypeId);

        AZStd::span<HairVertexState> m_vertexStates;
        AZStd::span<AZ::Vector3> m_renderPositions;
        AZStd::span<AZ::Vector3> m_scalpPositions;
        AZStd::span<HairGridCellState> m_gridCells;
    };

    struct HairReadbackResult final
    {
        AZ_TYPE_INFO(HairReadbackResult, HairReadbackResultTypeId);

        QueryResult m_vertexStates;
        QueryResult m_renderPositions;
        QueryResult m_scalpPositions;
        QueryResult m_gridCells;
    };

    struct HairRenderBuffer final
    {
        //! A renderable position buffer remains valid for the hair lifetime. Its contents are owned by
        //! rendering until the matching token is returned through ImportHairRenderBufferHandoff.
        [[nodiscard]]
        explicit operator bool() const
        {
            return m_buffer
                && !m_attachmentId.IsEmpty()
                && m_token != 0
                && m_vertexCount != 0
                && m_stride != 0;
        }

        AZ::Name m_attachmentId;
        AZ::RHI::Buffer* m_buffer = nullptr;
        AZ::u64 m_token = 0;
        AZ::u32 m_vertexCount = 0;
        AZ::u32 m_stride = 0;
    };
} // namespace Jolt
