/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Queries.h>
#include <Box3D/SystemConfiguration.h>
#include <Box3D/TypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <cstddef>

namespace Box3D
{
    //! Script-facing input adapter for the allocation-free C++ batch query API.
    struct RaycastRequestCollection final
    {
        AZ_TYPE_INFO(RaycastRequestCollection, RaycastRequestCollectionTypeId);

        void AddRequest(const RaycastRequest& request)
        {
            m_requests.push_back(request);
        }

        void Clear()
        {
            m_requests.clear();
        }

        [[nodiscard]] size_t GetRequestCount() const noexcept
        {
            return m_requests.size();
        }

        [[nodiscard]] RaycastRequest GetRequest(size_t index) const
        {
            return index < m_requests.size() ? m_requests[index] : RaycastRequest{};
        }

        [[nodiscard]] AZStd::span<const RaycastRequest> GetRequests() const noexcept
        {
            return m_requests;
        }

    private:
        AZStd::vector<RaycastRequest> m_requests;
    };

    struct QueryHitCollection final
    {
        AZ_TYPE_INFO(QueryHitCollection, "{222FCAC3-2532-4FE3-B563-ECD7FB8EDE36}");

        AZStd::vector<QueryHit> m_hits;
        size_t m_requiredHitCount = 0;

        [[nodiscard]] size_t GetHitCount() const noexcept
        {
            return m_hits.size();
        }

        [[nodiscard]] QueryHit GetHit(size_t index) const
        {
            return index < m_hits.size() ? m_hits[index] : QueryHit{};
        }

        [[nodiscard]] bool HasOverflow() const noexcept
        {
            return m_requiredHitCount > m_hits.size();
        }
    };

    //! Script-facing adapter for the allocation-free C++ batch query API.
    struct ClosestQueryResultCollection final
    {
        AZ_TYPE_INFO(ClosestQueryResultCollection, ClosestQueryResultCollectionTypeId);

        AZStd::vector<ClosestQueryResult> m_results;
        size_t m_requiredResultCount = 0;

        [[nodiscard]] size_t GetResultCount() const noexcept
        {
            return m_results.size();
        }

        [[nodiscard]] ClosestQueryResult GetResult(size_t index) const
        {
            return index < m_results.size() ? m_results[index] : ClosestQueryResult{};
        }

        [[nodiscard]] bool HasOverflow() const noexcept
        {
            return m_requiredResultCount > m_results.size();
        }
    };

    struct ConvexCastParameters final
    {
        AZ_TYPE_INFO(ConvexCastParameters, "{BA9B9A55-C9BD-46CA-9B9C-A4879DC62BE0}");

        AZ::Transform m_start = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        QueryFilter m_filter;
        AZ::u32 m_maxHitCount = 64;
    };

    struct ConvexOverlapParameters final
    {
        AZ_TYPE_INFO(ConvexOverlapParameters, "{6F2A22A5-1522-4663-93B0-55980053263D}");

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        QueryFilter m_filter;
        AZ::u32 m_maxHitCount = 64;
    };

    class WorldRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        [[nodiscard]] virtual WorldHandle GetDefaultWorldHandle() const = 0;
        [[nodiscard]] virtual WorldHandle CreateWorld(const WorldConfiguration& configuration) = 0;
        virtual bool DestroyWorld(WorldHandle worldHandle) = 0;
        virtual bool StepWorld(WorldHandle worldHandle, float fixedTimeStep) = 0;
        [[nodiscard]] virtual ClosestQueryResult RaycastClosest(WorldHandle worldHandle, const RaycastRequest& request) const = 0;
        [[nodiscard]] virtual ClosestQueryResultCollection RaycastClosestBatch(
            WorldHandle worldHandle, const RaycastRequestCollection& requests) const = 0;
        [[nodiscard]] virtual QueryHitCollection Raycast(
            WorldHandle worldHandle, const RaycastRequest& request, AZ::u32 maxHitCount) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapAabb(
            WorldHandle worldHandle, const AabbOverlapRequest& request, AZ::u32 maxHitCount) const = 0;
        [[nodiscard]] virtual size_t GetContactPointCount(WorldHandle worldHandle) const = 0;
        [[nodiscard]] virtual ContactPoint GetContactPointAt(WorldHandle worldHandle, size_t index) const = 0;

        [[nodiscard]] virtual QueryHitCollection ShapeCastSphere(
            WorldHandle worldHandle, const SphereShapeConfiguration& geometry, const ConvexCastParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapSphere(
            WorldHandle worldHandle, const SphereShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection ShapeCastCapsule(
            WorldHandle worldHandle, const CapsuleShapeConfiguration& geometry, const ConvexCastParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapCapsule(
            WorldHandle worldHandle, const CapsuleShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection ShapeCastBox(
            WorldHandle worldHandle, const BoxShapeConfiguration& geometry, const ConvexCastParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapBox(
            WorldHandle worldHandle, const BoxShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection ShapeCastCylinder(
            WorldHandle worldHandle, const CylinderShapeConfiguration& geometry, const ConvexCastParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapCylinder(
            WorldHandle worldHandle, const CylinderShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection ShapeCastConvexHull(
            WorldHandle worldHandle, const ConvexHullShapeConfiguration& geometry, const ConvexCastParameters& parameters) const = 0;
        [[nodiscard]] virtual QueryHitCollection OverlapConvexHull(
            WorldHandle worldHandle, const ConvexHullShapeConfiguration& geometry, const ConvexOverlapParameters& parameters) const = 0;
    };

    using WorldRequestBus = AZ::EBus<WorldRequests>;

    class WorldNotifications
        : public AZ::EBusTraits
    {
    public:
        using BusIdType = WorldHandle;

        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;

        virtual void OnBodyMoved([[maybe_unused]] const BodyMoveEvent& event)
        {
        }
        virtual void OnSensor([[maybe_unused]] const SensorEvent& event)
        {
        }
        virtual void OnContact([[maybe_unused]] const ContactEvent& event)
        {
        }
        virtual void OnContactHit([[maybe_unused]] const ContactHitEvent& event)
        {
        }
        virtual void OnJointThresholdExceeded([[maybe_unused]] const JointThresholdEvent& event)
        {
        }
    };

    using WorldNotificationBus = AZ::EBus<WorldNotifications>;
} // namespace Box3D
