/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Operation.h>
#include <Jolt/Query.h>
#include <Jolt/WorldTypes.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class RaycastBatchOperationResult final
    {
    public:
        [[nodiscard]]
        const BufferResult& GetBufferResult() const
        {
            return m_bufferResult;
        }

        [[nodiscard]]
        AZStd::span<const ClosestRaycastResult> GetResults() const &
        {
            return m_results;
        }

        AZStd::span<const ClosestRaycastResult> GetResults() const && = delete;

    private:
        friend class RuntimeImplementation;

        template<class Result, class Work>
        friend class Internal::TypedOperationRecord;

        void Reset()
        {
            m_bufferResult = {};
            m_results.clear();
        }

        BufferResult m_bufferResult;
        AZStd::vector<ClosestRaycastResult> m_results;
    };

    class JOLT_API WorldQueries
    {
    public:
        [[nodiscard]]
        static WorldQueries* Get();

        [[nodiscard]]
        bool RaycastShapeClosest(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const;

        [[nodiscard]]
        QueryResult RaycastShapeAll(
            WorldHandle worldHandle,
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const;

        [[nodiscard]]
        QueryResult CollideShapePoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const;

        [[nodiscard]]
        bool CollideShapePointAny(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const;

        [[nodiscard]]
        QueryResult CollectShapeTriangles(
            WorldHandle worldHandle,
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const;

        [[nodiscard]]
        bool RaycastTransformedShapeClosest(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const;

        [[nodiscard]]
        QueryResult RaycastTransformedShapeAll(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapePoint(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool CollideTransformedShapePointAny(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const;

        [[nodiscard]]
        QueryResult CollectTransformedShapeChildren(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const;

        [[nodiscard]]
        QueryResult CollectTransformedShapeTriangles(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const;

        [[nodiscard]]
        bool GetTransformedShapeSurfaceNormal(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        QueryResult GetTransformedShapeSupportingFace(
            WorldHandle worldHandle,
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const;

        [[nodiscard]]
        bool RetainShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const WorldTransform& transform,
            float uniformScale,
            TransformedShape& shape) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        bool CollideTransformedShapes(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            ITransformedShapeCollisionCollector& collector) const;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        bool CastTransformedShape(
            WorldHandle worldHandle,
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            ITransformedShapeCastCollector& collector) const;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            WorldHandle worldHandle,
            const ShapePlacement& firstShape,
            const ShapePlacement& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        bool RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            RaycastHit& hit) const;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const;

        //! Query callbacks are rejected because their lifetime cannot be retained by an asynchronous operation.
        [[nodiscard]]
        Operation<RaycastBatchOperationResult> RaycastClosestBatchAsync(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests) const;

        [[nodiscard]]
        QueryResult RaycastClosestPerBody(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        bool RaycastAny(
            WorldHandle worldHandle,
            const RaycastRequest& request) const;

        [[nodiscard]]
        QueryResult RaycastAll(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const;

        [[nodiscard]]
        QueryResult OverlapPoint(
            WorldHandle worldHandle,
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool OverlapPointAny(
            WorldHandle worldHandle,
            const PointOverlapRequest& request) const;

        [[nodiscard]]
        QueryResult CollideShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult OverlapShape(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const;

        [[nodiscard]]
        bool OverlapShapeAny(
            WorldHandle worldHandle,
            const ShapeOverlapRequest& request) const;

        [[nodiscard]]
        bool CastShapeClosest(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult CastShapeClosestPerBody(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult CastShapeAll(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const;

        [[nodiscard]]
        QueryResult OverlapBroadPhase(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const;

        [[nodiscard]]
        bool OverlapBroadPhaseAny(
            WorldHandle worldHandle,
            const BroadPhaseOverlapRequest& request) const;

        [[nodiscard]]
        bool CastBroadPhaseClosest(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const;

        [[nodiscard]]
        QueryResult CastBroadPhaseAll(
            WorldHandle worldHandle,
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const;

        [[nodiscard]]
        QueryResult CollectShapesInBounds(
            WorldHandle worldHandle,
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const;

        [[nodiscard]]
        QueryResult GetSupportingFace(
            WorldHandle worldHandle,
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const;

        [[nodiscard]]
        QueryResult CollectTriangles(
            WorldHandle worldHandle,
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const;

        [[nodiscard]]
        bool GetBroadPhaseBounds(
            WorldHandle worldHandle,
            BroadPhaseAabb& bounds) const;

        bool OptimizeBroadPhase(WorldHandle worldHandle);

        [[nodiscard]]
        bool WereBodiesInContact(
            WorldHandle worldHandle,
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const;

    private:
        friend class Runtime;

        WorldQueries() = default;
        ~WorldQueries() = default;

        static AZStd::atomic<WorldQueries*> s_instance;
    };
} // namespace Jolt
