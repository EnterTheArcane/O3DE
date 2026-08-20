/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Cooking.h>
#include <Box3D/Diagnostics.h>
#include <Box3D/Effects.h>
#include <Box3D/Internal/HandleEncoding.h>
#include <Box3D/NativeApi.h>
#include <Box3D/System.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

namespace Box3D
{
    class World;

    class BOX3D_API System final
        : public AZ::Interface<ISystem>::Registrar
        , public AZ::Interface<ICooking>::Registrar
        , public AZ::Interface<IDiagnostics>::Registrar
        , public AZ::Interface<IEffects>::Registrar
        , private IMaterialCallbacks
    {
    public:
        AZ_CLASS_ALLOCATOR(System, AZ::SystemAllocator);

        AZ_RTTI(System, "{DF0E67A4-324A-4890-B168-CD326D7A6A89}", ISystem, ICooking, IDiagnostics, IEffects);

        explicit System(
            const SystemConfiguration& configuration = {},
            AZ::JobContext* defaultJobContext = nullptr);
        ~System() override;
        AZ_DISABLE_COPY_MOVE(System);

        void UpdateConfiguration(const SystemConfiguration& configuration) override;

        [[nodiscard]]
        const SystemConfiguration& GetConfiguration() const override;

        [[nodiscard]]
        WorldHandle CreateWorld(const WorldConfiguration& configuration) override;

        bool DestroyWorld(WorldHandle worldHandle) override;

        [[nodiscard]]
        WorldHandle GetDefaultWorldHandle() const override;

        [[nodiscard]]
        const IWorldQueries* GetWorldQueries(WorldHandle worldHandle) const override;

        [[nodiscard]]
        WorldHandle FindWorld(AZ::Name name) const override;

        [[nodiscard]]
        bool GetWorldConfiguration(
            WorldHandle worldHandle,
            WorldConfiguration& configuration) const override;

        [[nodiscard]]
        AZ::Aabb GetWorldAabb(WorldHandle worldHandle) const override;

        [[nodiscard]]
        bool IsValid(WorldHandle worldHandle) const override;

        bool SetWorldEnabled(
            WorldHandle worldHandle,
            bool enabled) override;

        [[nodiscard]]
        bool IsWorldEnabled(WorldHandle worldHandle) const override;

        bool SetWorldGravity(
            WorldHandle worldHandle,
            const AZ::Vector3& gravity) override;

        [[nodiscard]]
        bool GetWorldGravity(
            WorldHandle worldHandle,
            AZ::Vector3& gravity) const override;

        bool StepWorld(
            WorldHandle worldHandle,
            float fixedTimeStep) override;

        void StepAutoSimulatedWorlds(float deltaTime) override;

        [[nodiscard]]
        SimulationTick GetLastCompletedTick(WorldHandle worldHandle) const override;

        [[nodiscard]]
        AZ::u64 GetStateDigest(WorldHandle worldHandle) const override;

        [[nodiscard]]
        AZStd::string_view GetCompatibilityFingerprint() const override;

        [[nodiscard]]
        MaterialHandle CreateMaterial(const MaterialConfiguration& configuration) override;

        bool UpdateMaterial(
            MaterialHandle materialHandle,
            const MaterialConfiguration& configuration) override;

        [[nodiscard]]
        bool GetMaterial(
            MaterialHandle materialHandle,
            MaterialConfiguration& configuration) const override;

        bool DestroyMaterial(MaterialHandle materialHandle) override;

        [[nodiscard]]
        CookedShapeHandle CookShape(const ShapeConfiguration& configuration) override;

        bool DestroyCookedShape(CookedShapeHandle cookedShapeHandle) override;

        [[nodiscard]]
        bool IsValid(CookedShapeHandle cookedShapeHandle) const override;

        [[nodiscard]]
        AZ::Aabb GetAabb(CookedShapeHandle cookedShapeHandle) const override;

        [[nodiscard]]
        bool Raycast(
            CookedShapeHandle cookedShapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            GeometryHit& hit) const override;

        [[nodiscard]]
        BodyHandle CreateBody(
            WorldHandle worldHandle,
            const RigidBodyConfiguration& configuration) override;

        bool DestroyBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        bool GetBodyState(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyState& state) const override;

        [[nodiscard]]
        AZ::Name GetBodyName(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        bool SetBodyName(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Name name) override;

        [[nodiscard]]
        bool GetBodyProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            BodyProperties& properties) const override;

        bool SetBodyProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyProperties& properties) override;

        bool SetBodyAwake(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool awake) override;

        bool SetBodyEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        bool SetBodyHitEventsEnabled(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool enabled) override;

        bool SetBodyTransform(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Transform& transform) override;

        [[nodiscard]]
        bool GetBodyLocalPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& worldPoint,
            AZ::Vector3& localPoint) const override;

        [[nodiscard]]
        bool GetBodyWorldPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& localPoint,
            AZ::Vector3& worldPoint) const override;

        [[nodiscard]]
        bool GetBodyLocalVector(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& worldVector,
            AZ::Vector3& localVector) const override;

        [[nodiscard]]
        bool GetBodyWorldVector(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& localVector,
            AZ::Vector3& worldVector) const override;

        bool SetLinearVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& velocity) override;

        bool SetAngularVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& velocity) override;

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocityAtLocalPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& localPoint) const override;

        [[nodiscard]]
        AZ::Vector3 GetLinearVelocityAtWorldPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& worldPoint) const override;

        bool SetKinematicTarget(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Transform& transform,
            float fixedTimeStep,
            bool wake = true) override;

        bool ApplyLinearImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            bool wake = true) override;

        bool ApplyLinearImpulseAtWorldPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const AZ::Vector3& worldPoint,
            bool wake = true) override;

        bool ApplyAngularImpulse(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            bool wake = true) override;

        bool ApplyForce(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool wake) override;

        bool ApplyForceAtWorldPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& worldPoint,
            bool wake) override;

        bool ApplyTorque(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool wake) override;

        [[nodiscard]]
        bool GetMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            MassProperties& properties) const override;

        bool SetMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const MassProperties& properties) override;

        bool RecomputeMassFromShapes(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        AZ::Matrix3x3 GetWorldInverseInertia(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        [[nodiscard]]
        AZ::Vector3 GetWorldCenterOfMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        [[nodiscard]]
        bool GetBodyClosestPoint(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& target,
            AZ::Vector3& position,
            float& distance) const override;

        [[nodiscard]]
        AZ::Aabb GetBodyAabb(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) const override;

        [[nodiscard]]
        BufferResult GetBodyShapes(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<ShapeHandle> shapeHandles) const override;

        [[nodiscard]]
        BufferResult GetBodyJoints(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<JointHandle> jointHandles) const override;

        [[nodiscard]]
        ContactSnapshotResult GetBodyContacts(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const override;

        [[nodiscard]]
        BufferResult GetBodySensorOverlaps(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SensorOverlap> overlaps) const override;

        [[nodiscard]]
        bool RaycastBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyRaycastRequest& request,
            QueryHit& hit) const override;

        [[nodiscard]]
        bool ShapeCastBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyShapeCastRequest& request,
            QueryHit& hit) const override;

        [[nodiscard]]
        bool OverlapBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const BodyOverlapRequest& request) const override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const ShapeConfiguration& configuration) override;

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const ShapeConfiguration& configuration,
            float uniformScale);

        [[nodiscard]]
        ShapeHandle CreateShapeFromCooked(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            CookedShapeHandle cookedShapeHandle,
            const ShapeProperties& properties) override;

        bool UpdateShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const ShapeConfiguration& configuration) override;

        bool UpdateShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const ShapeConfiguration& configuration,
            float uniformScale);

        bool DestroyShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            bool updateBodyMass = true) override;

        bool SetShapeCollisionFilter(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const CollisionFilter& collisionFilter) override;

        bool SetShapeMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<const MaterialHandle> materials) override;

        [[nodiscard]]
        AZ::Aabb GetShapeAabb(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) const override;

        [[nodiscard]]
        bool GetShapeState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeState& state) const override;

        [[nodiscard]]
        BufferResult GetShapeMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const override;

        bool SetShapeDensity(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            float density,
            bool updateBodyMass = true) override;

        bool SetShapeFriction(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            float friction) override;

        bool SetShapeRestitution(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            float restitution) override;

        bool SetShapeEventSubscriptions(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            bool sensorEvents,
            bool contactEvents,
            bool hitEvents,
            bool preSolveEvents) override;

        [[nodiscard]]
        bool GetShapeMassProperties(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            MassProperties& properties) const override;

        [[nodiscard]]
        bool GetShapeClosestPoint(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& target,
            AZ::Vector3& position,
            float& distance) const override;

        [[nodiscard]]
        bool RaycastShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& start,
            const AZ::Vector3& direction,
            float distance,
            QueryHit& hit) const override;

        [[nodiscard]]
        ContactSnapshotResult GetShapeContacts(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<ContactSnapshot> contacts,
            AZStd::span<ContactPoint> points) const override;

        [[nodiscard]]
        BufferResult GetShapeSensorOverlaps(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<SensorOverlap> overlaps) const override;

        [[nodiscard]]
        JointHandle CreateJoint(
            WorldHandle worldHandle,
            const JointConfiguration& configuration) override;

        bool SetJointEntityId(
            WorldHandle worldHandle,
            JointHandle jointHandle,
            AZ::EntityId entityId);

        bool UpdateJoint(
            WorldHandle worldHandle,
            JointHandle jointHandle,
            const JointConfiguration& configuration) override;

        bool DestroyJoint(
            WorldHandle worldHandle,
            JointHandle jointHandle,
            bool wakeAttachedBodies = true) override;

        bool WakeJointBodies(
            WorldHandle worldHandle,
            JointHandle jointHandle) override;

        [[nodiscard]]
        bool GetJointConfiguration(
            WorldHandle worldHandle,
            JointHandle jointHandle,
            JointConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetJointMeasurements(
            WorldHandle worldHandle,
            JointHandle jointHandle,
            JointMeasurements& measurements) const override;

        [[nodiscard]]
        CharacterHandle CreateCharacter(
            WorldHandle worldHandle,
            const CharacterConfiguration& configuration) override;

        bool UpdateCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const CharacterConfiguration& configuration) override;

        bool DestroyCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle) override;

        bool MoveCharacter(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity,
            float fixedTimeStep) override;

        [[nodiscard]]
        bool GetCharacterState(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterState& state) const override;

        [[nodiscard]]
        bool GetCharacterConfiguration(
            WorldHandle worldHandle,
            CharacterHandle characterHandle,
            CharacterConfiguration& configuration) const override;

        [[nodiscard]]
        bool RaycastClosest(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            QueryHit& hit) const override;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            WorldHandle worldHandle,
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestQueryResult> results) const override;

        [[nodiscard]]
        QueryResult Raycast(
            WorldHandle worldHandle,
            const RaycastRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult ShapeCast(
            WorldHandle worldHandle,
            const ShapeCastRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult Overlap(
            WorldHandle worldHandle,
            const OverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        QueryResult Overlap(
            WorldHandle worldHandle,
            const OverlapRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapAabb(
            WorldHandle worldHandle,
            const AabbOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapAabb(
            WorldHandle worldHandle,
            const AabbOverlapRequest& request,
            AZStd::span<QueryHit> hits) const override;

        [[nodiscard]]
        StepEvents GetStepEvents(WorldHandle worldHandle) const override;

        bool SetContactCallbacks(
            WorldHandle worldHandle,
            CollisionFilterCallback collisionFilterCallback,
            PreSolveCallback preSolveCallback,
            void* userData) override;

        [[nodiscard]]
        bool GetWorldStatistics(
            WorldHandle worldHandle,
            StatisticsFlags flags,
            WorldStatistics& statistics) const override;

        [[nodiscard]]
        bool StartRecording(
            WorldHandle worldHandle,
            size_t initialCapacityBytes) override;

        [[nodiscard]]
        bool StopRecording(
            WorldHandle worldHandle,
            AZStd::vector<AZ::u8>& data) override;

        [[nodiscard]]
        bool ValidateRecording(
            AZStd::span<const AZ::u8> data,
            AZ::u32 workerCount) const override;

        [[nodiscard]]
        AZStd::unique_ptr<IReplay> CreateReplay(
            AZStd::span<const AZ::u8> data,
            AZ::u32 workerCount) const override;

        [[nodiscard]]
        bool DrawWorld(
            WorldHandle worldHandle,
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer) const override;

        [[nodiscard]]
        bool RebuildStaticTree(WorldHandle worldHandle) override;

        [[nodiscard]]
        bool ApplyWind(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const WindConfiguration& configuration) override;

        [[nodiscard]]
        bool Explode(
            WorldHandle worldHandle,
            const ExplosionConfiguration& configuration) override;

        [[nodiscard]]
        SurfaceMaterial ResolveMaterial(MaterialHandle materialHandle) const;

    private:
        friend struct SystemLayoutChecks;

        struct WorldSlot final
        {
            AZStd::unique_ptr<World> m_world;
            AZ::u32 m_generation = 1;
        };

        struct MaterialSlot final
        {
            AZ::u32 m_generation = 0;
        };

        struct CookedShapeSlot final
        {
            AZ::u32 m_generation = 0;
        };

        struct CookedShapeResources final
        {
            NativeGeometry m_geometry;
            AZStd::vector<MaterialHandle> m_materials;
        };

        [[nodiscard]]
        World* FindWorldInstance(WorldHandle worldHandle);

        [[nodiscard]]
        const World* FindWorldInstance(WorldHandle worldHandle) const;

        [[nodiscard]]
        MaterialSlot* FindMaterialSlot(
            MaterialHandle materialHandle,
            AZ::u32* materialIndex = nullptr);

        [[nodiscard]]
        const MaterialSlot* FindMaterialSlot(
            MaterialHandle materialHandle,
            AZ::u32* materialIndex = nullptr) const;

        [[nodiscard]]
        CookedShapeSlot* FindCookedShapeSlot(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32* cookedShapeIndex = nullptr);

        [[nodiscard]]
        const CookedShapeSlot* FindCookedShapeSlot(
            CookedShapeHandle cookedShapeHandle,
            AZ::u32* cookedShapeIndex = nullptr) const;

        [[nodiscard]]
        bool UsesCookedMaterial(MaterialHandle materialHandle) const;

        void DispatchStepEvents(const World& world) const;

        [[nodiscard]]
        SurfaceTypeId ResolveSurfaceType(AZ::u64 materialId) const;

        [[nodiscard]]
        float MixFriction(
            float valueA,
            AZ::u64 materialIdA,
            float valueB,
            AZ::u64 materialIdB) const override;

        [[nodiscard]]
        float MixRestitution(
            float valueA,
            AZ::u64 materialIdA,
            float valueB,
            AZ::u64 materialIdB) const override;

        void UpdateCompatibilityFingerprint();

        SystemConfiguration m_configuration;

        AZStd::vector<WorldSlot> m_worldSlots;
        AZStd::vector<AZ::u32> m_freeWorldSlots;

        AZStd::array<Internal::GenerationSource, Internal::MaximumWorldCount> m_bodyGenerations;
        AZStd::array<Internal::GenerationSource, Internal::MaximumWorldCount> m_shapeGenerations;
        AZStd::array<Internal::GenerationSource, Internal::MaximumWorldCount> m_jointGenerations;
        AZStd::array<Internal::GenerationSource, Internal::MaximumWorldCount> m_characterGenerations;

        AZStd::vector<MaterialSlot> m_materialSlots;
        AZStd::vector<MaterialConfiguration> m_materialConfigurations;
        AZStd::vector<AZ::u32> m_freeMaterialSlots;
        Internal::GenerationSource m_materialGenerations;

        AZStd::vector<CookedShapeSlot> m_cookedShapeSlots;
        AZStd::vector<CookedShapeResources> m_cookedShapeResources;
        AZStd::vector<AZ::u32> m_freeCookedShapeSlots;
        Internal::GenerationSource m_cookedShapeGenerations;

        AZStd::string m_compatibilityFingerprint;

        World* m_defaultWorldInstance = nullptr;
        WorldHandle m_defaultWorldHandle;
    };
} // namespace Box3D
