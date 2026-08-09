/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string_view.h>
#include <Box3D/Handle.h>
#include <Box3D/Material.h>
#include <Box3D/Statistics.h>
#include <Box3D/TypeIds.h>

namespace Box3D
{
    //! Selects scene details emitted through IDebugRenderer.
    struct DebugDrawSettings final
    {
        AZ::Aabb m_bounds = AZ::Aabb::CreateNull();
        AZ::u64 m_maskBits = (AZStd::numeric_limits<AZ::u64>::max)();
        float m_forceScale = 1.0f;
        float m_jointScale = 1.0f;
        bool m_drawShapes = true;
        bool m_drawJoints = true;
        bool m_drawJointExtras = false;
        bool m_drawBounds = false;
        bool m_drawMass = false;
        bool m_drawBodyNames = false;
        bool m_drawContacts = false;
        AZ::s32 m_drawContactAnchor = -1;
        bool m_drawGraphColors = false;
        bool m_drawContactFeatures = false;
        bool m_drawContactNormals = false;
        bool m_drawContactForces = false;
        bool m_drawFrictionForces = false;
        bool m_drawIslands = false;
    };

    //! Sink for diagnostic scene and replay geometry.
    class IDebugRenderer
    {
    public:
        AZ_RTTI(IDebugRenderer, IDebugRendererTypeId);

        virtual ~IDebugRenderer() = default;
        virtual bool DrawTriangles(
            AZStd::span<const AZ::Vector3> vertices,
            AZStd::span<const AZ::u32> indices,
            const AZ::Transform& transform,
            const AZ::Color& color,
            DebugMaterialPreset material) = 0;
        virtual void DrawLine(const AZ::Vector3& start, const AZ::Vector3& end, const AZ::Color& color) = 0;
        virtual void DrawTransform(const AZ::Transform& transform) = 0;
        virtual void DrawPoint(const AZ::Vector3& position, float size, const AZ::Color& color) = 0;
        virtual void DrawSphere(const AZ::Vector3& position, float radius, const AZ::Color& color, float opacity) = 0;
        virtual void DrawCapsule(const AZ::Vector3& start, const AZ::Vector3& end, float radius, const AZ::Color& color, float opacity) = 0;
        virtual void DrawBounds(const AZ::Aabb& bounds, const AZ::Color& color) = 0;
        virtual void DrawBox(const AZ::Vector3& halfExtents, const AZ::Transform& transform, const AZ::Color& color) = 0;
        virtual void DrawText(const AZ::Vector3& position, AZStd::string_view text, const AZ::Color& color) = 0;
    };

    //! Immutable metadata read from a recording.
    struct ReplayInfo final
    {
        AZ_TYPE_INFO(ReplayInfo, ReplayInfoTypeId);

        AZ::u32 m_frameCount = 0;
        AZ::u32 m_workerCount = 0;
        float m_timeStep = 0.0f;
        AZ::u32 m_subStepCount = 0;
        float m_lengthScale = 1.0f;
        AZ::Aabb m_bounds = AZ::Aabb::CreateNull();
    };

    //! Body state at the current replay frame.
    struct ReplayBody final
    {
        AZ_TYPE_INFO(ReplayBody, ReplayBodyTypeId);

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
        bool m_exists = false;
        bool m_awake = false;
    };

    //! Recorded query operation represented by ReplayQuery.
    enum class ReplayQueryType : AZ::u8
    {
        OverlapAabb,
        OverlapShape,
        Raycast,
        ShapeCast,
        ClosestRaycast,
        MoverCast,
        MoverCollision,
    };

    //! Query metadata captured at the current replay frame.
    struct ReplayQuery final
    {
        AZ_TYPE_INFO(ReplayQuery, ReplayQueryTypeId);

        ReplayQueryType m_type = ReplayQueryType::OverlapAabb;
        AZ::u64 m_categoryBits = 0;
        AZ::u64 m_maskBits = 0;
        AZ::Aabb m_bounds = AZ::Aabb::CreateNull();
        AZ::Vector3 m_origin = AZ::Vector3::CreateZero();
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::u32 m_hitCount = 0;
        AZ::u64 m_key = 0;
        AZ::u64 m_id = 0;
        AZ::Name m_name;
    };

    //! Historical shape identity scoped to one recording.
    struct ReplayShapeId final
    {
        AZ_TYPE_INFO(ReplayShapeId, ReplayShapeIdTypeId);

        static constexpr AZ::u32 InvalidIndex = (AZStd::numeric_limits<AZ::u32>::max)();

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_index != InvalidIndex;
        }

        constexpr bool operator==(const ReplayShapeId&) const noexcept = default;

        AZ::u32 m_index = InvalidIndex;
        AZ::u16 m_generation = 0;
    };

    //! One result produced by a recorded query.
    struct ReplayQueryHit final
    {
        AZ_TYPE_INFO(ReplayQueryHit, ReplayQueryHitTypeId);

        //! Recording-local historical identity. It is not a live ShapeHandle.
        ReplayShapeId m_shapeId;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        float m_fraction = 0.0f;
    };

    //! Seekable deterministic playback of an in-memory recording.
    class IReplay
    {
    public:
        AZ_RTTI(IReplay, IReplayTypeId);
        virtual ~IReplay() = default;

        [[nodiscard]] virtual ReplayInfo GetInfo() const = 0;
        [[nodiscard]] virtual bool Step() = 0;
        virtual void Restart() = 0;
        virtual void Seek(AZ::u32 frame) = 0;
        [[nodiscard]] virtual AZ::u32 GetFrame() const = 0;
        [[nodiscard]] virtual bool IsAtEnd() const = 0;
        [[nodiscard]] virtual bool HasDiverged() const = 0;
        [[nodiscard]] virtual AZ::s32 GetDivergenceFrame() const = 0;
        virtual void SetWorkerCount(AZ::u32 workerCount) = 0;
        virtual void SetKeyframePolicy(size_t budgetBytes, AZ::u32 minimumInterval) = 0;
        [[nodiscard]] virtual size_t GetKeyframeBudget() const = 0;
        [[nodiscard]] virtual AZ::u32 GetMinimumKeyframeInterval() const = 0;
        [[nodiscard]] virtual AZ::u32 GetKeyframeInterval() const = 0;
        [[nodiscard]] virtual size_t GetKeyframeBytes() const = 0;
        [[nodiscard]] virtual AZ::u32 GetBodyCount() const = 0;
        [[nodiscard]] virtual bool GetBody(AZ::u32 index, ReplayBody& body) const = 0;
        [[nodiscard]] virtual AZ::u32 GetFrameQueryCount() const = 0;
        [[nodiscard]] virtual bool GetFrameQuery(AZ::u32 index, ReplayQuery& query) const = 0;
        [[nodiscard]] virtual bool GetFrameQueryHit(AZ::u32 queryIndex, AZ::u32 hitIndex, ReplayQueryHit& hit) const = 0;
        [[nodiscard]] virtual bool Draw(
            const DebugDrawSettings& settings, IDebugRenderer& renderer, AZ::s32 queryIndex = -1, AZ::s32 selectedQueryIndex = -1) = 0;
    };

    //! World statistics, recording, replay, debug drawing, and maintenance operations.
    class IDiagnostics
    {
    public:
        AZ_RTTI(IDiagnostics, IDiagnosticsTypeId);
        virtual ~IDiagnostics() = default;

        [[nodiscard]] virtual bool GetWorldStatistics(
            WorldHandle worldHandle, StatisticsFlags flags, WorldStatistics& statistics) const = 0;

        [[nodiscard]] virtual bool StartRecording(WorldHandle worldHandle, size_t initialCapacityBytes = 0) = 0;
        [[nodiscard]] virtual bool StopRecording(WorldHandle worldHandle, AZStd::vector<AZ::u8>& data) = 0;
        [[nodiscard]] virtual bool ValidateRecording(AZStd::span<const AZ::u8> data, AZ::u32 workerCount = 1) const = 0;
        [[nodiscard]] virtual AZStd::unique_ptr<IReplay> CreateReplay(AZStd::span<const AZ::u8> data, AZ::u32 workerCount = 1) const = 0;
        [[nodiscard]] virtual bool DrawWorld(
            WorldHandle worldHandle, const DebugDrawSettings& settings, IDebugRenderer& renderer) const = 0;
        [[nodiscard]] virtual bool RebuildStaticTree(WorldHandle worldHandle) = 0;
    };
} // namespace Box3D
