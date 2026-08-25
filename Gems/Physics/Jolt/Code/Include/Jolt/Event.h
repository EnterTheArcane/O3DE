/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Rollback.h>

#include <Jolt/Handle.h>
#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    class EventBatchPool;
    struct EventBatchStorage;

    JOLT_API void ReflectEvents(AZ::ReflectContext* context);

    enum class ActivationState : AZ::u8
    {
        None = 0,
        Active,
        Inactive,
    };

    enum class EventPhase : AZ::u8
    {
        None = 0,
        Begin,
        End,
        Persist,
    };

    enum class ContactDecision : AZ::u8
    {
        None = 0,
        AcceptAllForPair,
        Accept,
        Reject,
        RejectAllForPair,
    };

    struct ContactPoint final
    {
        AZ_TYPE_INFO(ContactPoint, ContactPointTypeId);

        WorldPosition m_positionOnFirstBody;
        WorldPosition m_positionOnSecondBody;
    };

    struct ContactEvent final
    {
        AZ_TYPE_INFO(ContactEvent, ContactEventTypeId);

        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        ShapeHandle m_firstShapeHandle;
        ShapeHandle m_secondShapeHandle;
        MaterialHandle m_firstMaterialHandle;
        MaterialHandle m_secondMaterialHandle;
        SubShapeId m_firstSubShapeId;
        SubShapeId m_secondSubShapeId;

        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        float m_penetrationDepth = 0.0f;
        AZ::u32 m_firstPoint = 0;
        AZ::u32 m_pointCount = 0;
        EventPhase m_phase = EventPhase::None;
    };

    struct ContactSettings final
    {
        AZ_TYPE_INFO(ContactSettings, ContactSettingsTypeId);

        AZ::Vector3 m_relativeAngularSurfaceVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_relativeLinearSurfaceVelocity = AZ::Vector3::CreateZero();
        float m_combinedFriction = 0.0f;
        float m_combinedRestitution = 0.0f;
        float m_firstBodyInverseInertiaScale = 1.0f;
        float m_firstBodyInverseMassScale = 1.0f;
        float m_secondBodyInverseInertiaScale = 1.0f;
        float m_secondBodyInverseMassScale = 1.0f;
        bool m_isSensor = false;
    };

    struct ContactValidation final
    {
        AZ_TYPE_INFO(ContactValidation, ContactValidationTypeId);

        WorldPosition m_positionOnFirstBody;
        WorldPosition m_positionOnSecondBody;
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();
        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        ShapeHandle m_firstShapeHandle;
        ShapeHandle m_secondShapeHandle;
        SubShapeId m_firstSubShapeId;
        SubShapeId m_secondSubShapeId;
        float m_penetrationDepth = 0.0f;
    };

    struct ContactResponseConfiguration final
    {
        AZ_TYPE_INFO(ContactResponseConfiguration, ContactResponseConfigurationTypeId);

        float m_combinedFriction = 0.0f;
        float m_combinedRestitution = 0.0f;
        float m_minimumVelocityForRestitution = 1.0f;
        AZ::u32 m_iterationCount = 10;
    };

    struct ContactResponseEstimate final
    {
        AZ_TYPE_INFO(ContactResponseEstimate, ContactResponseEstimateTypeId);

        AZ::Vector3 m_firstLinearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_firstAngularVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondLinearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondAngularVelocity = AZ::Vector3::CreateZero();

        WorldPosition m_frictionPoint;
        AZ::Vector3 m_firstTangent = AZ::Vector3::CreateZero();
        AZ::Vector3 m_secondTangent = AZ::Vector3::CreateZero();

        float m_firstFrictionImpulse = 0.0f;
        float m_secondFrictionImpulse = 0.0f;
        float m_angularFrictionImpulse = 0.0f;
        AZ::u32 m_impulseCount = 0;
        AZ::u32 m_requiredImpulseCount = 0;

        [[nodiscard]]
        constexpr bool IsComplete() const
        {
            return m_impulseCount == m_requiredImpulseCount;
        }
    };

    class IContactManifold
    {
    public:
        virtual ~IContactManifold() = default;

        //! This view is valid only for the duration of its callback.

        [[nodiscard]]
        virtual BodyHandle GetFirstBody() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetSecondBody() const = 0;

        [[nodiscard]]
        virtual ShapeHandle GetFirstShape() const = 0;

        [[nodiscard]]
        virtual ShapeHandle GetSecondShape() const = 0;

        [[nodiscard]]
        virtual SubShapeId GetFirstSubShape() const = 0;

        [[nodiscard]]
        virtual SubShapeId GetSecondSubShape() const = 0;

        [[nodiscard]]
        virtual AZ::Vector3 GetNormal() const = 0;

        [[nodiscard]]
        virtual float GetPenetrationDepth() const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetPointCount() const = 0;

        [[nodiscard]]
        virtual bool GetPoint(
            AZ::u32 pointIndex,
            ContactPoint& point) const = 0;

        [[nodiscard]]
        virtual bool EstimateResponse(
            const ContactResponseConfiguration& configuration,
            ContactResponseEstimate& estimate,
            AZStd::span<float> impulses) const = 0;
    };

    class IContactCallbacks
        : public IRollbackParticipant
    {
    public:
        virtual ~IContactCallbacks() = default;

        //! Callbacks run under simulation locks and may run concurrently. They must not call runtime capabilities.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual ContactDecision OnContactValidate(const ContactValidation& contact) = 0;

        virtual void OnContactAdded(
            const IContactManifold& manifold,
            ContactSettings& settings) = 0;

        virtual void OnContactPersisted(
            const IContactManifold& manifold,
            ContactSettings& settings) = 0;

        virtual void OnContactRemoved(const ContactEvent& contact) = 0;
    };

    struct ActivationEvent final
    {
        AZ_TYPE_INFO(ActivationEvent, ActivationEventTypeId);

        BodyHandle m_bodyHandle;
        ActivationState m_state = ActivationState::None;
    };

    struct BodyMoveEvent final
    {
        AZ_TYPE_INFO(BodyMoveEvent, BodyMoveEventTypeId);

        WorldTransform m_transform;
        BodyHandle m_bodyHandle;
        AZ::EntityId m_entityId = AZ::EntityId();
    };

    struct VirtualCharacterMoveEvent final
    {
        AZ_TYPE_INFO(VirtualCharacterMoveEvent, VirtualCharacterMoveEventTypeId);

        WorldTransform m_transform;
        VirtualCharacterHandle m_characterHandle;
        AZ::EntityId m_entityId = AZ::EntityId();
    };

    //! Callback-only view; the points remain valid only for the current notification.
    class ContactPointView final
    {
    public:
        AZ_TYPE_INFO(ContactPointView, ContactPointViewTypeId);

        [[nodiscard]]
        AZ::u32 GetPointCount() const
        {
            return static_cast<AZ::u32>(m_points.size());
        }

        [[nodiscard]]
        ContactPoint GetPoint(
            const AZ::u32 pointIndex) const
        {
            if (pointIndex < m_points.size())
            {
                return m_points[pointIndex];
            }

            return {};
        }

    private:
        friend class SystemComponent;

        explicit ContactPointView(
            const AZStd::span<const ContactPoint> points)
            : m_points(points)
        {
        }

        AZStd::span<const ContactPoint> m_points;
    };

    //! Immutable events from one published world generation. Copies share provider-owned storage.
    class JOLT_API EventBatch final
    {
    public:
        EventBatch() = default;
        EventBatch(const EventBatch& other);
        EventBatch(EventBatch&& other) noexcept;
        ~EventBatch();

        EventBatch& operator=(const EventBatch& other);

        EventBatch& operator=(EventBatch&& other) noexcept;

        [[nodiscard]]
        explicit operator bool() const;

        [[nodiscard]]
        AZ::u64 GetSequence() const;

        [[nodiscard]]
        AZStd::span<const ActivationEvent> GetActivations() const &;

        AZStd::span<const ActivationEvent> GetActivations() const && = delete;

        [[nodiscard]]
        AZStd::span<const ContactEvent> GetContacts() const &;

        AZStd::span<const ContactEvent> GetContacts() const && = delete;

        [[nodiscard]]
        AZStd::span<const BodyMoveEvent> GetBodyMoves() const &;

        AZStd::span<const BodyMoveEvent> GetBodyMoves() const && = delete;

        [[nodiscard]]
        AZStd::span<const VirtualCharacterMoveEvent> GetVirtualCharacterMoves() const &;

        AZStd::span<const VirtualCharacterMoveEvent> GetVirtualCharacterMoves() const && = delete;

        [[nodiscard]]
        AZStd::span<const ContactPoint> GetContactPoints(const ContactEvent& contact) const &;

        AZStd::span<const ContactPoint> GetContactPoints(const ContactEvent& contact) const && = delete;

    private:
        friend class EventBatchPool;
        friend class World;

        explicit EventBatch(EventBatchStorage* storage);

        EventBatchStorage* m_storage = nullptr;
    };

    struct WorldEventBatch final
    {
        EventBatch m_events;
        WorldHandle m_worldHandle;
    };

    class IWorldNotifications
        : public AZ::EBusTraits
    {
    public:
        using BusIdType = WorldHandle;

        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

        virtual void OnBodyActivation(
            [[maybe_unused]] const ActivationEvent& event)
        {
        }

        virtual void OnBodyMoved(
            [[maybe_unused]] const BodyMoveEvent& event)
        {
        }

        virtual void OnContact(
            [[maybe_unused]] const ContactEvent& event,
            [[maybe_unused]] const ContactPointView& points)
        {
        }

        virtual void OnVirtualCharacterMoved(
            [[maybe_unused]] const VirtualCharacterMoveEvent& event)
        {
        }
    };

    using WorldNotificationBus = AZ::EBus<IWorldNotifications>;
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::ActivationState, "{4A1DD434-A763-4DCC-B7C7-3BD0AED5E62F}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::ContactDecision, "{A4879ED9-5074-44BD-AC6C-C78A0A580381}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::EventPhase, "{96AEED99-9D4D-4811-989C-8EAE017AD61F}");
