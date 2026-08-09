/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/CharacterConfiguration.h>
#include <Box3D/Events.h>
#include <Box3D/Handle.h>
#include <Box3D/Joints.h>
#include <Box3D/Material.h>
#include <Box3D/Queries.h>
#include <Box3D/RigidBodyConfiguration.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemConfiguration.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/string/string_view.h>

namespace Box3D
{
    using SimulationTick = AZ::u64;

    struct MassProperties final
    {
        AZ_TYPE_INFO(MassProperties, MassPropertiesTypeId);

        AZ::Matrix3x3 m_inertia = AZ::Matrix3x3::CreateIdentity();
        AZ::Vector3 m_center = AZ::Vector3::CreateZero();
        float m_mass = 1.0f;
    };

    struct BodyState final
    {
        AZ_TYPE_INFO(BodyState, BodyStateTypeId);

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
        AZ::EntityId m_entityId{};
        AZ::Name m_name;
        BodyType m_bodyType = BodyType::Static;
        bool m_isAwake = false;
        bool m_isEnabled = false;
        bool m_allowFastRotation = false;
    };

    enum class ShapeType : AZ::u8
    {
        Sphere,
        Capsule,
        Hull,
        Mesh,
        Heightfield,
        Compound,
    };

    struct ShapeState final
    {
        AZ_TYPE_INFO(ShapeState, ShapeStateTypeId);

        BodyHandle m_bodyHandle;
        CollisionFilter m_collisionFilter;
        ShapeType m_type = ShapeType::Sphere;
        float m_density = 0.0f;
        float m_friction = 0.0f;
        float m_restitution = 0.0f;
        bool m_isSensor = false;
        bool m_enableSensorEvents = false;
        bool m_enableContactEvents = false;
        bool m_enableHitEvents = false;
        bool m_enablePreSolveEvents = false;
    };

    //! Runtime-mutable body policy grouped to avoid repeated interface lookups and virtual calls.
    struct BodyProperties final
    {
        AZ_TYPE_INFO(BodyProperties, BodyPropertiesTypeId);

        MotionLocks m_motionLocks;
        BodyType m_bodyType = BodyType::Dynamic;
        float m_linearDamping = 0.0f;
        float m_angularDamping = 0.0f;
        float m_gravityScale = 1.0f;
        float m_sleepThreshold = 0.05f;
        bool m_isBullet = false;
        bool m_enableSleep = true;
        bool m_enableContactRecycling = true;
    };

    struct StepEvents final
    {
        AZStd::span<const BodyMoveEvent> m_bodyMoves;
        AZStd::span<const SensorEvent> m_sensorEvents;
        AZStd::span<const ContactEvent> m_contactEvents;
        AZStd::span<const ContactPoint> m_contactPoints;
        AZStd::span<const ContactHitEvent> m_contactHits;
        AZStd::span<const JointThresholdEvent> m_jointThresholds;
    };

    using CollisionFilterCallback = bool (*)(ShapeHandle shapeA, ShapeHandle shapeB, void* userData);
    using PreSolveCallback =
        bool (*)(ShapeHandle shapeA, ShapeHandle shapeB, const AZ::Vector3& position, const AZ::Vector3& normal, void* userData);

    //! Box3D-owned simulation API. No native identifiers or shared physics-provider singletons cross this boundary.
    class ISystem
    {
    public:
        AZ_RTTI(ISystem, ISystemTypeId);

        virtual ~ISystem() = default;

        virtual void UpdateConfiguration(const SystemConfiguration& configuration) = 0;
        [[nodiscard]] virtual const SystemConfiguration& GetConfiguration() const = 0;

        [[nodiscard]] virtual WorldHandle CreateWorld(const WorldConfiguration& configuration) = 0;
        virtual bool DestroyWorld(WorldHandle worldHandle) = 0;
        [[nodiscard]] virtual WorldHandle GetDefaultWorldHandle() const = 0;
        [[nodiscard]] virtual const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const = 0;
        [[nodiscard]] virtual WorldHandle FindWorld(AZ::Name name) const = 0;
        [[nodiscard]] virtual bool GetWorldConfiguration(WorldHandle worldHandle, WorldConfiguration& configuration) const = 0;
        [[nodiscard]] virtual AZ::Aabb GetWorldAabb(WorldHandle worldHandle) const = 0;
        [[nodiscard]] virtual bool IsValid(WorldHandle worldHandle) const = 0;
        virtual bool SetWorldEnabled(WorldHandle worldHandle, bool enabled) = 0;
        [[nodiscard]] virtual bool IsWorldEnabled(WorldHandle worldHandle) const = 0;
        virtual bool SetWorldGravity(WorldHandle worldHandle, const AZ::Vector3& gravity) = 0;
        [[nodiscard]] virtual bool GetWorldGravity(WorldHandle worldHandle, AZ::Vector3& gravity) const = 0;
        virtual bool StepWorld(WorldHandle worldHandle, float fixedTimeStep) = 0;
        virtual void StepAutoSimulatedWorlds(float deltaTime) = 0;
        [[nodiscard]] virtual SimulationTick GetLastCompletedTick(WorldHandle worldHandle) const = 0;
        [[nodiscard]] virtual AZ::u64 GetStateDigest(WorldHandle worldHandle) const = 0;
        [[nodiscard]] virtual AZStd::string_view GetCompatibilityFingerprint() const = 0;

        [[nodiscard]] virtual MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) = 0;
        virtual bool UpdateMaterial(MaterialHandle materialHandle, const MaterialConfiguration& configuration) = 0;
        [[nodiscard]] virtual bool GetMaterial(MaterialHandle materialHandle, MaterialConfiguration& configuration) const = 0;
        virtual bool DestroyMaterial(MaterialHandle materialHandle) = 0;

        [[nodiscard]] virtual BodyHandle CreateBody(WorldHandle worldHandle, const RigidBodyConfiguration& configuration) = 0;
        virtual bool DestroyBody(WorldHandle worldHandle, BodyHandle bodyHandle) = 0;
        [[nodiscard]] virtual bool GetBodyState(WorldHandle worldHandle, BodyHandle bodyHandle, BodyState& state) const = 0;
        [[nodiscard]] virtual AZ::Name GetBodyName(WorldHandle worldHandle, BodyHandle bodyHandle) const = 0;
        virtual bool SetBodyName(WorldHandle worldHandle, BodyHandle bodyHandle, AZ::Name name) = 0;
        [[nodiscard]] virtual bool GetBodyProperties(WorldHandle worldHandle, BodyHandle bodyHandle, BodyProperties& properties) const = 0;
        virtual bool SetBodyProperties(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyProperties& properties) = 0;
        virtual bool SetBodyAwake(WorldHandle worldHandle, BodyHandle bodyHandle, bool awake) = 0;
        virtual bool SetBodyEnabled(WorldHandle worldHandle, BodyHandle bodyHandle, bool enabled) = 0;
        virtual bool SetBodyHitEventsEnabled(WorldHandle worldHandle, BodyHandle bodyHandle, bool enabled) = 0;
        virtual bool SetBodyTransform(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Transform& transform) = 0;
        [[nodiscard]] virtual bool GetBodyLocalPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldPoint, AZ::Vector3& localPoint) const = 0;
        [[nodiscard]] virtual bool GetBodyWorldPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localPoint, AZ::Vector3& worldPoint) const = 0;
        [[nodiscard]] virtual bool GetBodyLocalVector(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldVector, AZ::Vector3& localVector) const = 0;
        [[nodiscard]] virtual bool GetBodyWorldVector(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localVector, AZ::Vector3& worldVector) const = 0;
        virtual bool SetLinearVelocity(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& velocity) = 0;
        virtual bool SetAngularVelocity(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& velocity) = 0;
        [[nodiscard]] virtual AZ::Vector3 GetLinearVelocityAtLocalPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localPoint) const = 0;
        [[nodiscard]] virtual AZ::Vector3 GetLinearVelocityAtWorldPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldPoint) const = 0;
        virtual bool SetKinematicTarget(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Transform& transform, float fixedTimeStep, bool wake = true) = 0;
        virtual bool ApplyLinearImpulse(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& impulse, bool wake = true) = 0;
        virtual bool ApplyLinearImpulseAtWorldPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const AZ::Vector3& worldPoint,
            bool wake = true) = 0;
        virtual bool ApplyAngularImpulse(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& impulse, bool wake = true) = 0;
        virtual bool ApplyForce(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& force, bool wake) = 0;
        virtual bool ApplyForceAtWorldPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& force, const AZ::Vector3& worldPoint, bool wake) = 0;
        virtual bool ApplyTorque(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& torque, bool wake) = 0;
        [[nodiscard]] virtual bool GetMassProperties(WorldHandle worldHandle, BodyHandle bodyHandle, MassProperties& properties) const = 0;
        virtual bool SetMassProperties(WorldHandle worldHandle, BodyHandle bodyHandle, const MassProperties& properties) = 0;
        virtual bool RecomputeMassFromShapes(WorldHandle worldHandle, BodyHandle bodyHandle) = 0;
        [[nodiscard]] virtual AZ::Matrix3x3 GetWorldInverseInertia(WorldHandle worldHandle, BodyHandle bodyHandle) const = 0;
        [[nodiscard]] virtual AZ::Vector3 GetWorldCenterOfMass(WorldHandle worldHandle, BodyHandle bodyHandle) const = 0;
        [[nodiscard]] virtual bool GetBodyClosestPoint(
            WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const = 0;
        [[nodiscard]] virtual AZ::Aabb GetBodyAabb(WorldHandle worldHandle, BodyHandle bodyHandle) const = 0;
        [[nodiscard]] virtual BufferResult GetBodyShapes(
            WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<ShapeHandle> shapeHandles) const = 0;
        [[nodiscard]] virtual BufferResult GetBodyJoints(
            WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<JointHandle> jointHandles) const = 0;
        [[nodiscard]] virtual ContactSnapshotResult GetBodyContacts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const = 0;
        [[nodiscard]] virtual BufferResult GetBodySensorOverlaps(
            WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<SensorOverlap> overlaps) const = 0;
        [[nodiscard]] virtual bool RaycastBody(
            WorldHandle worldHandle, BodyHandle bodyHandle, const BodyRaycastRequest& request, QueryHit& hit) const = 0;
        [[nodiscard]] virtual bool ShapeCastBody(
            WorldHandle worldHandle, BodyHandle bodyHandle, const BodyShapeCastRequest& request, QueryHit& hit) const = 0;
        [[nodiscard]] virtual bool OverlapBody(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyOverlapRequest& request) const = 0;

        [[nodiscard]] virtual ShapeHandle CreateShape(
            WorldHandle worldHandle, BodyHandle bodyHandle, const ShapeConfiguration& configuration) = 0;
        //! The cooked geometry owns its baked transform and materials. Instance properties must leave those fields empty and identity.
        [[nodiscard]] virtual ShapeHandle CreateShapeFromCooked(
            WorldHandle worldHandle, BodyHandle bodyHandle, CookedShapeHandle cookedShapeHandle, const ShapeProperties& properties) = 0;
        virtual bool UpdateShape(WorldHandle worldHandle, ShapeHandle shapeHandle, const ShapeConfiguration& configuration) = 0;
        virtual bool DestroyShape(WorldHandle worldHandle, ShapeHandle shapeHandle, bool updateBodyMass = true) = 0;
        virtual bool SetShapeCollisionFilter(WorldHandle worldHandle, ShapeHandle shapeHandle, const CollisionFilter& collisionFilter) = 0;
        virtual bool SetShapeMaterials(WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<const MaterialHandle> materials) = 0;
        [[nodiscard]] virtual AZ::Aabb GetShapeAabb(WorldHandle worldHandle, ShapeHandle shapeHandle) const = 0;
        [[nodiscard]] virtual bool GetShapeState(WorldHandle worldHandle, ShapeHandle shapeHandle, ShapeState& state) const = 0;
        [[nodiscard]] virtual BufferResult GetShapeMaterials(
            WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<MaterialHandle> materialHandles) const = 0;
        virtual bool SetShapeDensity(WorldHandle worldHandle, ShapeHandle shapeHandle, float density, bool updateBodyMass = true) = 0;
        virtual bool SetShapeFriction(WorldHandle worldHandle, ShapeHandle shapeHandle, float friction) = 0;
        virtual bool SetShapeRestitution(WorldHandle worldHandle, ShapeHandle shapeHandle, float restitution) = 0;
        virtual bool SetShapeEventSubscriptions(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            bool sensorEvents,
            bool contactEvents,
            bool hitEvents,
            bool preSolveEvents) = 0;
        [[nodiscard]] virtual bool GetShapeMassProperties(
            WorldHandle worldHandle, ShapeHandle shapeHandle, MassProperties& properties) const = 0;
        [[nodiscard]] virtual bool GetShapeClosestPoint(
            WorldHandle worldHandle, ShapeHandle shapeHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const = 0;
        [[nodiscard]] virtual bool RaycastShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            QueryHit& hit) const = 0;
        [[nodiscard]] virtual ContactSnapshotResult GetShapeContacts(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const = 0;
        [[nodiscard]] virtual BufferResult GetShapeSensorOverlaps(
            WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<SensorOverlap> overlaps) const = 0;

        [[nodiscard]] virtual JointHandle CreateJoint(WorldHandle worldHandle, const JointConfiguration& configuration) = 0;
        virtual bool UpdateJoint(WorldHandle worldHandle, JointHandle jointHandle, const JointConfiguration& configuration) = 0;
        virtual bool DestroyJoint(WorldHandle worldHandle, JointHandle jointHandle, bool wakeAttachedBodies = true) = 0;
        [[nodiscard]] virtual bool GetJointConfiguration(
            WorldHandle worldHandle, JointHandle jointHandle, JointConfiguration& configuration) const = 0;
        [[nodiscard]] virtual bool GetJointMeasurements(
            WorldHandle worldHandle, JointHandle jointHandle, JointMeasurements& measurements) const = 0;
        virtual bool WakeJointBodies(WorldHandle worldHandle, JointHandle jointHandle) = 0;

        [[nodiscard]] virtual CharacterHandle CreateCharacter(WorldHandle worldHandle, const CharacterConfiguration& configuration) = 0;
        virtual bool UpdateCharacter(
            WorldHandle worldHandle, CharacterHandle characterHandle, const CharacterConfiguration& configuration) = 0;
        virtual bool DestroyCharacter(WorldHandle worldHandle, CharacterHandle characterHandle) = 0;
        virtual bool MoveCharacter(
            WorldHandle worldHandle, CharacterHandle characterHandle, const AZ::Vector3& velocity, float fixedTimeStep) = 0;
        [[nodiscard]] virtual bool GetCharacterState(
            WorldHandle worldHandle, CharacterHandle characterHandle, CharacterState& state) const = 0;
        [[nodiscard]] virtual bool GetCharacterConfiguration(
            WorldHandle worldHandle, CharacterHandle characterHandle, CharacterConfiguration& configuration) const = 0;

        [[nodiscard]] virtual bool RaycastClosest(WorldHandle worldHandle, const RaycastRequest& request, QueryHit& hit) const = 0;
        [[nodiscard]] virtual BufferResult RaycastClosestBatch(
            WorldHandle worldHandle, AZStd::span<const RaycastRequest> requests, AZStd::span<ClosestQueryResult> results) const = 0;
        [[nodiscard]] virtual QueryResult Raycast(
            WorldHandle worldHandle, const RaycastRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult ShapeCast(
            WorldHandle worldHandle, const ShapeCastRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult Overlap(
            WorldHandle worldHandle, const OverlapRequest& request, AZStd::span<OverlapHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult Overlap(
            WorldHandle worldHandle, const OverlapRequest& request, AZStd::span<QueryHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult OverlapAabb(
            WorldHandle worldHandle, const AabbOverlapRequest& request, AZStd::span<OverlapHit> hits) const = 0;
        [[nodiscard]] virtual QueryResult OverlapAabb(
            WorldHandle worldHandle, const AabbOverlapRequest& request, AZStd::span<QueryHit> hits) const = 0;

        //! Views remain valid until the next step or mutation of the world.
        [[nodiscard]] virtual StepEvents GetStepEvents(WorldHandle worldHandle) const = 0;
        virtual bool SetContactCallbacks(
            WorldHandle worldHandle,
            CollisionFilterCallback collisionFilterCallback,
            PreSolveCallback preSolveCallback,
            void* userData) = 0;
    };

} // namespace Box3D
