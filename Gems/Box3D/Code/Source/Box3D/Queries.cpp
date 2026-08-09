/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Queries.h>
#include <Box3D/WorldBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Box3D
{
    void ReflectQueries(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Enum<static_cast<AZ::u8>(QueryBodyTypes::None)>("QueryBodyTypes_None")
            ->Enum<static_cast<AZ::u8>(QueryBodyTypes::Static)>("QueryBodyTypes_Static")
            ->Enum<static_cast<AZ::u8>(QueryBodyTypes::Kinematic)>("QueryBodyTypes_Kinematic")
            ->Enum<static_cast<AZ::u8>(QueryBodyTypes::Dynamic)>("QueryBodyTypes_Dynamic")
            ->Enum<static_cast<AZ::u8>(QueryBodyTypes::All)>("QueryBodyTypes_All");
        behaviorContext->Class<QueryFilter>("QueryFilter")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Property("collisionFilter", BehaviorValueProperty(&QueryFilter::m_collisionFilter))
            ->Property("bodyTypes", BehaviorValueProperty(&QueryFilter::m_bodyTypes))
            ->Property("includeSensors", BehaviorValueProperty(&QueryFilter::m_includeSensors));
        behaviorContext->Class<QueryHit>("QueryHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("bodyHandle", BehaviorValueProperty(&QueryHit::m_bodyHandle))
            ->Property("shapeHandle", BehaviorValueProperty(&QueryHit::m_shapeHandle))
            ->Property("materialHandle", BehaviorValueProperty(&QueryHit::m_materialHandle))
            ->Property("position", BehaviorValueProperty(&QueryHit::m_position))
            ->Property("normal", BehaviorValueProperty(&QueryHit::m_normal))
            ->Property("distance", BehaviorValueProperty(&QueryHit::m_distance))
            ->Property("fraction", BehaviorValueProperty(&QueryHit::m_fraction))
            ->Property("faceIndex", BehaviorValueProperty(&QueryHit::m_faceIndex))
            ->Property("childIndex", BehaviorValueProperty(&QueryHit::m_childIndex));
        behaviorContext->Class<OverlapHit>("OverlapHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("bodyHandle", BehaviorValueProperty(&OverlapHit::m_bodyHandle))
            ->Property("shapeHandle", BehaviorValueProperty(&OverlapHit::m_shapeHandle));
        behaviorContext->Class<GeometryHit>("GeometryHit")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("position", BehaviorValueProperty(&GeometryHit::m_position))
            ->Property("normal", BehaviorValueProperty(&GeometryHit::m_normal))
            ->Property("distance", BehaviorValueProperty(&GeometryHit::m_distance))
            ->Property("fraction", BehaviorValueProperty(&GeometryHit::m_fraction))
            ->Property("materialIndex", BehaviorValueProperty(&GeometryHit::m_materialIndex))
            ->Property("faceIndex", BehaviorValueProperty(&GeometryHit::m_faceIndex))
            ->Property("childIndex", BehaviorValueProperty(&GeometryHit::m_childIndex));
        behaviorContext->Class<RaycastRequest>("RaycastRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Property("start", BehaviorValueProperty(&RaycastRequest::m_start))
            ->Property("direction", BehaviorValueProperty(&RaycastRequest::m_direction))
            ->Property("filter", BehaviorValueProperty(&RaycastRequest::m_filter))
            ->Property("distance", BehaviorValueProperty(&RaycastRequest::m_distance));
        behaviorContext->Class<RaycastRequestCollection>("RaycastRequestCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Method("AddRequest", &RaycastRequestCollection::AddRequest)
            ->Method("Clear", &RaycastRequestCollection::Clear)
            ->Method("GetRequestCount", &RaycastRequestCollection::GetRequestCount)
            ->Method("GetRequest", &RaycastRequestCollection::GetRequest);
        behaviorContext->Class<AabbOverlapRequest>("AabbOverlapRequest")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Property("aabb", BehaviorValueProperty(&AabbOverlapRequest::m_aabb))
            ->Property("filter", BehaviorValueProperty(&AabbOverlapRequest::m_filter));
        behaviorContext->Class<ClosestQueryResult>("ClosestQueryResult")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("hit", BehaviorValueProperty(&ClosestQueryResult::m_hit))
            ->Property("found", BehaviorValueProperty(&ClosestQueryResult::m_found));
        behaviorContext->Class<ClosestQueryResultCollection>("ClosestQueryResultCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("requiredResultCount", BehaviorValueProperty(&ClosestQueryResultCollection::m_requiredResultCount))
            ->Method("GetResultCount", &ClosestQueryResultCollection::GetResultCount)
            ->Method("GetResult", &ClosestQueryResultCollection::GetResult)
            ->Method("HasOverflow", &ClosestQueryResultCollection::HasOverflow);
        behaviorContext->Class<QueryHitCollection>("QueryHitCollection")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("requiredHitCount", BehaviorValueProperty(&QueryHitCollection::m_requiredHitCount))
            ->Method("GetHitCount", &QueryHitCollection::GetHitCount)
            ->Method("GetHit", &QueryHitCollection::GetHit)
            ->Method("HasOverflow", &QueryHitCollection::HasOverflow);
        behaviorContext->Class<ConvexCastParameters>("ConvexCastParameters")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Property("start", BehaviorValueProperty(&ConvexCastParameters::m_start))
            ->Property("scale", BehaviorValueProperty(&ConvexCastParameters::m_scale))
            ->Property("translation", BehaviorValueProperty(&ConvexCastParameters::m_translation))
            ->Property("filter", BehaviorValueProperty(&ConvexCastParameters::m_filter))
            ->Property("maxHitCount", BehaviorValueProperty(&ConvexCastParameters::m_maxHitCount));
        behaviorContext->Class<ConvexOverlapParameters>("ConvexOverlapParameters")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Constructor<>()
            ->Property("transform", BehaviorValueProperty(&ConvexOverlapParameters::m_transform))
            ->Property("scale", BehaviorValueProperty(&ConvexOverlapParameters::m_scale))
            ->Property("filter", BehaviorValueProperty(&ConvexOverlapParameters::m_filter))
            ->Property("maxHitCount", BehaviorValueProperty(&ConvexOverlapParameters::m_maxHitCount));
    }
} // namespace Box3D
