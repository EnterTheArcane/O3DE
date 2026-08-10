/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Effects.h>
#include <Box3D/FloatEnvironment.h>
#include <Box3D/Internal/HandleEncoding.h>
#include <Box3D/NativeApi.h>
#include <Box3D/System.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Box3D
{
    class System;

    class World final
        : public IWorldQueries
        , private IContactCallbacks
    {
    public:
        AZ_CLASS_ALLOCATOR(World, AZ::SystemAllocator);

        World(
            System& system,
            AZ::u32 worldIndex,
            WorldHandle worldHandle,
            const WorldConfiguration& configuration,
            const SystemConfiguration& systemConfiguration,
            Internal::GenerationSource& bodyGenerations,
            Internal::GenerationSource& shapeGenerations,
            Internal::GenerationSource& jointGenerations,
            Internal::GenerationSource& characterGenerations);

        ~World();

        AZ_DISABLE_COPY_MOVE(World);

        [[nodiscard]]
        bool IsValid() const;

        explicit operator bool() const;

        [[nodiscard]]
        WorldHandle GetHandle() const;

        [[nodiscard]]
        const AZ::Name& GetName() const;

        [[nodiscard]]
        WorldConfiguration GetConfiguration() const;

        [[nodiscard]]
        AZ::Aabb GetAabb() const;

        void Reconfigure(const SystemConfiguration& systemConfiguration);

        bool SetEnabled(bool enabled);

        [[nodiscard]]
        bool IsEnabled() const;

        bool SetGravity(const AZ::Vector3& gravity);

        [[nodiscard]]
        bool GetGravity(AZ::Vector3& gravity) const;

        bool Step(
            float fixedTimeStep,
            AZ::u32 subStepCount);

        void StepAutomatically(
            float deltaTime,
            AZ::u32 subStepCount);

        [[nodiscard]]
        SimulationTick GetLastCompletedTick() const;

        [[nodiscard]]
        AZ::u64 GetStateDigest() const;

        [[nodiscard]]
        BodyHandle CreateBody(const RigidBodyConfiguration& configuration);

        bool DestroyBody(BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyState(
            BodyHandle bodyHandle,
            BodyState& state) const;

        [[nodiscard]]
        AZ::Name GetBodyName(BodyHandle bodyHandle) const;

        bool SetBodyName(
            BodyHandle bodyHandle,
            AZ::Name name);

        [[nodiscard]]
        bool GetBodyProperties(
            BodyHandle bodyHandle,
            BodyProperties& properties) const;

        bool SetBodyProperties(
            BodyHandle bodyHandle,
            const BodyProperties& properties);

        bool SetBodyAwake(
            BodyHandle bodyHandle,
            bool awake);

        bool SetBodyEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        bool SetBodyHitEventsEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        bool SetBodyTransform(
            BodyHandle bodyHandle,
            const AZ::Transform& transform);

        [[nodiscard]]
        bool GetBodyLocalPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& worldPoint,
            AZ::Vector3& localPoint) const;

        [[nodiscard]]
        bool GetBodyWorldPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& localPoint,
            AZ::Vector3& worldPoint) const;

        [[nodiscard]]
        bool GetBodyLocalVector(
            BodyHandle bodyHandle,
            const AZ::Vector3& worldVector,
            AZ::Vector3& localVector) const;

        [[nodiscard]]
        bool GetBodyWorldVector(
            BodyHandle bodyHandle,
            const AZ::Vector3& localVector,
            AZ::Vector3& worldVector) const;

        bool SetLinearVelocity(
            BodyHandle bodyHandle,
            const AZ::Vector3& velocity);

        bool SetAngularVelocity(
            BodyHandle bodyHandle,
            const AZ::Vector3& velocity);

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocityAtLocalPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& localPoint) const;

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocityAtWorldPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& worldPoint) const;

        bool SetKinematicTarget(
            BodyHandle bodyHandle,
            const AZ::Transform& transform,
            float fixedTimeStep,
            bool wake);

        bool ApplyLinearImpulse(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            bool wake);

        bool ApplyLinearImpulseAtWorldPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const AZ::Vector3& worldPoint,
            bool wake);

        bool ApplyAngularImpulse(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            bool wake);

        bool ApplyForce(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool wake);

        bool ApplyForceAtWorldPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& worldPoint,
            bool wake);

        bool ApplyTorque(
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool wake);

        [[nodiscard]]
        bool GetMassProperties(
            BodyHandle bodyHandle,
            MassProperties& properties) const;

        bool SetMassProperties(
            BodyHandle bodyHandle,
            const MassProperties& properties);

        bool RecomputeMassFromShapes(BodyHandle bodyHandle);

        [[nodiscard]]
        AZ::Matrix3x3 GetWorldInverseInertia(BodyHandle bodyHandle) const;

        [[nodiscard]]
        AZ::Vector3 GetWorldCenterOfMass(BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool GetBodyClosestPoint(
            BodyHandle bodyHandle,
            const AZ::Vector3& target,
            AZ::Vector3& position,
            float& distance) const;

        [[nodiscard]]
        AZ::Aabb GetBodyAabb(BodyHandle bodyHandle) const;

        [[nodiscard]]
        BufferResult GetBodyShapes(
            BodyHandle bodyHandle,
            AZStd::span<ShapeHandle> shapeHandles) const;

        [[nodiscard]]
        BufferResult GetBodyJoints(
            BodyHandle bodyHandle,
            AZStd::span<JointHandle> jointHandles) const;

        [[nodiscard]]
        ContactSnapshotResult GetBodyContacts(
            BodyHandle bodyHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const;

        [[nodiscard]]
        BufferResult GetBodySensorOverlaps(
            BodyHandle bodyHandle,
            AZStd::span<SensorOverlap> overlaps) const;

        [[nodiscard]]
        bool RaycastBody(
            BodyHandle bodyHandle,
            const BodyRaycastRequest& request,
            QueryHit& hit) const;

        [[nodiscard]]
        bool ShapeCastBody(
            BodyHandle bodyHandle,
            const BodyShapeCastRequest& request,
            QueryHit& hit) const;

        [[nodiscard]]
        bool OverlapBody(
            BodyHandle bodyHandle,
            const BodyOverlapRequest& request) const;

        [[nodiscard]]
        ShapeHandle CreateShape(
            BodyHandle bodyHandle,
            const ShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            BodyHandle bodyHandle,
            const ShapeConfiguration& configuration,
            float uniformScale);

        [[nodiscard]]
        ShapeHandle CreateShapeFromCooked(
            BodyHandle bodyHandle,
            const NativeGeometry& geometry,
            const ShapeProperties& properties);

        bool UpdateShape(
            ShapeHandle shapeHandle,
            const ShapeConfiguration& configuration);

        bool UpdateShape(
            ShapeHandle shapeHandle,
            const ShapeConfiguration& configuration,
            float uniformScale);

        bool DestroyShape(
            ShapeHandle shapeHandle,
            bool updateBodyMass);

        bool SetShapeCollisionFilter(
            ShapeHandle shapeHandle,
            const CollisionFilter& collisionFilter);

        bool SetShapeMaterials(
            ShapeHandle shapeHandle,
            AZStd::span<const MaterialHandle> materials);

        [[nodiscard]]
        AZ::Aabb GetShapeAabb(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        bool GetShapeState(
            ShapeHandle shapeHandle,
            ShapeState& state) const;

        [[nodiscard]]
        BufferResult GetShapeMaterials(
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        bool SetShapeDensity(
            ShapeHandle shapeHandle,
            float density,
            bool updateBodyMass);

        bool SetShapeFriction(
            ShapeHandle shapeHandle,
            float friction);

        bool SetShapeRestitution(
            ShapeHandle shapeHandle,
            float restitution);

        bool SetShapeEventSubscriptions(
            ShapeHandle shapeHandle,
            bool sensorEvents,
            bool contactEvents,
            bool hitEvents,
            bool preSolveEvents);

        [[nodiscard]]
        bool GetShapeMassProperties(
            ShapeHandle shapeHandle,
            MassProperties& properties) const;

        [[nodiscard]]
        bool GetShapeClosestPoint(
            ShapeHandle shapeHandle,
            const AZ::Vector3& target,
            AZ::Vector3& position,
            float& distance) const;

        [[nodiscard]]
        bool RaycastShape(
            ShapeHandle shapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            QueryHit& hit) const;

        [[nodiscard]]
        ContactSnapshotResult GetShapeContacts(
            ShapeHandle shapeHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const;

        [[nodiscard]]
        BufferResult GetShapeSensorOverlaps(
            ShapeHandle shapeHandle,
            AZStd::span<SensorOverlap> overlaps) const;

        [[nodiscard]]
        bool UsesMaterial(MaterialHandle materialHandle) const;

        bool RefreshMaterial(MaterialHandle materialHandle);

        [[nodiscard]]
        JointHandle CreateJoint(const JointConfiguration& configuration);

        bool UpdateJoint(
            JointHandle jointHandle,
            const JointConfiguration& configuration);

        bool DestroyJoint(
            JointHandle jointHandle,
            bool wakeAttachedBodies);

        bool WakeJointBodies(JointHandle jointHandle);

        [[nodiscard]]
        bool GetJointConfiguration(
            JointHandle jointHandle,
            JointConfiguration& configuration) const;

        [[nodiscard]]
        bool GetJointMeasurements(
            JointHandle jointHandle,
            JointMeasurements& measurements) const;

        [[nodiscard]]
        CharacterHandle CreateCharacter(const CharacterConfiguration& configuration);

        bool UpdateCharacter(
            CharacterHandle characterHandle,
            const CharacterConfiguration& configuration);

        bool DestroyCharacter(CharacterHandle characterHandle);

        bool MoveCharacter(
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity,
            float fixedTimeStep);

        [[nodiscard]]
        bool GetCharacterState(
            CharacterHandle characterHandle,
            CharacterState& state) const;

        [[nodiscard]]
        bool GetCharacterConfiguration(
            CharacterHandle characterHandle,
            CharacterConfiguration& configuration) const;

        [[nodiscard]]
        bool RaycastClosest(
            const RaycastRequest& request,
            QueryHit& hit) const override;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestQueryResult> results) const override;

        [[nodiscard]]
        QueryResult Raycast(
            const RaycastRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult ShapeCast(
            const ShapeCastRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult Overlap(
            const OverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        QueryResult Overlap(
            const OverlapRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapAabb(
            const AabbOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapAabb(
            const AabbOverlapRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        StepEvents GetStepEvents() const;

        bool SetContactCallbacks(
            CollisionFilterCallback collisionFilterCallback,
            PreSolveCallback preSolveCallback,
            void* userData);

        [[nodiscard]]
        bool GetStatistics(
            StatisticsFlags flags,
            WorldStatistics& statistics) const;

        [[nodiscard]]
        bool StartRecording(size_t initialCapacityBytes);

        [[nodiscard]]
        bool StopRecording(AZStd::vector<AZ::u8>& data);

        [[nodiscard]]
        bool Draw(
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer) const;

        [[nodiscard]]
        bool RebuildStaticTree();

        [[nodiscard]]
        bool Explode(const ExplosionConfiguration& configuration);

        [[nodiscard]]
        bool ApplyWind(
            BodyHandle bodyHandle,
            const WindConfiguration& configuration);

    private:
        friend class System;
        friend struct WorldLayoutChecks;

        template<typename Hit>
        [[nodiscard]]
        QueryResult OverlapImpl(
            const OverlapRequest& request,
            AZStd::span<Hit> hits) const;

        template<typename Hit>
        [[nodiscard]]
        QueryResult OverlapAabbImpl(
            const AabbOverlapRequest& request,
            AZStd::span<Hit> hits) const;

        static constexpr AZ::u32 InvalidSlotIndex = AZStd::numeric_limits<AZ::u32>::max();

        struct BodySlot final
        {
            BodyId m_nativeId;
            AZ::EntityId m_entityId{};

            AZ::u32 m_generation = 0;
            AZ::u32 m_firstShapeIndex = InvalidSlotIndex;

            NativeBodyType m_type = NativeBodyType::Static;
            bool m_allowFastRotation = false;
        };

        struct ShapeSlot final
        {
            ShapeId m_nativeId;
            BodyHandle m_bodyHandle;

            AZ::u32 m_generation = 0;
            AZ::u32 m_nextShapeIndex = InvalidSlotIndex;

            float m_explosionScale = 1.0f;

            bool m_isSensor = false;
            bool m_enableCustomFiltering = false;
            bool m_isCompound = false;
        };

        struct ShapeResources final
        {
            NativeGeometry m_geometry;
            AZStd::vector<MaterialHandle> m_materials;
        };

        struct ResolvedShape final
        {
            const ShapeSlot* m_slot = nullptr;
            ShapeHandle m_shapeHandle;
            AZ::u32 m_index = InvalidSlotIndex;
        };

        struct JointSlot final
        {
            JointId m_nativeId;
            AZ::EntityId m_entityId{};
            AZ::u32 m_generation = 0;
            JointKind m_kind = JointKind::None;
        };

        struct CharacterSlot final
        {
            AZ::Vector3 m_pendingVelocity = AZ::Vector3::CreateZero();

            AZ::u32 m_generation = 0;

            bool m_hasPendingMove = false;
        };

        struct CharacterResources final
        {
            CharacterConfiguration m_configuration;
            CharacterState m_state;
        };

        struct EntityBodyMove final
        {
            AZ::EntityId m_entityId;
            BodyMoveEvent m_event;
        };

        struct EntityJointThreshold final
        {
            AZ::EntityId m_entityId;
            JointThresholdEvent m_event;
        };

        struct EntityCharacterMove final
        {
            AZ::EntityId m_entityId;
            CharacterState m_state;
        };

        struct RetiredShapeProvenance final
        {
            ShapeId m_nativeId;
            BodyHandle m_bodyHandle;
            ShapeHandle m_shapeHandle;
        };

        [[nodiscard]]
        BodySlot* FindBodySlot(BodyHandle bodyHandle);

        [[nodiscard]]
        const BodySlot* FindBodySlot(BodyHandle bodyHandle) const;

        [[nodiscard]]
        ShapeSlot* FindShapeSlot(ShapeHandle shapeHandle);

        [[nodiscard]]
        const ShapeSlot* FindShapeSlot(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        bool ResolveShape(
            void* userData,
            ShapeId nativeId,
            ResolvedShape& resolvedShape) const;

        [[nodiscard]]
        bool ResolveShape(
            ShapeId nativeId,
            ResolvedShape& resolvedShape) const;

        void RegisterShape(
            ShapeId nativeId,
            AZ::u32 shapeIndex);

        void UnregisterShape(ShapeId nativeId);

        [[nodiscard]]
        JointSlot* FindJointSlot(JointHandle jointHandle);

        [[nodiscard]]
        const JointSlot* FindJointSlot(JointHandle jointHandle) const;

        bool SetJointEntityId(
            JointHandle jointHandle,
            AZ::EntityId entityId);

        [[nodiscard]]
        CharacterSlot* FindCharacterSlot(
            CharacterHandle characterHandle,
            AZ::u32* characterIndex = nullptr);

        [[nodiscard]]
        const CharacterSlot* FindCharacterSlot(
            CharacterHandle characterHandle,
            AZ::u32* characterIndex = nullptr) const;

        [[nodiscard]]
        ShapeHandle AttachShape(
            BodyHandle bodyHandle,
            const ShapeProperties& properties,
            const ShapeGeometry* geometry,
            const NativeGeometry* cookedGeometry,
            float uniformScale = 1.0f);

        [[nodiscard]]
        ShapeId CreateNativeShape(
            AZ::u32 shapeIndex,
            BodyId bodyId,
            ShapeSlot& shapeSlot,
            ShapeResources& resources,
            const ShapeProperties& properties,
            const ShapeGeometry* geometry,
            const NativeGeometry* cookedGeometry,
            float uniformScale);

        [[nodiscard]]
        bool RecreateShapeWithMaterials(
            ShapeHandle shapeHandle,
            ShapeSlot& shapeSlot,
            ShapeResources& resources,
            AZStd::span<const SurfaceMaterial> materials,
            float density,
            float explosionScale,
            AZStd::span<const MaterialHandle> materialHandles);

        [[nodiscard]]
        bool GetCompoundSourceMaterials(
            const ShapeSlot& shapeSlot,
            const ShapeResources& resources,
            AZStd::vector<SurfaceMaterial>& materials) const;

        [[nodiscard]]
        bool BuildNativeJointConfiguration(
            AZ::u32 jointIndex,
            const JointConfiguration& configuration,
            NativeJointConfiguration& nativeConfiguration) const;

        [[nodiscard]]
        bool MoveCharacterImmediately(
            CharacterResources& resources,
            const AZ::Vector3& velocity,
            float fixedTimeStep);

        void EnsureCharacterScratchCapacity(size_t maximumPlaneCount);

        [[nodiscard]]
        bool ApplyPendingCharacterMoves(
            float fixedTimeStep,
            bool consume);

        [[nodiscard]]
        BodyHandle GetBodyHandle(const BodyMove& bodyMove) const;

        [[nodiscard]]
        BodyHandle GetBodyHandle(const ShapeData& shapeData) const;

        [[nodiscard]]
        ShapeHandle GetShapeHandle(const ShapeData& shapeData) const;

        [[nodiscard]]
        bool ResolveShapeHandles(
            ShapeId nativeId,
            BodyHandle& bodyHandle,
            ShapeHandle& shapeHandle) const;

        [[nodiscard]]
        bool CanUseClosestRaycast(const QueryFilter& filter) const;

        [[nodiscard]]
        bool RaycastClosestDefaultUnlocked(
            const RaycastRequest& request,
            QueryHit& hit) const;

        [[nodiscard]]
        bool RaycastClosestFilteredUnlocked(
            const RaycastRequest& request,
            QueryHit& hit) const;

        [[nodiscard]]
        ContactSnapshotResult CopyContactSnapshots(
            AZStd::span<const ContactData> nativeContacts,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const;

        [[nodiscard]]
        BufferResult CopySensorOverlaps(
            AZStd::span<const SensorOverlapData> nativeOverlaps,
            AZStd::span<SensorOverlap> overlaps) const;

        void GatherStepEvents(bool append);

        bool ShouldCollide(
            ShapeId shapeA,
            ShapeId shapeB) override;

        bool BeforeSolve(
            ShapeId shapeA,
            ShapeId shapeB,
            const AZ::Vector3& position,
            const AZ::Vector3& normal) override;

        System& m_system;
        Internal::GenerationSource& m_bodyGenerations;
        Internal::GenerationSource& m_shapeGenerations;
        Internal::GenerationSource& m_jointGenerations;
        Internal::GenerationSource& m_characterGenerations;

        WorldConfiguration m_configuration;
        SystemConfiguration m_systemConfiguration;

        WorldHandle m_worldHandle;
        NativeTaskContext m_nativeTaskContext;
        WorldId m_nativeId;

        AZStd::vector<BodySlot> m_bodySlots;
        AZStd::vector<AZ::Name> m_bodyNames;
        AZStd::vector<AZ::u32> m_freeBodySlots;

        AZStd::vector<ShapeSlot> m_shapeSlots;
        AZStd::vector<ShapeResources> m_shapeResources;
        AZStd::vector<AZ::u32> m_nativeShapeSlots;
        AZStd::vector<AZ::u32> m_freeShapeSlots;

        AZStd::vector<JointSlot> m_jointSlots;
        AZStd::vector<JointConfiguration> m_jointConfigurations;
        AZStd::vector<AZ::u32> m_freeJointSlots;

        AZStd::vector<CharacterSlot> m_characterSlots;
        AZStd::vector<CharacterResources> m_characterResources;
        AZStd::vector<AZ::u32> m_freeCharacterSlots;
        AZStd::unique_ptr<MoverScratch> m_characterScratch;
        AZStd::unique_ptr<MoverScratch> m_characterStepScratch;
        size_t m_characterScratchCapacity = 0;

        AZStd::vector<BodyMove> m_nativeBodyMoves;
        AZStd::vector<SensorTransition> m_nativeSensorEvents;
        AZStd::vector<ContactTransition> m_nativeContactEvents;
        AZStd::vector<ContactId> m_activeContactIds;
        AZStd::vector<ContactId> m_endedContactIds;
        AZStd::vector<ContactHit> m_nativeContactHits;
        AZStd::vector<NativeJointThresholdEvent> m_nativeJointEvents;
        AZStd::vector<RetiredShapeProvenance> m_retiredShapes;

        mutable AZStd::vector<ContactData> m_nativeContactSnapshots;
        mutable AZStd::vector<NativeContactPoint> m_nativeContactPoints;
        mutable ContactScratch m_nativeContactScratch;
        mutable AZStd::vector<SensorOverlapData> m_nativeSensorOverlaps;
        mutable SensorScratch m_nativeSensorScratch;
        mutable JointScratch m_nativeBodyJoints;

        AZStd::vector<BodyMoveEvent> m_bodyMoveEvents;
        AZStd::vector<SensorEvent> m_sensorEvents;
        AZStd::vector<ContactEvent> m_contactEvents;
        AZStd::vector<ContactPoint> m_contactPoints;
        AZStd::vector<ContactHitEvent> m_contactHitEvents;
        AZStd::vector<JointThresholdEvent> m_jointEvents;

        AZStd::vector<EntityBodyMove> m_entityBodyMoves;
        AZStd::vector<EntityJointThreshold> m_entityJointThresholds;
        AZStd::vector<EntityCharacterMove> m_entityCharacterMoves;

        AZStd::unique_ptr<Recording> m_recording;

        CollisionFilterCallback m_collisionFilterCallback = nullptr;
        PreSolveCallback m_preSolveCallback = nullptr;
        void* m_contactCallbackUserData = nullptr;

        mutable DeterministicRecursiveMutex m_mutex;

        double m_accumulatedTime = 0.0;
        SimulationTick m_lastCompletedTick = 0;

        AZ::u32 m_worldIndex = 0;
        AZ::u32 m_sensorShapeCount = 0;
        AZ::u32 m_entityBodyCount = 0;
        AZ::u32 m_entityJointCount = 0;
    };
} // namespace Box3D
