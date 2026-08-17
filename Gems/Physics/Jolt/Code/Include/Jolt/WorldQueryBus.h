/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>
#include <Jolt/DebugDraw.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/Query.h>
#include <Jolt/SystemConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    struct RuntimeInfo;

    inline constexpr AZ::u32 MaximumScriptQueryHits = 4'096;

    void ReflectWorldQueries(AZ::ReflectContext* context);

    struct ClosestShapeCastResult final
    {
        AZ_TYPE_INFO(ClosestShapeCastResult, ClosestShapeCastResultTypeId);

        [[nodiscard]]
        AZ::u32 GetQueryFaceVertexCount() const;

        [[nodiscard]]
        WorldPosition GetQueryFaceVertex(AZ::u32 vertexIndex) const;

        [[nodiscard]]
        AZ::u32 GetTargetFaceVertexCount() const;

        [[nodiscard]]
        WorldPosition GetTargetFaceVertex(AZ::u32 vertexIndex) const;

        ShapeCastHit m_hit;
        bool m_found = false;

    private:
        friend class SystemComponent;

        AZStd::vector<WorldPosition> m_queryFaceVertices;
        AZStd::vector<WorldPosition> m_targetFaceVertices;
    };

    struct ClosestShapeRaycastResult final
    {
        AZ_TYPE_INFO(ClosestShapeRaycastResult, ClosestShapeRaycastResultTypeId);

        ShapeRaycastHit m_hit;
        bool m_found = false;
    };

    struct SurfaceNormalResult final
    {
        AZ_TYPE_INFO(SurfaceNormalResult, SurfaceNormalResultTypeId);

        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        bool m_found = false;
    };

    struct ClosestBroadPhaseCastResult final
    {
        AZ_TYPE_INFO(ClosestBroadPhaseCastResult, ClosestBroadPhaseCastResultTypeId);

        BroadPhaseCastHit m_hit;
        bool m_found = false;
    };

    struct BroadPhaseBoundsResult final
    {
        AZ_TYPE_INFO(BroadPhaseBoundsResult, BroadPhaseBoundsResultTypeId);

        BroadPhaseAabb m_bounds;
        bool m_found = false;
    };

    class RaycastRequestCollection final
    {
    public:
        AZ_TYPE_INFO(RaycastRequestCollection, RaycastRequestCollectionTypeId);

        bool AddRequest(const RaycastRequest& request);

        void Clear();

        [[nodiscard]]
        AZ::u32 GetRequestCount() const;

        [[nodiscard]]
        RaycastRequest GetRequest(AZ::u32 index) const;

        [[nodiscard]]
        AZStd::span<const RaycastRequest> GetRequests() const;

    private:
        AZStd::vector<RaycastRequest> m_requests;
    };

    class ClosestRaycastResultCollection final
    {
    public:
        AZ_TYPE_INFO(ClosestRaycastResultCollection, ClosestRaycastResultCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetResultCount() const;

        [[nodiscard]]
        ClosestRaycastResult GetResult(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredResultCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ClosestRaycastResult> m_results;
        AZ::u32 m_requiredResultCount = 0;
    };

    class BodyCollection final
    {
    public:
        AZ_TYPE_INFO(BodyCollection, BodyCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetBodyCount() const;

        [[nodiscard]]
        BodyHandle GetBody(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredBodyCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<BodyHandle> m_bodies;
        AZ::u32 m_requiredBodyCount = 0;
    };

    class RaycastHitCollection final
    {
    public:
        AZ_TYPE_INFO(RaycastHitCollection, RaycastHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        RaycastHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<RaycastHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class OverlapHitCollection final
    {
    public:
        AZ_TYPE_INFO(OverlapHitCollection, OverlapHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        OverlapHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<OverlapHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class ShapeOverlapHitCollection final
    {
    public:
        AZ_TYPE_INFO(ShapeOverlapHitCollection, ShapeOverlapHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        ShapeOverlapHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        AZ::u32 GetQueryFaceVertexCount(AZ::u32 hitIndex) const;

        [[nodiscard]]
        WorldPosition GetQueryFaceVertex(
            AZ::u32 hitIndex,
            AZ::u32 vertexIndex) const;

        [[nodiscard]]
        AZ::u32 GetTargetFaceVertexCount(AZ::u32 hitIndex) const;

        [[nodiscard]]
        WorldPosition GetTargetFaceVertex(
            AZ::u32 hitIndex,
            AZ::u32 vertexIndex) const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ShapeOverlapHit> m_hits;
        AZStd::vector<WorldPosition> m_queryFaceVertices;
        AZStd::vector<WorldPosition> m_targetFaceVertices;
        AZ::u32 m_requiredHitCount = 0;
    };

    class ShapeCastHitCollection final
    {
    public:
        AZ_TYPE_INFO(ShapeCastHitCollection, ShapeCastHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        ShapeCastHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        AZ::u32 GetQueryFaceVertexCount(AZ::u32 hitIndex) const;

        [[nodiscard]]
        WorldPosition GetQueryFaceVertex(
            AZ::u32 hitIndex,
            AZ::u32 vertexIndex) const;

        [[nodiscard]]
        AZ::u32 GetTargetFaceVertexCount(AZ::u32 hitIndex) const;

        [[nodiscard]]
        WorldPosition GetTargetFaceVertex(
            AZ::u32 hitIndex,
            AZ::u32 vertexIndex) const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ShapeCastHit> m_hits;
        AZStd::vector<WorldPosition> m_queryFaceVertices;
        AZStd::vector<WorldPosition> m_targetFaceVertices;
        AZ::u32 m_requiredHitCount = 0;
    };

    class ShapePointHitCollection final
    {
    public:
        AZ_TYPE_INFO(ShapePointHitCollection, ShapePointHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        ShapePointHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ShapePointHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class ShapeRaycastHitCollection final
    {
    public:
        AZ_TYPE_INFO(ShapeRaycastHitCollection, ShapeRaycastHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        ShapeRaycastHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ShapeRaycastHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class ShapeTriangleCollection final
    {
    public:
        AZ_TYPE_INFO(ShapeTriangleCollection, ShapeTriangleCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetTriangleCount() const;

        [[nodiscard]]
        ShapeTriangle GetTriangle(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredTriangleCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<ShapeTriangle> m_triangles;
        AZ::u32 m_requiredTriangleCount = 0;
    };

    class BroadPhaseHitCollection final
    {
    public:
        AZ_TYPE_INFO(BroadPhaseHitCollection, BroadPhaseHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        BroadPhaseHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<BroadPhaseHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class BroadPhaseCastHitCollection final
    {
    public:
        AZ_TYPE_INFO(BroadPhaseCastHitCollection, BroadPhaseCastHitCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetHitCount() const;

        [[nodiscard]]
        BroadPhaseCastHit GetHit(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredHitCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<BroadPhaseCastHit> m_hits;
        AZ::u32 m_requiredHitCount = 0;
    };

    class TransformedShapeCollection final
    {
    public:
        AZ_TYPE_INFO(TransformedShapeCollection, TransformedShapeCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetShapeCount() const;

        [[nodiscard]]
        TransformedShape GetShape(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredShapeCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<TransformedShape> m_shapes;
        AZ::u32 m_requiredShapeCount = 0;
    };

    class SupportingFaceVertexCollection final
    {
    public:
        AZ_TYPE_INFO(SupportingFaceVertexCollection, SupportingFaceVertexCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetVertexCount() const;

        [[nodiscard]]
        WorldPosition GetVertex(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredVertexCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<WorldPosition> m_vertices;
        AZ::u32 m_requiredVertexCount = 0;
    };

    class TransformedTriangleCollection final
    {
    public:
        AZ_TYPE_INFO(TransformedTriangleCollection, TransformedTriangleCollectionTypeId);

        [[nodiscard]]
        AZ::u32 GetTriangleCount() const;

        [[nodiscard]]
        TransformedTriangle GetTriangle(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredTriangleCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

    private:
        friend class SystemComponent;

        AZStd::vector<TransformedTriangle> m_triangles;
        AZ::u32 m_requiredTriangleCount = 0;
    };

    class IWorldQueryRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual ~IWorldQueryRequests() = default;

        [[nodiscard]]
        virtual WorldHandle CreateWorld(const WorldConfiguration& configuration) = 0;

        virtual bool DestroyWorld(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual WorldHandle GetDefaultWorldHandle() const = 0;

        [[nodiscard]]
        virtual RuntimeInfo GetRuntimeInfo() const = 0;

        [[nodiscard]]
        virtual bool IsWorldValid(WorldHandle worldHandle) const = 0;

        [[nodiscard]]
        virtual SimulationResult StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetGravity(WorldHandle worldHandle) const = 0;

        virtual bool SetGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) = 0;

        [[nodiscard]]
        virtual SimulationConfiguration GetSimulationConfiguration(WorldHandle worldHandle) const = 0;

        virtual bool UpdateSimulationConfiguration(
            WorldHandle worldHandle,
            const SimulationConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual WorldRuntimeConfiguration GetRuntimeConfiguration(WorldHandle worldHandle) const = 0;

        virtual bool UpdateRuntimeConfiguration(
            WorldHandle worldHandle,
            const WorldRuntimeConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldState(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual StateSnapshotHandle CaptureWorldStateConfigured(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) = 0;

        [[nodiscard]]
        virtual AZStd::vector<StateSnapshotHandle> CaptureWorldStateParts(
            WorldHandle worldHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles,
            const AZStd::vector<AZ::u32>& partitionBodyCounts) = 0;

        virtual bool ExportWorldStateArchive(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles,
            StateSnapshotArchive& archive) = 0;

        [[nodiscard]]
        virtual AZStd::vector<StateSnapshotHandle> ImportWorldStateArchive(
            WorldHandle worldHandle,
            const StateSnapshotArchive& archive) = 0;

        virtual bool RecaptureWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        virtual bool RecaptureWorldStateConfigured(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            const AZStd::vector<BodyHandle>& bodyHandles) = 0;

        virtual bool DestroyStateSnapshot(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        [[nodiscard]]
        virtual bool IsStateSnapshotValid(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) const = 0;

        virtual bool RestoreWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle) = 0;

        //! Prevalidates a batch returned by CaptureWorldStateParts before beginning restore.
        virtual bool RestoreWorldStateParts(
            WorldHandle worldHandle,
            const AZStd::vector<StateSnapshotHandle>& snapshotHandles) = 0;

        virtual bool ValidateWorldState(
            WorldHandle worldHandle,
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result) = 0;

        [[nodiscard]]
        virtual bool GetWorldStateDigest(
            WorldHandle worldHandle,
            WorldStateDigest& digest) const = 0;

        [[nodiscard]]
        virtual bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const = 0;

        virtual bool ConfigurePerformanceStatistics(
            WorldHandle worldHandle,
            PerformanceStatisticsFlags flags) = 0;

        [[nodiscard]]
        virtual bool GetPerformanceStatistics(
            WorldHandle worldHandle,
            WorldPerformanceStatistics& statistics,
            bool reset) = 0;

        virtual bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const = 0;

        [[nodiscard]]
        virtual BodyCollection GetBodies(
            WorldHandle worldHandle,
            BodyKind kind,
            bool activeOnly,
            AZ::u32 maximumBodyCount) const = 0;

        [[nodiscard]]
        virtual bool GetBodyId(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyId& bodyId) const = 0;

        [[nodiscard]]
        virtual ClosestShapeRaycastResult RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual ShapeRaycastHitCollection RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual ShapePointHitCollection CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition) const = 0;

        [[nodiscard]]
        virtual ShapeTriangleCollection CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZ::u32 maximumTriangleCount) const = 0;

        [[nodiscard]]
        virtual ClosestRaycastResult RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual RaycastHitCollection RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual OverlapHitCollection CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position) const = 0;

        [[nodiscard]]
        virtual TransformedShapeCollection CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZ::u32 maximumShapeCount) const = 0;

        [[nodiscard]]
        virtual TransformedTriangleCollection CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZ::u32 maximumTriangleCount) const = 0;

        [[nodiscard]]
        virtual SurfaceNormalResult GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position) const = 0;

        [[nodiscard]]
        virtual SupportingFaceVertexCollection GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZ::u32 maximumVertexCount) const = 0;

        [[nodiscard]]
        virtual ClosestRaycastResult RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual ClosestRaycastResultCollection RaycastClosestBatch(
            WorldHandle worldHandle,
            const RaycastRequestCollection& requests) const = 0;

        [[nodiscard]]
        virtual RaycastHitCollection RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const = 0;

        [[nodiscard]]
        virtual RaycastHitCollection RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual OverlapHitCollection OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual ShapeOverlapHitCollection CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual OverlapHitCollection OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual ClosestShapeCastResult CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request) const = 0;

        [[nodiscard]]
        virtual ShapeCastHitCollection CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual ShapeCastHitCollection CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual BroadPhaseHitCollection OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const = 0;

        [[nodiscard]]
        virtual ClosestBroadPhaseCastResult CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request) const = 0;

        [[nodiscard]]
        virtual BroadPhaseCastHitCollection CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZ::u32 maximumHitCount) const = 0;

        [[nodiscard]]
        virtual TransformedShapeCollection CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZ::u32 maximumShapeCount) const = 0;

        [[nodiscard]]
        virtual SupportingFaceVertexCollection GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZ::u32 maximumVertexCount) const = 0;

        [[nodiscard]]
        virtual TransformedTriangleCollection CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZ::u32 maximumTriangleCount) const = 0;

        [[nodiscard]]
        virtual BroadPhaseBoundsResult GetBroadPhaseBounds(WorldHandle worldHandle) const = 0;

        virtual bool OptimizeBroadPhase(WorldHandle worldHandle) = 0;

        [[nodiscard]]
        virtual bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const = 0;
    };

    using WorldQueryRequestBus = AZ::EBus<IWorldQueryRequests>;
} // namespace Jolt
