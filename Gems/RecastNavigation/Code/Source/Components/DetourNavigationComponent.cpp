/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/math.h>
#include <Components/DetourNavigationComponent.h>
#include <RecastNavigation/RecastHelpers.h>
#include <RecastNavigation/RecastNavigationMeshBus.h>

AZ_DECLARE_BUDGET(Navigation);

namespace RecastNavigation
{
    static bool HasRepresentableTileBounds(
        const dtNavMesh& navMesh,
        const float* center,
        const float* halfExtents)
    {
        const dtNavMeshParams& navMeshParams = *navMesh.getParams();
        if (!AZ::IsFiniteFloat(navMeshParams.orig[0])
            || !AZ::IsFiniteFloat(navMeshParams.orig[2])
            || !AZ::IsFiniteFloat(navMeshParams.tileWidth)
            || !AZ::IsFiniteFloat(navMeshParams.tileHeight)
            || navMeshParams.tileWidth <= 0.0f
            || navMeshParams.tileHeight <= 0.0f)
        {
            return false;
        }

        constexpr double MinTileCoordinate = static_cast<double>(AZStd::numeric_limits<int32_t>::lowest());
        constexpr double MaxTileCoordinate = static_cast<double>(AZStd::numeric_limits<int32_t>::max()) - 1.0;
        constexpr int HorizontalAxes[] = { 0, 2 };

        for (int axis : HorizontalAxes)
        {
            const float tileSize = axis == 0 ? navMeshParams.tileWidth : navMeshParams.tileHeight;
            const float minimumTile = AZStd::floor(((center[axis] - halfExtents[axis]) - navMeshParams.orig[axis]) / tileSize);
            const float maximumTile = AZStd::floor(((center[axis] + halfExtents[axis]) - navMeshParams.orig[axis]) / tileSize);

            // Detour casts these values to int and then traverses an inclusive range.
            if (!AZ::IsFiniteFloat(minimumTile)
                || !AZ::IsFiniteFloat(maximumTile)
                || static_cast<double>(minimumTile) < MinTileCoordinate
                || static_cast<double>(maximumTile) > MaxTileCoordinate)
            {
                return false;
            }
        }

        return true;
    }

    DetourNavigationComponent::DetourNavigationComponent(AZ::EntityId navQueryEntityId, float nearestDistance)
        : m_navQueryEntityId(navQueryEntityId), m_nearestDistance(nearestDistance)
    {
    }

    void DetourNavigationComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<DetourNavigationComponent, AZ::Component>()
                ->Field("Navigation Query Entity", &DetourNavigationComponent::m_navQueryEntityId)
                ->Field("Nearest Distance", &DetourNavigationComponent::m_nearestDistance)
                ->Version(1);
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<DetourNavigationRequestBus>("DetourNavigationRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "navigation")
                ->Attribute(AZ::Script::Attributes::Category, "Recast Navigation")
                ->Event("FindPathBetweenEntities", &DetourNavigationRequests::FindPathBetweenEntities)
                ->Event("FindPathBetweenPositions", &DetourNavigationRequests::FindPathBetweenPositions)
                ->Event("SetNavigationMeshEntity", &DetourNavigationRequests::SetNavigationMeshEntity)
                ->Event("GetNavigationMeshEntity", &DetourNavigationRequests::GetNavigationMeshEntity)
                ;

            behaviorContext->Class<DetourNavigationComponent>()->RequestBus("DetourNavigationRequestBus");
        }
    }

    AZStd::vector<AZ::Vector3> DetourNavigationComponent::FindPathBetweenEntities(AZ::EntityId fromEntity, AZ::EntityId toEntity)
    {
        if (fromEntity.IsValid() && toEntity.IsValid())
        {
            AZ::Vector3 start = AZ::Vector3::CreateZero(), end = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(start, fromEntity, &AZ::TransformBus::Events::GetWorldTranslation);
            AZ::TransformBus::EventResult(end, toEntity, &AZ::TransformBus::Events::GetWorldTranslation);

            return FindPathBetweenPositions(start, end);
        }

        return {};
    }

    AZStd::vector<AZ::Vector3> DetourNavigationComponent::FindPathBetweenPositions(const AZ::Vector3& fromWorldPosition, const AZ::Vector3& toWorldPosition)
    {
        AZ_PROFILE_SCOPE(Navigation, "Navigation: FindPathBetweenPositions");

        if (!fromWorldPosition.IsFinite()
            || !toWorldPosition.IsFinite()
            || !AZ::IsFiniteFloat(m_nearestDistance)
            || m_nearestDistance < 0.0f)
        {
            return {};
        }

        AZStd::shared_ptr<NavMeshQuery> navMeshQuery;
        RecastNavigationMeshRequestBus::EventResult(navMeshQuery, m_navQueryEntityId, &RecastNavigationMeshRequests::GetNavigationObject);
        if (!navMeshQuery)
        {
            return {};
        }

        NavMeshQuery::LockGuard lock(*navMeshQuery);
        dtNavMesh* navMesh = lock.GetNavMesh();
        dtNavMeshQuery* query = lock.GetNavQuery();
        if (!navMesh || !query)
        {
            return {};
        }

        RecastVector3 startRecast = RecastVector3::CreateFromVector3SwapYZ(fromWorldPosition);
        RecastVector3 endRecast = RecastVector3::CreateFromVector3SwapYZ(toWorldPosition);
        const float halfExtents[3] = { m_nearestDistance, m_nearestDistance, m_nearestDistance };

        if (!HasRepresentableTileBounds(*navMesh, startRecast.GetData(), halfExtents)
            || !HasRepresentableTileBounds(*navMesh, endRecast.GetData(), halfExtents))
        {
            return {};
        }

        dtPolyRef startPoly = 0, endPoly = 0;

        RecastVector3 nearestStartPoint, nearestEndPoint;

        const dtQueryFilter filter;

        // Find nearest points on the navigation mesh given the positions provided.
        // We are allowing some flexibility where looking for a point just a bit outside of the navigation mesh would still work.
        dtStatus result = query->findNearestPoly(startRecast.GetData(), halfExtents, &filter, &startPoly, nearestStartPoint.GetData());
        if (dtStatusFailed(result) || startPoly == 0)
        {
            return {};
        }

        result = query->findNearestPoly(endRecast.GetData(), halfExtents, &filter, &endPoly, nearestEndPoint.GetData());
        if (dtStatusFailed(result) || endPoly == 0)
        {
            return {};
        }

        // Some reasonable amount of waypoints along the path. Recast isn't made to calculate very long paths.
        constexpr int MaxPathLength = 100;

        AZStd::array<dtPolyRef, MaxPathLength> path;
        int pathLength = 0;

        // Find an approximate path first. In Recast, an approximate path is a collection of polygons, where a polygon covers an area.
        result = query->findPath(startPoly, endPoly, nearestStartPoint.GetData(), nearestEndPoint.GetData(),
            &filter, path.data(), &pathLength, MaxPathLength);
        if (dtStatusFailed(result))
        {
            return {};
        }

        AZStd::array<RecastVector3, MaxPathLength> detailedPath;
        AZStd::array<AZ::u8, MaxPathLength> detailedPathFlags;
        AZStd::array<dtPolyRef, MaxPathLength> detailedPolyPathRefs;
        int detailedPathCount = 0;

        // Then the detailed path. This gives us actual specific waypoints along the path over the polygons found earlier.
        result = query->findStraightPath(startRecast.GetData(), endRecast.GetData(), path.data(), pathLength,
            detailedPath[0].GetData(), detailedPathFlags.data(), detailedPolyPathRefs.data(),
            &detailedPathCount, MaxPathLength, DT_STRAIGHTPATH_ALL_CROSSINGS);
        if (dtStatusFailed(result))
        {
            return {};
        }

        AZStd::vector<AZ::Vector3> pathPoints;
        pathPoints.reserve(detailedPathCount);
        // Note: Recast uses +Y, O3DE used +Z as up vectors.
        for (int i = 0; i < detailedPathCount; ++i)
        {
            pathPoints.push_back(detailedPath[i].AsVector3WithZup());
        }

        return pathPoints;
    }

    void DetourNavigationComponent::SetNavigationMeshEntity(AZ::EntityId navMeshEntity)
    {
        m_navQueryEntityId = navMeshEntity;
    }

    AZ::EntityId DetourNavigationComponent::GetNavigationMeshEntity() const
    {
        return m_navQueryEntityId;
    }

    void DetourNavigationComponent::Activate()
    {
        if (!m_navQueryEntityId.IsValid())
        {
            // Default to looking for the navigation mesh component on the same entity if one is not specified.
            m_navQueryEntityId = GetEntityId();
        }

        DetourNavigationRequestBus::Handler::BusConnect(GetEntityId());
    }

    void DetourNavigationComponent::Deactivate()
    {
        DetourNavigationRequestBus::Handler::BusDisconnect();
    }
} // namespace RecastNavigation
