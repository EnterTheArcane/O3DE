/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldQueryBus.h>

#include <Jolt/Capabilities.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    namespace
    {
        template<class Hit>
        [[nodiscard]]
        AZ::u32 GetCollectionSize(const AZStd::vector<Hit>& hits)
        {
            return aznumeric_cast<AZ::u32>(hits.size());
        }

        template<class Hit>
        [[nodiscard]]
        Hit GetCollectionItem(
            const AZStd::vector<Hit>& hits,
            const AZ::u32 index)
        {
            if (index < hits.size())
            {
                return hits[index];
            }

            return {};
        }

        template<class Hit>
        [[nodiscard]]
        bool HasCollectionOverflow(
            const AZStd::vector<Hit>& hits,
            const AZ::u32 requiredHitCount)
        {
            return hits.size() < requiredHitCount;
        }

        template<class Hit>
        [[nodiscard]]
        AZ::u32 GetFaceVertexCount(
            const AZStd::vector<Hit>& hits,
            const AZ::u32 hitIndex,
            AZ::u8 Hit::* vertexCount)
        {
            if (hitIndex >= hits.size())
            {
                return 0;
            }

            return hits[hitIndex].*vertexCount;
        }

        [[nodiscard]]
        WorldPosition GetFaceVertex(
            const AZStd::vector<WorldPosition>& vertices,
            const AZ::u32 hitIndex,
            const AZ::u32 vertexIndex,
            const AZ::u32 vertexCount)
        {
            if (vertexIndex >= vertexCount)
            {
                return {};
            }

            const size_t flattenedIndex =
                static_cast<size_t>(hitIndex) * MaximumSupportingFaceVertexCount + vertexIndex;
            if (flattenedIndex >= vertices.size())
            {
                return {};
            }

            return vertices[flattenedIndex];
        }
    } // namespace

    AZ::u32 ClosestShapeCastResult::GetQueryFaceVertexCount() const
    {
        if (!m_found)
        {
            return 0;
        }

        return m_hit.m_queryFaceVertexCount;
    }

    WorldPosition ClosestShapeCastResult::GetQueryFaceVertex(
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_queryFaceVertices,
            0,
            vertexIndex,
            GetQueryFaceVertexCount());
    }

    AZ::u32 ClosestShapeCastResult::GetTargetFaceVertexCount() const
    {
        if (!m_found)
        {
            return 0;
        }

        return m_hit.m_targetFaceVertexCount;
    }

    WorldPosition ClosestShapeCastResult::GetTargetFaceVertex(
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_targetFaceVertices,
            0,
            vertexIndex,
            GetTargetFaceVertexCount());
    }

    bool RaycastRequestCollection::AddRequest(
        const RaycastRequest& request)
    {
        if (m_requests.size() >= MaximumScriptQueryHits)
        {
            return false;
        }

        m_requests.push_back(request);
        return true;
    }

    void RaycastRequestCollection::Clear()
    {
        m_requests.clear();
    }

    AZ::u32 RaycastRequestCollection::GetRequestCount() const
    {
        return GetCollectionSize(m_requests);
    }

    RaycastRequest RaycastRequestCollection::GetRequest(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_requests, index);
    }

    AZStd::span<const RaycastRequest> RaycastRequestCollection::GetRequests() const
    {
        return m_requests;
    }

    AZ::u32 ClosestRaycastResultCollection::GetResultCount() const
    {
        return GetCollectionSize(m_results);
    }

    ClosestRaycastResult ClosestRaycastResultCollection::GetResult(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_results, index);
    }

    AZ::u32 ClosestRaycastResultCollection::GetRequiredResultCount() const
    {
        return m_requiredResultCount;
    }

    bool ClosestRaycastResultCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_results, m_requiredResultCount);
    }

    AZ::u32 BodyCollection::GetBodyCount() const
    {
        return GetCollectionSize(m_bodies);
    }

    BodyHandle BodyCollection::GetBody(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_bodies, index);
    }

    AZ::u32 BodyCollection::GetRequiredBodyCount() const
    {
        return m_requiredBodyCount;
    }

    bool BodyCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_bodies, m_requiredBodyCount);
    }

    AZ::u32 RaycastHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    RaycastHit RaycastHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 RaycastHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool RaycastHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 OverlapHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    OverlapHit OverlapHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 OverlapHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool OverlapHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 ShapeOverlapHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    ShapeOverlapHit ShapeOverlapHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 ShapeOverlapHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    AZ::u32 ShapeOverlapHitCollection::GetQueryFaceVertexCount(
        const AZ::u32 hitIndex) const
    {
        return GetFaceVertexCount(
            m_hits,
            hitIndex,
            &ShapeOverlapHit::m_queryFaceVertexCount);
    }

    WorldPosition ShapeOverlapHitCollection::GetQueryFaceVertex(
        const AZ::u32 hitIndex,
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_queryFaceVertices,
            hitIndex,
            vertexIndex,
            GetQueryFaceVertexCount(hitIndex));
    }

    AZ::u32 ShapeOverlapHitCollection::GetTargetFaceVertexCount(
        const AZ::u32 hitIndex) const
    {
        return GetFaceVertexCount(
            m_hits,
            hitIndex,
            &ShapeOverlapHit::m_targetFaceVertexCount);
    }

    WorldPosition ShapeOverlapHitCollection::GetTargetFaceVertex(
        const AZ::u32 hitIndex,
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_targetFaceVertices,
            hitIndex,
            vertexIndex,
            GetTargetFaceVertexCount(hitIndex));
    }

    bool ShapeOverlapHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 ShapeCastHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    ShapeCastHit ShapeCastHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 ShapeCastHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    AZ::u32 ShapeCastHitCollection::GetQueryFaceVertexCount(
        const AZ::u32 hitIndex) const
    {
        return GetFaceVertexCount(
            m_hits,
            hitIndex,
            &ShapeCastHit::m_queryFaceVertexCount);
    }

    WorldPosition ShapeCastHitCollection::GetQueryFaceVertex(
        const AZ::u32 hitIndex,
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_queryFaceVertices,
            hitIndex,
            vertexIndex,
            GetQueryFaceVertexCount(hitIndex));
    }

    AZ::u32 ShapeCastHitCollection::GetTargetFaceVertexCount(
        const AZ::u32 hitIndex) const
    {
        return GetFaceVertexCount(
            m_hits,
            hitIndex,
            &ShapeCastHit::m_targetFaceVertexCount);
    }

    WorldPosition ShapeCastHitCollection::GetTargetFaceVertex(
        const AZ::u32 hitIndex,
        const AZ::u32 vertexIndex) const
    {
        return GetFaceVertex(
            m_targetFaceVertices,
            hitIndex,
            vertexIndex,
            GetTargetFaceVertexCount(hitIndex));
    }

    bool ShapeCastHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 ShapePointHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    ShapePointHit ShapePointHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 ShapePointHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool ShapePointHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 ShapeRaycastHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    ShapeRaycastHit ShapeRaycastHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 ShapeRaycastHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool ShapeRaycastHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 ShapeTriangleCollection::GetTriangleCount() const
    {
        return GetCollectionSize(m_triangles);
    }

    ShapeTriangle ShapeTriangleCollection::GetTriangle(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_triangles, index);
    }

    AZ::u32 ShapeTriangleCollection::GetRequiredTriangleCount() const
    {
        return m_requiredTriangleCount;
    }

    bool ShapeTriangleCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_triangles, m_requiredTriangleCount);
    }

    AZ::u32 BroadPhaseHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    BroadPhaseHit BroadPhaseHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 BroadPhaseHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool BroadPhaseHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 BroadPhaseCastHitCollection::GetHitCount() const
    {
        return GetCollectionSize(m_hits);
    }

    BroadPhaseCastHit BroadPhaseCastHitCollection::GetHit(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_hits, index);
    }

    AZ::u32 BroadPhaseCastHitCollection::GetRequiredHitCount() const
    {
        return m_requiredHitCount;
    }

    bool BroadPhaseCastHitCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_hits, m_requiredHitCount);
    }

    AZ::u32 TransformedShapeCollection::GetShapeCount() const
    {
        return GetCollectionSize(m_shapes);
    }

    TransformedShape TransformedShapeCollection::GetShape(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_shapes, index);
    }

    AZ::u32 TransformedShapeCollection::GetRequiredShapeCount() const
    {
        return m_requiredShapeCount;
    }

    bool TransformedShapeCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_shapes, m_requiredShapeCount);
    }

    AZ::u32 SupportingFaceVertexCollection::GetVertexCount() const
    {
        return GetCollectionSize(m_vertices);
    }

    WorldPosition SupportingFaceVertexCollection::GetVertex(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_vertices, index);
    }

    AZ::u32 SupportingFaceVertexCollection::GetRequiredVertexCount() const
    {
        return m_requiredVertexCount;
    }

    bool SupportingFaceVertexCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_vertices, m_requiredVertexCount);
    }

    AZ::u32 TransformedTriangleCollection::GetTriangleCount() const
    {
        return GetCollectionSize(m_triangles);
    }

    TransformedTriangle TransformedTriangleCollection::GetTriangle(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_triangles, index);
    }

    AZ::u32 TransformedTriangleCollection::GetRequiredTriangleCount() const
    {
        return m_requiredTriangleCount;
    }

    bool TransformedTriangleCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_triangles, m_requiredTriangleCount);
    }

    void ReflectWorldQueries(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ClosestRaycastResult>()
                ->Field("Hit", &ClosestRaycastResult::m_hit)
                ->Field("Found", &ClosestRaycastResult::m_found);

            serializeContext
                ->Class<ClosestShapeRaycastResult>()
                ->Field("Hit", &ClosestShapeRaycastResult::m_hit)
                ->Field("Found", &ClosestShapeRaycastResult::m_found);

            serializeContext
                ->Class<ClosestShapeCastResult>()
                ->Field("Hit", &ClosestShapeCastResult::m_hit)
                ->Field("Found", &ClosestShapeCastResult::m_found);

            serializeContext
                ->Class<ClosestBroadPhaseCastResult>()
                ->Field("Hit", &ClosestBroadPhaseCastResult::m_hit)
                ->Field("Found", &ClosestBroadPhaseCastResult::m_found);

            serializeContext
                ->Class<BroadPhaseBoundsResult>()
                ->Field("Bounds", &BroadPhaseBoundsResult::m_bounds)
                ->Field("Found", &BroadPhaseBoundsResult::m_found);

            serializeContext
                ->Class<SurfaceNormalResult>()
                ->Field("Normal", &SurfaceNormalResult::m_normal)
                ->Field("Found", &SurfaceNormalResult::m_found);

            serializeContext->Class<RaycastRequestCollection>();
            serializeContext->Class<ClosestRaycastResultCollection>();
            serializeContext->Class<BodyCollection>();
            serializeContext->Class<RaycastHitCollection>();
            serializeContext->Class<OverlapHitCollection>();
            serializeContext->Class<ShapeOverlapHitCollection>();
            serializeContext->Class<ShapeCastHitCollection>();
            serializeContext->Class<ShapePointHitCollection>();
            serializeContext->Class<ShapeRaycastHitCollection>();
            serializeContext->Class<ShapeTriangleCollection>();
            serializeContext->Class<BroadPhaseHitCollection>();
            serializeContext->Class<BroadPhaseCastHitCollection>();
            serializeContext->Class<TransformedShapeCollection>();
            serializeContext->Class<SupportingFaceVertexCollection>();
            serializeContext->Class<TransformedTriangleCollection>();
        }

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->Class<ClosestRaycastResult>("ClosestRaycastResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("hit", BehaviorValueGetter(&ClosestRaycastResult::m_hit), nullptr)
            ->Property("found", BehaviorValueGetter(&ClosestRaycastResult::m_found), nullptr);

        behaviorContext->Class<ClosestShapeRaycastResult>("ClosestShapeRaycastResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("hit", BehaviorValueGetter(&ClosestShapeRaycastResult::m_hit), nullptr)
            ->Property("found", BehaviorValueGetter(&ClosestShapeRaycastResult::m_found), nullptr);

        behaviorContext->Class<ClosestShapeCastResult>("ClosestShapeCastResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetQueryFaceVertexCount", &ClosestShapeCastResult::GetQueryFaceVertexCount)
            ->Method("GetQueryFaceVertex", &ClosestShapeCastResult::GetQueryFaceVertex)
            ->Method("GetTargetFaceVertexCount", &ClosestShapeCastResult::GetTargetFaceVertexCount)
            ->Method("GetTargetFaceVertex", &ClosestShapeCastResult::GetTargetFaceVertex)
            ->Property("hit", BehaviorValueGetter(&ClosestShapeCastResult::m_hit), nullptr)
            ->Property("found", BehaviorValueGetter(&ClosestShapeCastResult::m_found), nullptr);

        behaviorContext->Class<ClosestBroadPhaseCastResult>("ClosestBroadPhaseCastResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("hit", BehaviorValueGetter(&ClosestBroadPhaseCastResult::m_hit), nullptr)
            ->Property("found", BehaviorValueGetter(&ClosestBroadPhaseCastResult::m_found), nullptr);

        behaviorContext->Class<BroadPhaseBoundsResult>("BroadPhaseBoundsResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("bounds", BehaviorValueGetter(&BroadPhaseBoundsResult::m_bounds), nullptr)
            ->Property("found", BehaviorValueGetter(&BroadPhaseBoundsResult::m_found), nullptr);

        behaviorContext->Class<SurfaceNormalResult>("SurfaceNormalResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Property("normal", BehaviorValueGetter(&SurfaceNormalResult::m_normal), nullptr)
            ->Property("found", BehaviorValueGetter(&SurfaceNormalResult::m_found), nullptr);

        behaviorContext->Class<RaycastRequestCollection>("JoltRaycastRequestCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Attribute(AZ::Script::Attributes::Alias, "RaycastRequestCollection")
            ->Attribute(AZ::Script::Attributes::ClassNameOverride, "RaycastRequestCollection")
            ->Method("AddRequest", &RaycastRequestCollection::AddRequest)
            ->Method("Clear", &RaycastRequestCollection::Clear)
            ->Method("GetRequestCount", &RaycastRequestCollection::GetRequestCount)
            ->Method("GetRequest", &RaycastRequestCollection::GetRequest);

        behaviorContext->Class<ClosestRaycastResultCollection>("ClosestRaycastResultCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetResultCount", &ClosestRaycastResultCollection::GetResultCount)
            ->Method("GetResult", &ClosestRaycastResultCollection::GetResult)
            ->Method("GetRequiredResultCount", &ClosestRaycastResultCollection::GetRequiredResultCount)
            ->Method("HasOverflow", &ClosestRaycastResultCollection::HasOverflow);

        behaviorContext->Class<BodyCollection>("BodyCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetBodyCount", &BodyCollection::GetBodyCount)
            ->Method("GetBody", &BodyCollection::GetBody)
            ->Method("GetRequiredBodyCount", &BodyCollection::GetRequiredBodyCount)
            ->Method("HasOverflow", &BodyCollection::HasOverflow);

        behaviorContext->Class<RaycastHitCollection>("RaycastHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &RaycastHitCollection::GetHitCount)
            ->Method("GetHit", &RaycastHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &RaycastHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &RaycastHitCollection::HasOverflow);

        behaviorContext->Class<OverlapHitCollection>("OverlapHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &OverlapHitCollection::GetHitCount)
            ->Method("GetHit", &OverlapHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &OverlapHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &OverlapHitCollection::HasOverflow);

        behaviorContext->Class<ShapeOverlapHitCollection>("ShapeOverlapHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &ShapeOverlapHitCollection::GetHitCount)
            ->Method("GetHit", &ShapeOverlapHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &ShapeOverlapHitCollection::GetRequiredHitCount)
            ->Method("GetQueryFaceVertexCount", &ShapeOverlapHitCollection::GetQueryFaceVertexCount)
            ->Method("GetQueryFaceVertex", &ShapeOverlapHitCollection::GetQueryFaceVertex)
            ->Method("GetTargetFaceVertexCount", &ShapeOverlapHitCollection::GetTargetFaceVertexCount)
            ->Method("GetTargetFaceVertex", &ShapeOverlapHitCollection::GetTargetFaceVertex)
            ->Method("HasOverflow", &ShapeOverlapHitCollection::HasOverflow);

        behaviorContext->Class<ShapeCastHitCollection>("ShapeCastHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &ShapeCastHitCollection::GetHitCount)
            ->Method("GetHit", &ShapeCastHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &ShapeCastHitCollection::GetRequiredHitCount)
            ->Method("GetQueryFaceVertexCount", &ShapeCastHitCollection::GetQueryFaceVertexCount)
            ->Method("GetQueryFaceVertex", &ShapeCastHitCollection::GetQueryFaceVertex)
            ->Method("GetTargetFaceVertexCount", &ShapeCastHitCollection::GetTargetFaceVertexCount)
            ->Method("GetTargetFaceVertex", &ShapeCastHitCollection::GetTargetFaceVertex)
            ->Method("HasOverflow", &ShapeCastHitCollection::HasOverflow);

        behaviorContext->Class<ShapePointHitCollection>("ShapePointHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &ShapePointHitCollection::GetHitCount)
            ->Method("GetHit", &ShapePointHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &ShapePointHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &ShapePointHitCollection::HasOverflow);

        behaviorContext->Class<ShapeRaycastHitCollection>("ShapeRaycastHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &ShapeRaycastHitCollection::GetHitCount)
            ->Method("GetHit", &ShapeRaycastHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &ShapeRaycastHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &ShapeRaycastHitCollection::HasOverflow);

        behaviorContext->Class<ShapeTriangleCollection>("ShapeTriangleCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetTriangleCount", &ShapeTriangleCollection::GetTriangleCount)
            ->Method("GetTriangle", &ShapeTriangleCollection::GetTriangle)
            ->Method("GetRequiredTriangleCount", &ShapeTriangleCollection::GetRequiredTriangleCount)
            ->Method("HasOverflow", &ShapeTriangleCollection::HasOverflow);

        behaviorContext->Class<BroadPhaseHitCollection>("BroadPhaseHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &BroadPhaseHitCollection::GetHitCount)
            ->Method("GetHit", &BroadPhaseHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &BroadPhaseHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &BroadPhaseHitCollection::HasOverflow);

        behaviorContext->Class<BroadPhaseCastHitCollection>("BroadPhaseCastHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetHitCount", &BroadPhaseCastHitCollection::GetHitCount)
            ->Method("GetHit", &BroadPhaseCastHitCollection::GetHit)
            ->Method("GetRequiredHitCount", &BroadPhaseCastHitCollection::GetRequiredHitCount)
            ->Method("HasOverflow", &BroadPhaseCastHitCollection::HasOverflow);

        behaviorContext->Class<SupportingFaceVertexCollection>("SupportingFaceVertexCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetVertexCount", &SupportingFaceVertexCollection::GetVertexCount)
            ->Method("GetVertex", &SupportingFaceVertexCollection::GetVertex)
            ->Method("GetRequiredVertexCount", &SupportingFaceVertexCollection::GetRequiredVertexCount)
            ->Method("HasOverflow", &SupportingFaceVertexCollection::HasOverflow);

        behaviorContext->Class<TransformedTriangleCollection>("TransformedTriangleCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Method("GetTriangleCount", &TransformedTriangleCollection::GetTriangleCount)
            ->Method("GetTriangle", &TransformedTriangleCollection::GetTriangle)
            ->Method("GetRequiredTriangleCount", &TransformedTriangleCollection::GetRequiredTriangleCount)
            ->Method("HasOverflow", &TransformedTriangleCollection::HasOverflow);

        behaviorContext->EBus<WorldQueryRequestBus>("JoltWorldQueryRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Event("CreateWorld", &IWorldQueryRequests::CreateWorld)
            ->Event("DestroyWorld", &IWorldQueryRequests::DestroyWorld)
            ->Event("GetDefaultWorldHandle", &IWorldQueryRequests::GetDefaultWorldHandle)
            ->Event("GetRuntimeInfo", &IWorldQueryRequests::GetRuntimeInfo)
            ->Event("IsWorldValid", &IWorldQueryRequests::IsWorldValid)
            ->Event("StepWorld", &IWorldQueryRequests::StepWorld)
            ->Event("GetGravity", &IWorldQueryRequests::GetGravity)
            ->Event("SetGravity", &IWorldQueryRequests::SetGravity)
            ->Event("GetSimulationConfiguration", &IWorldQueryRequests::GetSimulationConfiguration)
            ->Event("UpdateSimulationConfiguration", &IWorldQueryRequests::UpdateSimulationConfiguration)
            ->Event("GetRuntimeConfiguration", &IWorldQueryRequests::GetRuntimeConfiguration)
            ->Event("UpdateRuntimeConfiguration", &IWorldQueryRequests::UpdateRuntimeConfiguration)
            ->Event("CaptureWorldState", &IWorldQueryRequests::CaptureWorldState)
            ->Event("CaptureWorldStateConfigured", &IWorldQueryRequests::CaptureWorldStateConfigured)
            ->Event("CaptureWorldStateParts", &IWorldQueryRequests::CaptureWorldStateParts)
            ->Event("ExportWorldStateArchive", &IWorldQueryRequests::ExportWorldStateArchive)
            ->Event("ImportWorldStateArchive", &IWorldQueryRequests::ImportWorldStateArchive)
            ->Event("RecaptureWorldState", &IWorldQueryRequests::RecaptureWorldState)
            ->Event("RecaptureWorldStateConfigured", &IWorldQueryRequests::RecaptureWorldStateConfigured)
            ->Event("DestroyStateSnapshot", &IWorldQueryRequests::DestroyStateSnapshot)
            ->Event("IsStateSnapshotValid", &IWorldQueryRequests::IsStateSnapshotValid)
            ->Event("RestoreWorldState", &IWorldQueryRequests::RestoreWorldState)
            ->Event("RestoreWorldStateParts", &IWorldQueryRequests::RestoreWorldStateParts)
            ->Event("ValidateWorldState", &IWorldQueryRequests::ValidateWorldState)
            ->Event("GetWorldStateDigest", &IWorldQueryRequests::GetWorldStateDigest)
            ->Event("GetWorldStatistics", &IWorldQueryRequests::GetWorldStatistics)
            ->Event("ConfigurePerformanceStatistics", &IWorldQueryRequests::ConfigurePerformanceStatistics)
            ->Event("GetPerformanceStatistics", &IWorldQueryRequests::GetPerformanceStatistics)
            ->Event("ConfigureDebugCapture", &IWorldQueryRequests::ConfigureDebugCapture)
            ->Event("GetDebugCaptureStatistics", &IWorldQueryRequests::GetDebugCaptureStatistics)
            ->Event("GetBodies", &IWorldQueryRequests::GetBodies)
            ->Event("GetBodyId", &IWorldQueryRequests::GetBodyId)
            ->Event("RaycastShapeClosest", &IWorldQueryRequests::RaycastShapeClosest)
            ->Event("RaycastShapeAll", &IWorldQueryRequests::RaycastShapeAll)
            ->Event("CollideShapePoint", &IWorldQueryRequests::CollideShapePoint)
            ->Event("CollideShapePointAny", &IWorldQueryRequests::CollideShapePointAny)
            ->Event("CollectShapeTriangles", &IWorldQueryRequests::CollectShapeTriangles)
            ->Event("RaycastClosest", &IWorldQueryRequests::RaycastClosest)
            ->Event("RaycastClosestBatch", &IWorldQueryRequests::RaycastClosestBatch)
            ->Event("RaycastClosestPerBody", &IWorldQueryRequests::RaycastClosestPerBody)
            ->Event("RaycastAny", &IWorldQueryRequests::RaycastAny)
            ->Event("RaycastAll", &IWorldQueryRequests::RaycastAll)
            ->Event("OverlapPoint", &IWorldQueryRequests::OverlapPoint)
            ->Event("OverlapPointAny", &IWorldQueryRequests::OverlapPointAny)
            ->Event("CollideShape", &IWorldQueryRequests::CollideShape)
            ->Event("OverlapShape", &IWorldQueryRequests::OverlapShape)
            ->Event("OverlapShapeAny", &IWorldQueryRequests::OverlapShapeAny)
            ->Event("CastShapeClosest", &IWorldQueryRequests::CastShapeClosest)
            ->Event("CastShapeClosestPerBody", &IWorldQueryRequests::CastShapeClosestPerBody)
            ->Event("CastShapeAll", &IWorldQueryRequests::CastShapeAll)
            ->Event("OverlapBroadPhase", &IWorldQueryRequests::OverlapBroadPhase)
            ->Event("OverlapBroadPhaseAny", &IWorldQueryRequests::OverlapBroadPhaseAny)
            ->Event("CastBroadPhaseClosest", &IWorldQueryRequests::CastBroadPhaseClosest)
            ->Event("CastBroadPhaseAll", &IWorldQueryRequests::CastBroadPhaseAll)
            ->Event("GetSupportingFace", &IWorldQueryRequests::GetSupportingFace)
            ->Event("CollectTriangles", &IWorldQueryRequests::CollectTriangles)
            ->Event("GetBroadPhaseBounds", &IWorldQueryRequests::GetBroadPhaseBounds)
            ->Event("OptimizeBroadPhase", &IWorldQueryRequests::OptimizeBroadPhase)
            ->Event("WereBodiesInContact", &IWorldQueryRequests::WereBodiesInContact);
    }
} // namespace Jolt
