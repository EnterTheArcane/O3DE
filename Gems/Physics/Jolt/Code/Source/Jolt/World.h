/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/FloatEnvironment.h>
#include <Jolt/HairInternal.h>
#include <Jolt/JobSystem.h>
#include <Jolt/Capabilities.h>
#include <Jolt/TransformedShapeLease.h>

#include <AzCore/Jobs/JobContext.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Array.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>
#include <Jolt/Physics/Collision/SimShapeFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Compute/ComputeQueue.h>
#include <Jolt/Skeleton/SkeletonPose.h>

namespace JPH
{
    class Body;
    class CollideShapeSettings;
    class ConvexHullShape;
    class HeightFieldShape;
    class ShapeFilter;
    class TransformedShape;
} // namespace JPH

namespace Jolt
{
    namespace Internal
    {
        class StateArchiveReader;
        class StateArchiveWriter;
    }

    class DebugCapture;
    class DebugRenderer;
    class RuntimeImplementation;
    class World;

    struct TransformedShapeLeaseState final
    {
        AZStd::mutex m_mutex;
        World* m_world = nullptr;
        size_t m_referenceCount = 1;
    };

    struct WorldPerformanceAccumulator final
    {
        AZStd::atomic_uint64_t m_broadPhaseOptimizeCount{0};
        AZStd::atomic_uint64_t m_broadPhaseOptimizeNanoseconds{0};
        AZStd::atomic_uint64_t m_originShiftCount{0};

        AZStd::atomic_uint64_t m_contactEventCount{0};
        AZStd::atomic_uint64_t m_contactPointCount{0};

        AZStd::atomic_uint64_t m_droppedEventCount{0};
        AZStd::atomic_uint64_t m_eventHighWaterCount{0};
        AZStd::atomic_uint64_t m_publishedEventCount{0};

        AZStd::atomic_uint64_t m_queryCandidateCount{0};
        AZStd::atomic_uint64_t m_queryCount{0};
        AZStd::atomic_uint64_t m_queryHitCount{0};
        AZStd::atomic_uint64_t m_queryNanoseconds{0};

        AZStd::atomic_uint64_t m_snapshotCaptureCount{0};
        AZStd::atomic_uint64_t m_snapshotCaptureNanoseconds{0};
        AZStd::atomic_uint64_t m_snapshotFailureCount{0};
        AZStd::atomic_uint64_t m_snapshotPeakBytes{0};
        AZStd::atomic_uint64_t m_snapshotRestoreCount{0};
        AZStd::atomic_uint64_t m_snapshotRestoreNanoseconds{0};

        AZStd::atomic_uint64_t m_jobCount{0};
        AZStd::atomic_uint64_t m_jobExecutionNanoseconds{0};
        AZStd::atomic_uint64_t m_jobMaximumQueueLatencyNanoseconds{0};
        AZStd::atomic_uint64_t m_jobQueueLatencyNanoseconds{0};
        AZStd::atomic_uint64_t m_jobTaskCount{0};
        AZStd::atomic_uint32_t m_jobMaximumActiveTaskCount{0};

        AZStd::atomic_uint64_t m_hairReadbackBytes{0};
        AZStd::atomic_uint64_t m_hairReadbackCount{0};
        AZStd::atomic_uint64_t m_hairReadbackNanoseconds{0};
        AZStd::atomic_uint64_t m_hairUpdateCount{0};
        AZStd::atomic_uint64_t m_hairUpdateNanoseconds{0};

        AZStd::atomic_uint64_t m_simulationErrorCount{0};
        AZStd::atomic_uint64_t m_simulationNanoseconds{0};
        AZStd::atomic_uint64_t m_simulationStepCount{0};
    };

    class World final
        : public IWorldQueries
        , private JPH::BodyActivationListener
        , private JPH::CharacterContactListener
        , private JPH::ContactListener
        , private JPH::PhysicsStepListener
        , private JPH::SimShapeFilter
        , private JPH::SoftBodyContactListener
    {
    public:
        World(
            RuntimeImplementation& system,
            WorldHandle handle,
            AZ::u32 worldIndex,
            WorldConfiguration configuration,
            AZ::JobContext* jobContext);
        ~World() override;

        AZ_DISABLE_COPY_MOVE(World);

        constexpr explicit operator bool() const noexcept
        {
            return m_initialized;
        }

        [[nodiscard]]
        bool HasTransformedShapeLeases() const;

        [[nodiscard]]
        bool GetGravity(AZ::Vector3& gravity) const;

        bool SetGravity(const AZ::Vector3& gravity);

        [[nodiscard]]
        AZ::u32 GetWorkerCount() const;

        [[nodiscard]]
        bool GetSimulationConfiguration(SimulationConfiguration& configuration) const;

        bool UpdateSimulationConfiguration(const SimulationConfiguration& configuration);

        [[nodiscard]]
        bool GetRuntimeConfiguration(WorldRuntimeConfiguration& configuration) const;

        bool UpdateRuntimeConfiguration(const WorldRuntimeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(const ShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(const CompoundShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(const DecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(CookedShapeHandle cookedShapeHandle);

        [[nodiscard]]
        ShapeHandle CloneShape(ShapeHandle shapeHandle);

        [[nodiscard]]
        ShapeHandle ScaleShape(
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale);

        bool DestroyShape(ShapeHandle shapeHandle);

        [[nodiscard]]
        bool IsValid(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        bool GetShapeStats(
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetShapeStatsRecursive(
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetShapeProperties(
            ShapeHandle shapeHandle,
            ShapeProperties& properties) const;

        [[nodiscard]]
        bool GetShapeSubmergedVolume(
            ShapeHandle shapeHandle,
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetPrimitiveShapeState(
            ShapeHandle shapeHandle,
            PrimitiveShapeState& state) const;

        [[nodiscard]]
        bool GetConvexHullState(
            ShapeHandle shapeHandle,
            ConvexHullState& state) const;

        [[nodiscard]]
        BufferResult GetConvexHullPointsRelativeToCenterOfMass(
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Vector3> points) const;

        [[nodiscard]]
        BufferResult GetConvexHullPlanesRelativeToCenterOfMass(
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Plane> planes) const;

        [[nodiscard]]
        BufferResult GetConvexHullFaceVertexIndices(
            ShapeHandle shapeHandle,
            AZ::u32 faceIndex,
            AZStd::span<AZ::u32> vertexIndices) const;

        [[nodiscard]]
        bool GetShapeMaterial(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        [[nodiscard]]
        bool GetShapeSurfaceNormal(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetShapeUserData(
            ShapeHandle shapeHandle,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetShapeSubShapeUserData(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const;

        [[nodiscard]]
        bool GetDecoratedShapeConfiguration(
            ShapeHandle shapeHandle,
            DecoratedShapeConfiguration& configuration) const;

        [[nodiscard]]
        BufferResult GetMeshMaterials(
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const;

        [[nodiscard]]
        bool GetMeshTriangleUserData(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const;

        [[nodiscard]]
        bool IsShapeScaleValid(
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) const;

        [[nodiscard]]
        bool MakeShapeScaleValid(
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const;

        [[nodiscard]]
        bool GetHeightfieldState(
            ShapeHandle shapeHandle,
            HeightfieldState& state) const;

        [[nodiscard]]
        bool GetHeightfieldPosition(
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const;

        [[nodiscard]]
        bool ProjectOntoHeightfield(
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::Vector3& surfacePosition,
            SubShapeId& subShapeId) const;

        [[nodiscard]]
        bool IsHeightfieldNoCollision(
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const;

        [[nodiscard]]
        QueryResult GetHeightfieldHeights(
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<float> heights) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterialIndices(
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<AZ::u8> materialIndices) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterials(
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetHeightfieldSubShapeCoordinates(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const;

        bool UpdateHeightfieldHeights(
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const float> heights,
            const HeightfieldUpdateConfiguration& configuration);

        bool UpdateHeightfieldMaterials(
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const AZ::u8> materialIndices,
            AZStd::span<const MaterialHandle> materialHandles,
            bool activateBodies);

        bool AddMutableCompoundChild(
            ShapeHandle compoundShapeHandle,
            const CompoundChildConfiguration& child,
            AZ::u32 insertionIndex,
            AZ::u32& childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration);

        bool RemoveMutableCompoundChild(
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration);

        bool UpdateMutableCompoundChild(
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const CompoundChildConfiguration& child,
            const MutableCompoundUpdateConfiguration& updateConfiguration);

        bool UpdateMutableCompoundChildTransforms(
            ShapeHandle compoundShapeHandle,
            AZ::u32 startIndex,
            AZStd::span<const AZ::Vector3> positions,
            AZStd::span<const AZ::Quaternion> rotations,
            const MutableCompoundUpdateConfiguration& updateConfiguration);

        bool AdjustMutableCompoundCenterOfMass(
            ShapeHandle compoundShapeHandle,
            bool updateMassProperties,
            bool activateBodies);

        [[nodiscard]]
        bool GetCompoundChildCount(
            ShapeHandle compoundShapeHandle,
            AZ::u32& childCount) const;

        [[nodiscard]]
        bool GetCompoundChild(
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const;

        [[nodiscard]]
        bool GetCompoundChildIndex(
            ShapeHandle compoundShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const;

        [[nodiscard]]
        bool GetDirectChildShape(
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const;

        [[nodiscard]]
        BodyHandle CreateBody(const BodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateBodyWithId(
            BodyId bodyId,
            const BodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateSoftBody(const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle CreateSoftBodyWithId(
            BodyId bodyId,
            const SoftBodyConfiguration& configuration);

        bool AddBodyToSimulation(
            BodyHandle bodyHandle,
            bool activate);

        bool AddBodiesToSimulation(
            AZStd::span<const BodyHandle> bodyHandles,
            bool activate);

        bool RemoveBodyFromSimulation(BodyHandle bodyHandle);

        bool RemoveBodiesFromSimulation(AZStd::span<const BodyHandle> bodyHandles);

        bool DestroyBody(BodyHandle bodyHandle);

        bool DestroyBodies(AZStd::span<const BodyHandle> bodyHandles);

        [[nodiscard]]
        bool IsBodyInSimulation(BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool IsValid(BodyHandle bodyHandle) const;

        bool SetBodyMoveEventsEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        RagdollDefinitionHandle CreateRagdollDefinition(
            const RagdollDefinitionConfiguration& configuration);

        bool DestroyRagdollDefinition(RagdollDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(RagdollDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        QueryResult GetRagdollBodyConstraintIndices(
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<AZ::s32> constraintIndices) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraintBodyPairs(
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<RagdollConstraintBodyPair> bodyPairs) const;

        [[nodiscard]]
        RagdollHandle CreateRagdoll(const RagdollConfiguration& configuration);

        bool AddRagdollToSimulation(
            RagdollHandle ragdollHandle,
            bool activate);

        bool RemoveRagdollFromSimulation(RagdollHandle ragdollHandle);

        bool DestroyRagdoll(RagdollHandle ragdollHandle);

        [[nodiscard]]
        bool IsValid(RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool IsRagdollInSimulation(RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool GetRagdollState(
            RagdollHandle ragdollHandle,
            RagdollState& state) const;

        bool SetRagdollCollisionGroupId(
            RagdollHandle ragdollHandle,
            AZ::u32 collisionGroupId);

        [[nodiscard]]
        QueryResult GetRagdollBodies(
            RagdollHandle ragdollHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraints(
            RagdollHandle ragdollHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

        bool ActivateRagdoll(RagdollHandle ragdollHandle);

        bool SetRagdollPose(
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms);

        [[nodiscard]]
        QueryResult GetRagdollPose(
            RagdollHandle ragdollHandle,
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const;

        bool DriveRagdollKinematically(
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool DriveRagdollMotors(
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> modelTransforms);

        bool DriveRagdollMotors(
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool ResetRagdollWarmStart(RagdollHandle ragdollHandle);

        bool SetRagdollVelocity(
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity,
            AZ::Vector3 angularVelocity);

        bool SetRagdollLinearVelocity(
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollLinearVelocity(
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollImpulse(
            RagdollHandle ragdollHandle,
            AZ::Vector3 impulse);

        [[nodiscard]]
        ConstraintHandle CreateConstraint(const ConstraintConfiguration& configuration);

        bool AddConstraintToSimulation(ConstraintHandle constraintHandle);

        bool AddConstraintsToSimulation(AZStd::span<const ConstraintHandle> constraintHandles);

        bool RemoveConstraintFromSimulation(ConstraintHandle constraintHandle);

        bool RemoveConstraintsFromSimulation(AZStd::span<const ConstraintHandle> constraintHandles);

        bool DestroyConstraint(ConstraintHandle constraintHandle);

        bool DestroyConstraints(AZStd::span<const ConstraintHandle> constraintHandles);

        [[nodiscard]]
        bool IsConstraintInSimulation(ConstraintHandle constraintHandle) const;

        [[nodiscard]]
        bool IsValid(ConstraintHandle constraintHandle) const;

        [[nodiscard]]
        SceneInstanceHandle InstantiateScene(
            SceneDefinitionHandle definitionHandle,
            const SceneConfiguration& configuration);

        bool DestroySceneInstance(SceneInstanceHandle instanceHandle);

        [[nodiscard]]
        bool IsValid(SceneInstanceHandle instanceHandle) const;

        [[nodiscard]]
        bool GetSceneInstanceState(
            SceneInstanceHandle instanceHandle,
            SceneInstanceState& state) const;

        [[nodiscard]]
        QueryResult GetSceneBodies(
            SceneInstanceHandle instanceHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetSceneConstraints(
            SceneInstanceHandle instanceHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

        bool SetConstraintEnabled(
            ConstraintHandle constraintHandle,
            bool enabled);

        [[nodiscard]]
        bool GetConstraintState(
            ConstraintHandle constraintHandle,
            ConstraintState& state) const;

        [[nodiscard]]
        bool GetConstraintConfiguration(
            ConstraintHandle constraintHandle,
            ConstraintConfiguration& configuration) const;

        [[nodiscard]]
        bool GetConstraintUserData(
            ConstraintHandle constraintHandle,
            AZ::u64& userData) const;

        bool SetConstraintUserData(
            ConstraintHandle constraintHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetConstraintDebugDrawSize(
            ConstraintHandle constraintHandle,
            float& debugDrawSize) const;

        bool SetConstraintDebugDrawSize(
            ConstraintHandle constraintHandle,
            float debugDrawSize);

        [[nodiscard]]
        bool GetConstraintMeasurements(
            ConstraintHandle constraintHandle,
            ConstraintMeasurements& measurements) const;

        [[nodiscard]]
        bool GetCustomConstraintInfo(
            ConstraintHandle constraintHandle,
            CustomConstraintInfo& info) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintImpulses(
            ConstraintHandle constraintHandle,
            AZStd::span<float> impulses) const;

        [[nodiscard]]
        BufferResult GetCustomConstraintState(
            ConstraintHandle constraintHandle,
            AZStd::span<AZ::u8> state) const;

        bool SetCustomConstraintState(
            ConstraintHandle constraintHandle,
            AZStd::span<const AZ::u8> state);

        bool ResetConstraintWarmStart(ConstraintHandle constraintHandle);

        bool UpdateConstraintSolverConfiguration(
            ConstraintHandle constraintHandle,
            const ConstraintSolverConfiguration& configuration);

        bool UpdateConeLimit(
            ConstraintHandle constraintHandle,
            float halfConeAngle);

        bool UpdateDistanceLimits(
            ConstraintHandle constraintHandle,
            float minimumDistance,
            float maximumDistance,
            const SpringConfiguration& spring);

        bool UpdateHingeLimits(
            ConstraintHandle constraintHandle,
            float minimumAngle,
            float maximumAngle,
            const SpringConfiguration& spring,
            float maximumFrictionTorque);

        bool UpdateHingeMotor(
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetAngle,
            float targetAngularVelocity);

        bool SetHingeTargetOrientation(
            ConstraintHandle constraintHandle,
            const AZ::Quaternion& targetOrientation);

        bool UpdatePathMotor(
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPathFraction,
            float targetVelocity);

        bool UpdatePathProperties(
            ConstraintHandle constraintHandle,
            PathHandle pathHandle,
            float pathFraction,
            float maximumFrictionForce);

        bool UpdatePointAnchors(
            ConstraintHandle constraintHandle,
            ConstraintSpace space,
            const WorldPosition& firstPoint,
            const WorldPosition& secondPoint);

        bool UpdatePulleyLimits(
            ConstraintHandle constraintHandle,
            float minimumLength,
            float maximumLength);

        bool UpdateSixDofLimits(
            ConstraintHandle constraintHandle,
            AZStd::span<const SixDofAxisLimitConfiguration> axes);

        bool UpdateSixDofMotors(
            ConstraintHandle constraintHandle,
            AZStd::span<const MotorConfiguration> motors,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation,
            const AZ::Vector3& targetPosition,
            const AZ::Vector3& targetVelocity);

        bool UpdateSliderMotor(
            ConstraintHandle constraintHandle,
            const MotorConfiguration& motor,
            float targetPosition,
            float targetVelocity);

        bool UpdateSliderLimits(
            ConstraintHandle constraintHandle,
            float minimumPosition,
            float maximumPosition,
            const SpringConfiguration& spring,
            float maximumFrictionForce);

        bool UpdateSwingTwistMotors(
            ConstraintHandle constraintHandle,
            const MotorConfiguration& swingMotor,
            const MotorConfiguration& twistMotor,
            const AZ::Vector3& targetAngularVelocity,
            const AZ::Quaternion& targetOrientation);

        bool UpdateSwingTwistLimits(
            ConstraintHandle constraintHandle,
            float normalHalfConeAngle,
            float planeHalfConeAngle,
            float twistMinimumAngle,
            float twistMaximumAngle,
            float maximumFrictionTorque);

        [[nodiscard]]
        bool GetBodyState(
            BodyHandle bodyHandle,
            BodyState& state) const;

        [[nodiscard]]
        bool GetBodyCenterOfMassTransform(
            BodyHandle bodyHandle,
            WorldTransform& transform) const;

        [[nodiscard]]
        bool GetBodyConfiguration(
            BodyHandle bodyHandle,
            BodyConfiguration& configuration) const;

        [[nodiscard]]
        bool GetBodyUserData(
            BodyHandle bodyHandle,
            AZ::u64& userData) const;

        bool SetBodyUserData(
            BodyHandle bodyHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetBodyRuntimeConfiguration(
            BodyHandle bodyHandle,
            BodyRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        bool GetBodySimulationStatistics(
            BodyHandle bodyHandle,
            BodySimulationStatistics& statistics) const;

        bool ApplyBodyConfiguration(
            BodyHandle bodyHandle,
            const BodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyFaces(
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyFace> faces) const;

        [[nodiscard]]
        bool GetSoftBodyLocalBounds(
            BodyHandle bodyHandle,
            AZ::Aabb& bounds) const;

        [[nodiscard]]
        QueryResult GetSoftBodyMaterials(
            BodyHandle bodyHandle,
            AZStd::span<MaterialHandle> materials) const;

        [[nodiscard]]
        QueryResult GetSoftBodyRodStates(
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyRodState> rods) const;

        [[nodiscard]]
        bool GetSoftBodyRuntimeConfiguration(
            BodyHandle bodyHandle,
            SoftBodyRuntimeConfiguration& configuration) const;

        bool ApplySoftBodyConfiguration(
            BodyHandle bodyHandle,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyVertices(
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyVertex> vertices) const;

        [[nodiscard]]
        bool GetSoftBodyVolume(
            BodyHandle bodyHandle,
            float& volume) const;

        bool RecalculateSoftBodyMassProperties(
            BodyHandle bodyHandle,
            bool activate);

        bool SkinSoftBody(
            BodyHandle bodyHandle,
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll);

        bool UpdateSoftBodyManually(
            BodyHandle bodyHandle,
            float deltaTime);

        bool UpdateSoftBodyRuntimeConfiguration(
            BodyHandle bodyHandle,
            const SoftBodyRuntimeConfiguration& configuration);

        bool SetSoftBodyVertexInverseMass(
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            float inverseMass);

        bool SetSoftBodyVertexInverseMasses(
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses);

        bool SetSoftBodyVertexVelocity(
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity);

        bool SetSoftBodyVertexVelocities(
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities);

        [[nodiscard]]
        VirtualCharacterHandle CreateVirtualCharacter(
            const VirtualCharacterConfiguration& configuration);

        bool DestroyVirtualCharacter(VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(VirtualCharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetVirtualCharacterState(
            VirtualCharacterHandle characterHandle,
            VirtualCharacterState& state) const;

        [[nodiscard]]
        bool GetVirtualCharacterUserData(
            VirtualCharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetVirtualCharacterUserData(
            VirtualCharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetVirtualCharacterRuntimeConfiguration(
            VirtualCharacterHandle characterHandle,
            VirtualCharacterRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        QueryResult CheckVirtualCharacterCollision(
            VirtualCharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter) const;

        bool UpdateVirtualCharacterRuntimeConfiguration(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterRuntimeConfiguration& configuration);

        bool SetVirtualCharacterShape(
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetVirtualCharacterInnerBodyShape(
            VirtualCharacterHandle characterHandle,
            ShapeHandle shapeHandle);

        bool SetVirtualCharacterTransform(
            VirtualCharacterHandle characterHandle,
            const WorldTransform& transform);

        bool SetVirtualCharacterVelocity(
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        [[nodiscard]]
        bool CancelVirtualCharacterVelocityTowardsSteepSlopes(
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity,
            AZ::Vector3& adjustedVelocity) const;

        bool BeginVirtualCharacterContactTracking(VirtualCharacterHandle characterHandle);

        bool EndVirtualCharacterContactTracking(VirtualCharacterHandle characterHandle);

        bool SetVirtualCharacterContactCallbacks(
            VirtualCharacterHandle characterHandle,
            IVirtualCharacterContactCallbacks* callbacks);

        [[nodiscard]]
        bool CanVirtualCharacterWalkStairs(
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& desiredVelocity) const;

        bool WalkVirtualCharacterStairs(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter,
            DebugRenderer* debugRenderer);

        bool StickVirtualCharacterToFloor(
            VirtualCharacterHandle characterHandle,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter,
            DebugRenderer* debugRenderer);

        bool RefreshVirtualCharacterContacts(
            VirtualCharacterHandle characterHandle,
            const IQueryFilter* filter);

        bool UpdateVirtualCharacterGroundVelocity(VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        QueryResult GetVirtualCharacterContacts(
            VirtualCharacterHandle characterHandle,
            AZStd::span<VirtualCharacterContact> contacts) const;

        [[nodiscard]]
        bool HasVirtualCharacterCollidedWith(
            VirtualCharacterHandle characterHandle,
            BodyHandle bodyHandle) const;

        [[nodiscard]]
        bool HaveVirtualCharactersCollided(
            VirtualCharacterHandle firstCharacterHandle,
            VirtualCharacterHandle secondCharacterHandle) const;

        bool UpdateVirtualCharacter(
            VirtualCharacterHandle characterHandle,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration,
            DebugRenderer* debugRenderer);

        bool EnableVirtualCharacterAutoUpdate(
            VirtualCharacterHandle characterHandle,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool DisableVirtualCharacterAutoUpdate(VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        CharacterHandle CreateCharacter(const CharacterConfiguration& configuration);

        bool DestroyCharacter(CharacterHandle characterHandle);

        [[nodiscard]]
        bool IsValid(CharacterHandle characterHandle) const;

        [[nodiscard]]
        bool GetCharacterState(
            CharacterHandle characterHandle,
            CharacterState& state) const;

        [[nodiscard]]
        bool GetCharacterUserData(
            CharacterHandle characterHandle,
            AZ::u64& userData) const;

        bool SetCharacterUserData(
            CharacterHandle characterHandle,
            AZ::u64 userData);

        [[nodiscard]]
        bool GetCharacterRuntimeConfiguration(
            CharacterHandle characterHandle,
            CharacterRuntimeConfiguration& configuration) const;

        QueryResult CheckCharacterCollision(
            CharacterHandle characterHandle,
            const CharacterCollisionRequest& request,
            AZStd::span<CharacterCollisionHit> hits,
            const ICharacterCollisionFilter* filter) const;

        bool UpdateCharacterRuntimeConfiguration(
            CharacterHandle characterHandle,
            const CharacterRuntimeConfiguration& configuration);

        bool SetCharacterShape(
            CharacterHandle characterHandle,
            ShapeHandle shapeHandle,
            float maximumPenetrationDepth);

        bool SetCharacterTransform(
            CharacterHandle characterHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetCharacterVelocity(
            CharacterHandle characterHandle,
            const AZ::Vector3& velocity);

        bool AddCharacterImpulse(
            CharacterHandle characterHandle,
            const AZ::Vector3& impulse);

        bool ApplyVehicleEngineDamping(
            VehicleHandle vehicleHandle,
            float deltaTime);

        bool ApplyVehicleEngineTorque(
            VehicleHandle vehicleHandle,
            float torque,
            float deltaTime);

        [[nodiscard]]
        bool CalculateVehicleEngineTorque(
            VehicleHandle vehicleHandle,
            float acceleration,
            float& torque) const;

        [[nodiscard]]
        VehicleHandle CreateWheeledVehicle(const WheeledVehicleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateMotorcycle(const MotorcycleConfiguration& configuration);

        [[nodiscard]]
        VehicleHandle CreateTrackedVehicle(const TrackedVehicleConfiguration& configuration);

        bool DestroyVehicle(VehicleHandle vehicleHandle);

        [[nodiscard]]
        bool IsValid(VehicleHandle vehicleHandle) const;

        [[nodiscard]]
        QueryResult GetWheeledVehicleState(
            VehicleHandle vehicleHandle,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetMotorcycleState(
            VehicleHandle vehicleHandle,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        QueryResult GetTrackedVehicleState(
            VehicleHandle vehicleHandle,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels) const;

        [[nodiscard]]
        bool GetVehicleCollisionConfiguration(
            VehicleHandle vehicleHandle,
            VehicleCollisionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleDifferentialLimitedSlipRatio(
            VehicleHandle vehicleHandle,
            float& ratio) const;

        [[nodiscard]]
        bool GetVehicleEngineConfiguration(
            VehicleHandle vehicleHandle,
            VehicleEngineConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehiclePowertrainState(
            VehicleHandle vehicleHandle,
            VehiclePowertrainState& state) const;

        [[nodiscard]]
        bool GetVehicleRuntimeConfiguration(
            VehicleHandle vehicleHandle,
            VehicleRuntimeConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTransmissionConfiguration(
            VehicleHandle vehicleHandle,
            VehicleTransmissionConfiguration& configuration) const;

        [[nodiscard]]
        bool GetVehicleTrackConfiguration(
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            VehicleTrackConfiguration& configuration) const;

        [[nodiscard]]
        bool GetWheelLocalBasis(
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            WheelBasis& basis) const;

        [[nodiscard]]
        bool GetWheelLocalTransform(
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            AZ::Transform& transform) const;

        [[nodiscard]]
        bool GetWheelWorldTransform(
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const AZ::Vector3& wheelRight,
            const AZ::Vector3& wheelUp,
            WorldTransform& transform) const;

        [[nodiscard]]
        QueryResult QueryVehicleAntiRollBars(
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const;

        [[nodiscard]]
        QueryResult QueryVehicleDifferentials(
            VehicleHandle vehicleHandle,
            AZStd::span<VehicleDifferentialConfiguration> differentials) const;

        bool SetTrackedVehicleInput(
            VehicleHandle vehicleHandle,
            const TrackedVehicleInput& input);

        bool SetVehicleCallbacks(
            VehicleHandle vehicleHandle,
            IVehicleCallbacks* callbacks);

        bool SetVehicleCollisionFilter(
            VehicleHandle vehicleHandle,
            const IVehicleCollisionFilter* filter);

        bool SetVehicleDifferentialLimitedSlipRatio(
            VehicleHandle vehicleHandle,
            float ratio);

        bool SetVehiclePowertrainControl(
            VehicleHandle vehicleHandle,
            const VehiclePowertrainControl& control);

        bool SetVehicleTrackAngularVelocity(
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            float angularVelocity);

        bool SetWheelMotion(
            VehicleHandle vehicleHandle,
            AZ::u32 wheelIndex,
            const WheelMotion& motion);

        bool SetWheeledVehicleInput(
            VehicleHandle vehicleHandle,
            const WheeledVehicleInput& input);

        bool UpdateMotorcycleController(
            VehicleHandle vehicleHandle,
            const MotorcycleControllerUpdateConfiguration& configuration);

        bool UpdateVehicleAntiRollBars(
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars);

        bool UpdateVehicleCollisionConfiguration(
            VehicleHandle vehicleHandle,
            const VehicleCollisionConfiguration& configuration);

        bool UpdateVehicleDifferentials(
            VehicleHandle vehicleHandle,
            AZStd::span<const VehicleDifferentialConfiguration> differentials);

        bool UpdateVehicleEngineConfiguration(
            VehicleHandle vehicleHandle,
            const VehicleEngineConfiguration& configuration);

        bool UpdateVehicleRuntimeConfiguration(
            VehicleHandle vehicleHandle,
            const VehicleRuntimeConfiguration& configuration);

        bool UpdateVehicleTransmissionConfiguration(
            VehicleHandle vehicleHandle,
            const VehicleTransmissionConfiguration& configuration);

        bool UpdateVehicleTrackConfiguration(
            VehicleHandle vehicleHandle,
            AZ::u32 trackIndex,
            const VehicleTrackConfiguration& configuration);

        [[nodiscard]]
        BodySnapshotHandle CaptureBodyState(BodyHandle bodyHandle);

        bool CaptureBodyState(
            BodyHandle bodyHandle,
            BodySnapshotHandle snapshotHandle);

        bool DestroyBodyStateSnapshot(BodySnapshotHandle snapshotHandle);

        [[nodiscard]]
        bool IsValid(BodySnapshotHandle snapshotHandle) const;

        bool RestoreBodyState(BodySnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureState();

        bool CaptureState(StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        StateSnapshotHandle CaptureState(
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureState(
            StateSnapshotHandle snapshotHandle,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        bool CaptureStateParts(
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles,
            AZStd::span<const AZ::u32> partitionBodyCounts,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        bool ExportStateArchive(
            AZStd::span<const StateSnapshotHandle> snapshotHandles,
            StateSnapshotArchive& archive);

        bool ImportStateArchive(
            const StateSnapshotArchive& archive,
            AZStd::span<StateSnapshotHandle> snapshotHandles);

        bool DestroyStateSnapshot(StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        bool IsValid(StateSnapshotHandle snapshotHandle) const;

        bool RestoreState(StateSnapshotHandle snapshotHandle);

        bool RestoreStateParts(AZStd::span<const StateSnapshotHandle> snapshotHandles);

        bool ValidateState(
            StateSnapshotHandle snapshotHandle,
            StateValidationResult& result);

        [[nodiscard]]
        bool GetStateDigest(WorldStateDigest& digest) const;

        [[nodiscard]]
        bool GetStatistics(WorldStatistics& statistics) const;

        bool ConfigurePerformanceStatistics(PerformanceStatisticsFlags flags);

        [[nodiscard]]
        bool GetPerformanceStatistics(
            WorldPerformanceStatistics& statistics,
            bool reset);

        [[nodiscard]]
        DiagnosticStatisticsResult GetBroadPhaseStatistics(
            AZStd::span<BroadPhaseStatistics> statistics,
            bool reset);

        bool DrawDebug(
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer,
            DebugRenderer& nativeRenderer,
            const IDebugFilter* filter);

        bool ConfigureDebugCapture(const DebugCaptureConfiguration& configuration);

        [[nodiscard]]
        bool GetDebugCaptureStatistics(DebugCaptureStatistics& statistics) const;

        [[nodiscard]]
        bool IsDebugCaptureEnabled() const;

        QueryResult GetBodies(
            BodyKind kind,
            bool activeOnly,
            AZStd::span<BodyHandle> bodies) const;

        [[nodiscard]]
        bool GetBodyId(
            BodyHandle bodyHandle,
            BodyId& bodyId) const;

        bool ActivateBody(BodyHandle bodyHandle);

        bool ActivateBodies(AZStd::span<const BodyHandle> bodyHandles);

        bool ActivateBodiesInBounds(
            const BroadPhaseAabb& bounds,
            ObjectLayer collisionLayer);

        bool DeactivateBody(BodyHandle bodyHandle);

        bool DeactivateBodies(AZStd::span<const BodyHandle> bodyHandles);

        bool ResetBodySleepTimer(BodyHandle bodyHandle);

        bool InvalidateBodyContactCache(BodyHandle bodyHandle);

        void NotifyGroupFilterChanged(GroupFilterHandle filterHandle);

        [[nodiscard]]
        bool GetBodyPointVelocity(
            BodyHandle bodyHandle,
            const WorldPosition& point,
            AZ::Vector3& velocity) const;

        [[nodiscard]]
        bool GetBodyMotionType(
            BodyHandle bodyHandle,
            MotionType& motionType) const;

        [[nodiscard]]
        bool GetBodyObjectLayer(
            BodyHandle bodyHandle,
            ObjectLayer& objectLayer) const;

        [[nodiscard]]
        bool GetBodyCollisionGroup(
            BodyHandle bodyHandle,
            CollisionGroupConfiguration& collisionGroup) const;

        [[nodiscard]]
        bool GetBodyShape(
            BodyHandle bodyHandle,
            ShapeHandle& shapeHandle) const;

        [[nodiscard]]
        bool GetBodyAccumulatedForceAndTorque(
            BodyHandle bodyHandle,
            AZ::Vector3& force,
            AZ::Vector3& torque) const;

        bool ResetBodyAccumulatedForce(BodyHandle bodyHandle);

        bool ResetBodyAccumulatedTorque(BodyHandle bodyHandle);

        bool ResetBodyMotion(BodyHandle bodyHandle);

        [[nodiscard]]
        bool GetBodyBounds(
            BodyHandle bodyHandle,
            BroadPhaseAabb& bounds) const;

        [[nodiscard]]
        bool GetBodySubmergedVolume(
            BodyHandle bodyHandle,
            const WorldPosition& surfacePosition,
            const AZ::Vector3& surfaceNormal,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetBodySurfaceNormal(
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            const WorldPosition& surfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetBodyMaterial(
            BodyHandle bodyHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        [[nodiscard]]
        bool GetBodyPosition(
            BodyHandle bodyHandle,
            WorldPosition& position) const;

        [[nodiscard]]
        bool GetBodyRotation(
            BodyHandle bodyHandle,
            AZ::Quaternion& rotation) const;

        [[nodiscard]]
        bool GetBodyVelocities(
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity,
            AZ::Vector3& angularVelocity) const;

        [[nodiscard]]
        bool GetBodyLinearVelocity(
            BodyHandle bodyHandle,
            AZ::Vector3& linearVelocity) const;

        [[nodiscard]]
        bool GetBodyAngularVelocity(
            BodyHandle bodyHandle,
            AZ::Vector3& angularVelocity) const;

        bool SetBodyPosition(
            BodyHandle bodyHandle,
            const WorldPosition& position,
            bool activate);

        bool SetBodyRotation(
            BodyHandle bodyHandle,
            const AZ::Quaternion& rotation,
            bool activate);

        bool SetBodyTransform(
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyTransformWhenChanged(
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            bool activate);

        bool SetBodyVelocities(
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool SetBodyLinearVelocity(
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyAngularVelocity(
            BodyHandle bodyHandle,
            const AZ::Vector3& angularVelocity);

        bool AddBodyVelocities(
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool AddBodyLinearVelocity(
            BodyHandle bodyHandle,
            const AZ::Vector3& linearVelocity);

        bool SetBodyTransformAndVelocities(
            BodyHandle bodyHandle,
            const WorldTransform& transform,
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity);

        bool MoveBodyKinematically(
            BodyHandle bodyHandle,
            const WorldTransform& target,
            float duration);

        bool AddForce(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            bool activate);

        bool AddForceAtPosition(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const WorldPosition& position,
            bool activate);

        bool AddTorque(
            BodyHandle bodyHandle,
            const AZ::Vector3& torque,
            bool activate);

        bool AddForceAndTorque(
            BodyHandle bodyHandle,
            const AZ::Vector3& force,
            const AZ::Vector3& torque,
            bool activate);

        bool ApplyBuoyancyImpulse(
            BodyHandle bodyHandle,
            const BuoyancyConfiguration& configuration,
            DebugRenderer* debugRenderer);

        [[nodiscard]]
        bool GetBodyFriction(
            BodyHandle bodyHandle,
            float& friction) const;

        bool SetBodyFriction(
            BodyHandle bodyHandle,
            float friction);

        [[nodiscard]]
        bool GetBodyRestitution(
            BodyHandle bodyHandle,
            float& restitution) const;

        bool SetBodyRestitution(
            BodyHandle bodyHandle,
            float restitution);

        [[nodiscard]]
        bool GetBodyGravityFactor(
            BodyHandle bodyHandle,
            float& gravityFactor) const;

        bool SetBodyGravityFactor(
            BodyHandle bodyHandle,
            float gravityFactor);

        [[nodiscard]]
        bool GetBodyMaximumLinearVelocity(
            BodyHandle bodyHandle,
            float& maximumLinearVelocity) const;

        bool SetBodyMaximumLinearVelocity(
            BodyHandle bodyHandle,
            float maximumLinearVelocity);

        [[nodiscard]]
        bool GetBodyMaximumAngularVelocity(
            BodyHandle bodyHandle,
            float& maximumAngularVelocity) const;

        bool SetBodyMaximumAngularVelocity(
            BodyHandle bodyHandle,
            float maximumAngularVelocity);

        [[nodiscard]]
        bool GetBodyMotionQuality(
            BodyHandle bodyHandle,
            MotionQuality& motionQuality) const;

        bool SetBodyMotionQuality(
            BodyHandle bodyHandle,
            MotionQuality motionQuality);

        [[nodiscard]]
        bool IsBodyManifoldReductionEnabled(
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyManifoldReductionEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodySensor(
            BodyHandle bodyHandle,
            bool& sensor) const;

        bool SetBodySensor(
            BodyHandle bodyHandle,
            bool sensor);

        [[nodiscard]]
        bool GetBodyLinearDamping(
            BodyHandle bodyHandle,
            float& linearDamping) const;

        bool SetBodyLinearDamping(
            BodyHandle bodyHandle,
            float linearDamping);

        [[nodiscard]]
        bool GetBodyAngularDamping(
            BodyHandle bodyHandle,
            float& angularDamping) const;

        bool SetBodyAngularDamping(
            BodyHandle bodyHandle,
            float angularDamping);

        [[nodiscard]]
        bool IsBodySleepingAllowed(
            BodyHandle bodyHandle,
            bool& sleepingAllowed) const;

        bool SetBodySleepingAllowed(
            BodyHandle bodyHandle,
            bool sleepingAllowed);

        [[nodiscard]]
        bool IsBodyGyroscopicForceEnabled(
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyGyroscopicForceEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyKinematicVsNonDynamicCollisionEnabled(
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyKinematicVsNonDynamicCollisionEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool IsBodyEnhancedInternalEdgeRemovalEnabled(
            BodyHandle bodyHandle,
            bool& enabled) const;

        bool SetBodyEnhancedInternalEdgeRemovalEnabled(
            BodyHandle bodyHandle,
            bool enabled);

        [[nodiscard]]
        bool GetBodySolverStepCounts(
            BodyHandle bodyHandle,
            AZ::u8& velocityStepCount,
            AZ::u8& positionStepCount) const;

        bool SetBodySolverStepCounts(
            BodyHandle bodyHandle,
            AZ::u8 velocityStepCount,
            AZ::u8 positionStepCount);

        bool UpdateBodyRuntimeConfiguration(
            BodyHandle bodyHandle,
            const BodyRuntimeConfiguration& configuration,
            bool activate);

        [[nodiscard]]
        bool GetBodyInverseInertia(
            BodyHandle bodyHandle,
            AZ::Matrix3x3& inverseInertia) const;

        [[nodiscard]]
        bool GetBodyInverseMass(
            BodyHandle bodyHandle,
            float& inverseMass) const;

        bool AddImpulse(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse);

        bool AddImpulseAtPosition(
            BodyHandle bodyHandle,
            const AZ::Vector3& impulse,
            const WorldPosition& position);

        bool AddAngularImpulse(
            BodyHandle bodyHandle,
            const AZ::Vector3& angularImpulse);

        bool SetBodyShape(
            BodyHandle bodyHandle,
            ShapeHandle shapeHandle,
            bool updateMassProperties,
            bool activate);

        bool SetBodyMotionType(
            BodyHandle bodyHandle,
            MotionType motionType,
            bool activate);

        bool SetBodyObjectLayer(
            BodyHandle bodyHandle,
            ObjectLayer objectLayer);

        bool SetBodyCollisionGroup(
            BodyHandle bodyHandle,
            const CollisionGroupConfiguration& collisionGroup,
            bool activate);

        [[nodiscard]]
        bool RaycastShapeClosest(
            const ShapeRaycastRequest& request,
            ShapeRaycastHit& hit) const override;

        [[nodiscard]]
        QueryResult RaycastShapeAll(
            const ShapeRaycastRequest& request,
            AZStd::span<ShapeRaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollideShapePoint(
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter,
            AZStd::span<ShapePointHit> hits) const override;

        [[nodiscard]]
        bool CollideShapePointAny(
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            const IQueryFilter* filter = nullptr) const override;

        [[nodiscard]]
        QueryResult CollectShapeTriangles(
            const ShapeTriangleCollectionRequest& request,
            AZStd::span<ShapeTriangle> triangles) const override;

        [[nodiscard]]
        bool RaycastTransformedShapeClosest(
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            RaycastHit& hit) const override;

        [[nodiscard]]
        QueryResult RaycastTransformedShapeAll(
            const TransformedShape& shape,
            const TransformedShapeRaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollideTransformedShapePoint(
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool CollideTransformedShapePointAny(
            const TransformedShape& shape,
            const WorldPosition& position,
            const IQueryFilter* filter = nullptr) const override;

        [[nodiscard]]
        QueryResult CollectTransformedShapeChildren(
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            const IQueryFilter* filter,
            AZStd::span<TransformedShape> children) const override;

        [[nodiscard]]
        QueryResult CollectTransformedShapeTriangles(
            const TransformedShape& shape,
            const BroadPhaseAabb& bounds,
            AZStd::span<TransformedTriangle> triangles) const override;

        [[nodiscard]]
        bool GetTransformedShapeSurfaceNormal(
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const WorldPosition& position,
            AZ::Vector3& normal) const override;

        [[nodiscard]]
        QueryResult GetTransformedShapeSupportingFace(
            const TransformedShape& shape,
            SubShapeId subShapeId,
            const AZ::Vector3& direction,
            AZStd::span<WorldPosition> vertices) const override;

        [[nodiscard]]
        bool RetainShape(
            ShapeHandle shapeHandle,
            const WorldTransform& transform,
            float uniformScale,
            TransformedShape& shape) const override;

        [[nodiscard]]
        QueryResult CollideTransformedShapes(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            AZStd::span<TransformedShapeCollisionHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        bool CollideTransformedShapes(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCollisionRequest& request,
            ITransformedShapeCollisionCollector& collector) const override;

        [[nodiscard]]
        QueryResult CastTransformedShape(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            AZStd::span<TransformedShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers) const override;

        [[nodiscard]]
        bool CastTransformedShape(
            const TransformedShape& firstShape,
            const TransformedShape& secondShape,
            const TransformedShapeCastRequest& request,
            ITransformedShapeCastCollector& collector) const override;

        [[nodiscard]]
        bool RaycastClosest(
            const RaycastRequest& request,
            RaycastHit& hit) const override;

        [[nodiscard]]
        BufferResult RaycastClosestBatch(
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results) const override;

        [[nodiscard]]
        QueryResult RaycastClosestPerBody(
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        bool RaycastAny(const RaycastRequest& request) const override;

        [[nodiscard]]
        QueryResult RaycastAll(
            const RaycastRequest& request,
            AZStd::span<RaycastHit> hits) const override;

        [[nodiscard]]
        QueryResult OverlapPoint(
            const PointOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool OverlapPointAny(const PointOverlapRequest& request) const override;

        [[nodiscard]]
        QueryResult CollideShape(
            const ShapeOverlapRequest& request,
            AZStd::span<ShapeOverlapHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult OverlapShape(
            const ShapeOverlapRequest& request,
            AZStd::span<OverlapHit> hits) const override;

        [[nodiscard]]
        bool OverlapShapeAny(const ShapeOverlapRequest& request) const override;

        [[nodiscard]]
        bool CastShapeClosest(
            const ShapeCastRequest& request,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult CastShapeClosestPerBody(
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult CastShapeAll(
            const ShapeCastRequest& request,
            AZStd::span<ShapeCastHit> hits,
            const ShapeQueryFaceBuffers& faceBuffers = {}) const override;

        [[nodiscard]]
        QueryResult OverlapBroadPhase(
            const BroadPhaseOverlapRequest& request,
            AZStd::span<BroadPhaseHit> hits) const override;

        [[nodiscard]]
        bool OverlapBroadPhaseAny(const BroadPhaseOverlapRequest& request) const override;

        [[nodiscard]]
        bool CastBroadPhaseClosest(
            const BroadPhaseCastRequest& request,
            BroadPhaseCastHit& hit) const override;

        [[nodiscard]]
        QueryResult CastBroadPhaseAll(
            const BroadPhaseCastRequest& request,
            AZStd::span<BroadPhaseCastHit> hits) const override;

        [[nodiscard]]
        QueryResult CollectShapesInBounds(
            const ShapeCollectionRequest& request,
            AZStd::span<TransformedShape> shapes) const override;

        [[nodiscard]]
        QueryResult GetSupportingFace(
            const SupportingFaceRequest& request,
            AZStd::span<WorldPosition> vertices) const override;

        [[nodiscard]]
        QueryResult CollectTriangles(
            const TriangleCollectionRequest& request,
            AZStd::span<TransformedTriangle> triangles) const override;

        [[nodiscard]]
        bool GetBroadPhaseBounds(BroadPhaseAabb& bounds) const override;

        bool OptimizeBroadPhase();

        [[nodiscard]]
        bool WereBodiesInContact(
            BodyHandle firstBodyHandle,
            BodyHandle secondBodyHandle) const override;

        [[nodiscard]]
        EventView GetEvents() const;

        bool SetContactCallbacks(IContactCallbacks* callbacks);

        bool SetBodyPairCollider(IBodyPairCollider* collider);

        bool SetSimulationShapeFilter(ISimulationShapeFilter* filter);

        bool SetSoftBodyContactCallbacks(ISoftBodyContactCallbacks* callbacks);

        bool AddStepListener(IStepListener* listener);

        bool RemoveStepListener(IStepListener* listener);

        [[nodiscard]]
        HairHandle CreateHair(const HairConfiguration& configuration);

        bool DestroyHair(HairHandle hairHandle);

        [[nodiscard]]
        bool IsValid(HairHandle hairHandle) const;

        bool SetHairTransform(
            HairHandle hairHandle,
            const WorldTransform& worldTransform,
            bool teleport);

        bool SetHairScalpToHeadTransform(
            HairHandle hairHandle,
            const AZ::Transform& scalpToHeadTransform);

        bool UpdateHair(
            HairHandle hairHandle,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool EnableHairAutoUpdate(
            HairHandle hairHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool DisableHairAutoUpdate(HairHandle hairHandle);

        [[nodiscard]]
        bool GetHairState(
            HairHandle hairHandle,
            HairState& state) const;

        [[nodiscard]]
        bool GetHairReadback(
            HairHandle hairHandle,
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const;

        [[nodiscard]]
        QueryResult GetHairVertexStates(
            HairHandle hairHandle,
            AZStd::span<HairVertexState> states) const;

        [[nodiscard]]
        QueryResult GetHairRenderPositions(
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairScalpPositions(
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairGridCellStates(
            HairHandle hairHandle,
            AZStd::span<HairGridCellState> states) const;

        [[nodiscard]]
        SimulationResult StepDetailed(
            float fixedTimeStep,
            DebugRenderer* debugRenderer);

        [[nodiscard]]
        SimulationResult StepAutomaticallyDetailed(
            float elapsedTime,
            DebugRenderer* debugRenderer);

    private:
        friend void Internal::AcquireTransformedShapeLease(
            void* owner,
            ShapeHandle shapeHandle);

        friend void Internal::ReleaseTransformedShapeLease(
            void* owner,
            ShapeHandle shapeHandle);

        struct VehicleSlot;

        class CharacterCollisionCollector;
        class ClosestRaycastCollector;
        class NativeBodyPairCollisionCollector;
        class RaycastBatchJob;
        class RetainedShapeCastCollector;
        class RetainedShapeCastCallback;
        class RetainedShapeCollisionCollector;
        class RetainedShapeCollisionCallback;
        struct RaycastBatchWorkspace;
        class ShapeIdentityOverlapCollector;
        class ShapeOverlapCollector;
        class SphereIdentityOverlapCollector;
        class StandaloneShapeFilterAdapter;
        class RetainedShapePairFilterAdapter;

        class StepContext final
            : public IVehicleStepContext
        {
        public:
            explicit StepContext(World& world);

            bool AddForceAndTorque(
                BodyHandle bodyHandle,
                const AZ::Vector3& force,
                const AZ::Vector3& torque) override;

            bool AddImpulse(
                BodyHandle bodyHandle,
                const AZ::Vector3& impulse) override;

            [[nodiscard]]
            bool GetBodyState(
                BodyHandle bodyHandle,
                BodyState& state) const override;

            bool SetBodyVelocities(
                BodyHandle bodyHandle,
                const AZ::Vector3& linearVelocity,
                const AZ::Vector3& angularVelocity) override;

            bool SetConstraintEnabled(
                ConstraintHandle constraintHandle,
                bool enabled) override;

            [[nodiscard]]
            QueryResult GetMotorcycleState(
                VehicleHandle vehicleHandle,
                MotorcycleState& state,
                AZStd::span<WheelState> wheels) const override;

            [[nodiscard]]
            QueryResult GetTrackedVehicleState(
                VehicleHandle vehicleHandle,
                TrackedVehicleState& state,
                AZStd::span<WheelState> wheels) const override;

            [[nodiscard]]
            QueryResult GetWheeledVehicleState(
                VehicleHandle vehicleHandle,
                WheeledVehicleState& state,
                AZStd::span<WheelState> wheels) const override;

            bool SetTrackedVehicleInput(
                VehicleHandle vehicleHandle,
                const TrackedVehicleInput& input) override;

            bool SetVehiclePowertrainControl(
                VehicleHandle vehicleHandle,
                const VehiclePowertrainControl& control) override;

            bool SetWheelMotion(
                VehicleHandle vehicleHandle,
                AZ::u32 wheelIndex,
                const WheelMotion& motion) override;

            bool SetWheeledVehicleInput(
                VehicleHandle vehicleHandle,
                const WheeledVehicleInput& input) override;

        private:
            World& m_world;
        };

        class QueryFilterAdapter final
            : public JPH::BodyFilter
            , public JPH::ShapeFilter
        {
        public:
            QueryFilterAdapter(
                const World& world,
                const IQueryFilter* callback,
                ShapeHandle queryShapeHandle = ShapeHandle::Invalid);

            [[nodiscard]]
            bool ShouldCollide(const JPH::BodyID& bodyId) const override;

            [[nodiscard]]
            bool ShouldCollideLocked(const JPH::Body& body) const override;

            [[nodiscard]]
            bool ShouldCollide(
                const JPH::Shape* targetShape,
                const JPH::SubShapeID& targetSubShapeId) const override;

            [[nodiscard]]
            bool ShouldCollide(
                const JPH::Shape* queryShape,
                const JPH::SubShapeID& querySubShapeId,
                const JPH::Shape* targetShape,
                const JPH::SubShapeID& targetSubShapeId) const override;

        private:
            const World& m_world;
            const IQueryFilter* m_callback = nullptr;
            mutable BodyHandle m_bodyHandle;
            ShapeHandle m_queryShapeHandle;
        };

        class VehicleCollisionFilterAdapter final
            : public JPH::BodyFilter
            , public JPH::BroadPhaseLayerFilter
            , public JPH::ObjectLayerFilter
        {
        public:
            VehicleCollisionFilterAdapter(
                const World& world,
                BodyHandle vehicleBodyHandle,
                JPH::ObjectLayer collisionLayer,
                const IVehicleCollisionFilter& filter);

            [[nodiscard]]
            bool ShouldCollide(const JPH::BodyID& bodyId) const override;

            [[nodiscard]]
            bool ShouldCollide(JPH::BroadPhaseLayer broadPhaseLayer) const override;

            [[nodiscard]]
            bool ShouldCollide(JPH::ObjectLayer objectLayer) const override;

            [[nodiscard]]
            bool ShouldCollideLocked(const JPH::Body& body) const override;

        private:
            const World& m_world;
            const IVehicleCollisionFilter& m_filter;
            BodyHandle m_vehicleBodyHandle;
            JPH::ObjectLayer m_collisionLayer;
        };

        [[nodiscard]]
        VehicleHandle CreateWheeledVehicleInternal(
            const WheeledVehicleConfiguration& configuration,
            const MotorcycleControllerConfiguration* motorcycle);

        [[nodiscard]]
        QueryResult GetWheeledVehicleStateUnlocked(
            const VehicleSlot& slot,
            WheeledVehicleState& state,
            AZStd::span<WheelState> wheels,
            const JPH::BodyLockInterface& bodyLockInterface) const;

        [[nodiscard]]
        QueryResult GetMotorcycleStateUnlocked(
            const VehicleSlot& slot,
            MotorcycleState& state,
            AZStd::span<WheelState> wheels,
            const JPH::BodyLockInterface& bodyLockInterface) const;

        [[nodiscard]]
        QueryResult GetTrackedVehicleStateUnlocked(
            const VehicleSlot& slot,
            TrackedVehicleState& state,
            AZStd::span<WheelState> wheels,
            const JPH::BodyLockInterface& bodyLockInterface) const;

        [[nodiscard]]
        QueryResult CopyWheelStatesUnlocked(
            const VehicleSlot& slot,
            AZStd::span<WheelState> wheels,
            const JPH::BodyLockInterface& bodyLockInterface) const;

        [[nodiscard]]
        bool SetTrackedVehicleInputUnlocked(
            VehicleSlot& slot,
            const TrackedVehicleInput& input);

        [[nodiscard]]
        bool SetVehiclePowertrainControlUnlocked(
            VehicleSlot& slot,
            const VehiclePowertrainControl& control);

        [[nodiscard]]
        bool SetWheelMotionUnlocked(
            VehicleSlot& slot,
            AZ::u32 wheelIndex,
            const WheelMotion& motion);

        [[nodiscard]]
        bool SetWheeledVehicleInputUnlocked(
            VehicleSlot& slot,
            const WheeledVehicleInput& input);

        struct ShapeSlot final
        {
            JPH::RefConst<JPH::Shape> m_shape;
            AZStd::vector<MaterialHandle> m_materialHandles;
            AZStd::vector<ShapeHandle> m_childHandles;
            CookedShapeHandle m_cookedShapeHandle;
            AZ::u64 m_configurationRevision = 1;
            AZ::u32 m_generation = 1;
            AZ::u32 m_bodyCount = 0;
            AZ::u32 m_parentCount = 0;
            AZ::u32 m_ragdollDefinitionCount = 0;
            AZ::u32 m_transformedShapeLeaseCount = 0;
            SceneInstanceHandle m_sceneInstanceHandle;
            bool m_ownsChildHandles = false;
        };

        struct NativeShapeStatistics final
        {
            AZ::u64 m_retainedBytes = 0;
            AZ::u64 m_triangleCount = 0;
            AZ::u32 m_shapeCount = 0;
        };

        struct BodySlot final
        {
            JPH::BodyID m_bodyId;
            GroupFilterHandle m_groupFilterHandle;
            ShapeHandle m_shapeHandle;
            SoftBodyDefinitionHandle m_softBodyDefinitionHandle;
            CharacterHandle m_characterHandle;
            VirtualCharacterHandle m_virtualCharacterHandle;
            VehicleHandle m_vehicleHandle;
            RagdollHandle m_ragdollHandle;
            AZ::EntityId m_entityId;
            AZ::Name m_name;
            AZ::u64 m_userData = 0;
            AZ::u64 m_configurationRevision = 1;
            BodyKind m_kind = BodyKind::None;
            MotionType m_motionType = MotionType::None;
            bool m_softBodySkinPoseInitialized = true;
            bool m_softBodySkinConstraintsEnabled = false;
            AZ::u32 m_generation = 1;
            AZ::u32 m_constraintCount = 0;
            AZ::u32 m_moveEventIndex = AZStd::numeric_limits<AZ::u32>::max();
            SceneInstanceHandle m_sceneInstanceHandle;
        };

        struct ConstraintSlot final
        {
            JPH::Ref<JPH::Constraint> m_constraint;
            BodyHandle m_firstBodyHandle;
            BodyHandle m_secondBodyHandle;
            AZStd::array<ConstraintHandle, 2> m_dependencyHandles;
            PathHandle m_pathHandle;
            AZ::TypeId m_customProviderId = AZ::TypeId::CreateNull();
            AZ::EntityId m_entityId;
            AZ::Name m_name;
            AZ::u64 m_userData = 0;
            AZ::u64 m_configurationRevision = 1;
            PathRotationConstraint m_pathRotationConstraint = PathRotationConstraint::None;
            bool m_isInSimulation = false;
            AZ::u32 m_generation = 1;
            AZ::u32 m_parentCount = 0;
            RagdollHandle m_ragdollHandle;
            SceneInstanceHandle m_sceneInstanceHandle;
        };

        struct SceneInstanceSlot final
        {
            AZStd::vector<ShapeHandle> m_shapeHandles;
            AZStd::vector<BodyHandle> m_bodyHandles;
            AZStd::vector<ConstraintHandle> m_constraintHandles;
            SceneDefinitionHandle m_definitionHandle;
            AZ::u32 m_generation = 1;
            AZ::u32 m_rigidBodyCount = 0;
            AZ::u32 m_softBodyCount = 0;
        };

        struct RagdollDefinitionSlot final
        {
            JPH::Ref<JPH::RagdollSettings> m_settings;
            SkeletonDefinitionHandle m_skeletonHandle;
            AZStd::vector<ShapeHandle> m_shapeHandles;
            AZStd::vector<AZ::Transform> m_neutralModelTransforms;
            AZ::u32 m_generation = 1;
            AZ::u32 m_ragdollCount = 0;
            bool m_supportsMotorDrive = false;
        };

        struct RagdollSlot final
        {
            struct RemovedBodyMotionState final
            {
                JPH::Vec3 m_linearVelocity;
                JPH::Vec3 m_angularVelocity;
            };

            JPH::Ref<JPH::Ragdoll> m_ragdoll;
            AZStd::vector<BodyHandle> m_bodyHandles;
            AZStd::vector<ConstraintHandle> m_constraintHandles;
            AZStd::vector<RemovedBodyMotionState> m_removedBodyMotionStates;

            JPH::SkeletonPose m_pose;
            JPH::SkeletonPose m_previousPose;

            RagdollDefinitionHandle m_definitionHandle;
            AZ::EntityId m_entityId;
            AZ::Name m_name;
            AZ::u32 m_collisionGroupId = 0;
            AZ::u32 m_generation = 1;

            bool m_isInSimulation = false;
        };

        struct HairAutoUpdateState final
        {
            AZStd::vector<AZ::Transform> m_jointModelTransforms;
            AZ::Transform m_jointToHair = AZ::Transform::CreateIdentity();
        };

        struct HairSlot final
        {
            AZStd::unique_ptr<NativeHair> m_hair;
            AZStd::unique_ptr<HairAutoUpdateState> m_autoUpdateState;
            WorldTransform m_worldTransform;
            HairDefinitionHandle m_definitionHandle;
            AZ::u32 m_generation = 1;
            bool m_initialized = false;
            bool m_teleported = true;
        };

        struct VirtualCharacterSlot final
        {
            struct ContactProvenance final
            {
                BodyHandle m_bodyHandle;
                VirtualCharacterHandle m_characterHandle;
                AZ::u32 m_nativeId = 0;
                AZ::u32 m_subShapeId = 0;
                bool m_isCharacter = false;
            };

            JPH::Ref<JPH::CharacterVirtual> m_character;
            AZStd::unique_ptr<VirtualCharacterUpdateConfiguration> m_autoUpdateConfiguration;
            AZStd::unique_ptr<AZStd::vector<ContactProvenance>> m_contactProvenance;
            IVirtualCharacterContactCallbacks* m_contactCallbacks = nullptr;
            BodyHandle m_innerBodyHandle;
            ShapeHandle m_shapeHandle;
            AZ::EntityId m_entityId;
            AZ::Name m_name;
            AZ::u64 m_userData = 0;
            ObjectLayer m_objectLayer = ObjectLayer::Invalid;
            AZ::u32 m_contactTrackingDepth = 0;
            AZ::u32 m_generation = 1;
        };

        struct CharacterSlot final
        {
            JPH::Ref<JPH::Character> m_character;
            BodyHandle m_bodyHandle;
            ShapeHandle m_shapeHandle;
            AZ::EntityId m_entityId;
            AZ::Name m_name;
            AZ::u64 m_userData = 0;
            AZ::u64 m_configurationRevision = 1;
            float m_maximumSeparationDistance = 0.05f;
            AZ::u32 m_generation = 1;
        };

        struct VehicleBindings final
        {
            AZStd::unique_ptr<VehicleCollisionFilterAdapter> m_collisionFilterAdapter;

            IVehicleCallbacks* m_callbacks = nullptr;
            const IVehicleCollisionFilter* m_collisionFilter = nullptr;
        };

        struct VehicleSlot final
        {
            JPH::Ref<JPH::VehicleConstraint> m_constraint;
            JPH::RefConst<JPH::VehicleCollisionTester> m_collisionTester;
            AZStd::unique_ptr<VehicleBindings> m_bindings;

            BodyHandle m_bodyHandle;
            VehicleCollisionConfiguration m_collisionConfiguration;

            AZ::u64 m_configurationRevision = 1;
            float m_motorcycleMaximumLeanAngle = 0.0f;
            float m_gravityFactorBeforeOverride = 1.0f;

            VehicleKind m_kind = VehicleKind::None;
            AZ::u32 m_generation = 1;

            bool m_hasGravityFactorBeforeOverride = false;
        };

        struct RollbackParticipantState final
        {
            AZStd::vector<AZ::u8> m_data;
            AZ::TypeId m_typeId;
            AZ::u64 m_stateHash = 0;
            AZ::u32 m_version = 0;

            friend bool operator==(
                const RollbackParticipantState&,
                const RollbackParticipantState&) = default;
        };

        struct GroupFilterState final
        {
            RollbackParticipantState m_participantState;
            GroupFilterHandle m_filterHandle;
            AZ::u64 m_filterStateHash = 0;

            friend bool operator==(const GroupFilterState&, const GroupFilterState&) = default;
        };

        struct IndexedCallbackState final
        {
            RollbackParticipantState m_primaryState;
            RollbackParticipantState m_secondaryState;
            AZ::u32 m_slotIndex = 0;

            friend bool operator==(const IndexedCallbackState&, const IndexedCallbackState&) = default;
        };

        struct FilteredBodyTopologyState final
        {
            BodyHandle m_bodyHandle;
            CharacterHandle m_characterHandle;
            GroupFilterHandle m_groupFilterHandle;
            ShapeHandle m_shapeHandle;
            SoftBodyDefinitionHandle m_softBodyDefinitionHandle;

            AZ::u64 m_bodyConfigurationRevision = 0;
            AZ::u64 m_characterConfigurationRevision = 0;
            AZ::u64 m_shapeConfigurationRevision = 0;

            AZ::u32 m_collisionGroupId = 0;
            AZ::u32 m_collisionSubGroupId = 0;
            AZ::u32 m_nativeBodyId = 0;
            AZ::u32 m_objectLayer = 0;

            BodyKind m_kind = BodyKind::None;
            MotionType m_motionType = MotionType::None;
            bool m_isInSimulation = false;

            friend constexpr bool operator==(
                const FilteredBodyTopologyState&,
                const FilteredBodyTopologyState&) = default;
        };

        struct FilteredConstraintTopologyState final
        {
            AZ::u64 m_configurationRevision = 0;
            AZ::u32 m_generation = 0;
            AZ::u32 m_slotIndex = 0;
            bool m_isVehicle = false;

            friend constexpr bool operator==(
                const FilteredConstraintTopologyState&,
                const FilteredConstraintTopologyState&) = default;
        };

        struct BodySnapshotSlot final
        {
            AZStd::vector<AZ::u8> m_data;
            BodyHandle m_bodyHandle;
            AZ::u32 m_generation = 1;
        };

        struct StateSnapshotSlot final
        {
            AZStd::vector<AZ::u8> m_data;
            AZStd::vector<AZ::u32> m_bodyGenerations;
            AZStd::vector<AZ::u32> m_characterGenerations;
            AZStd::vector<AZ::u32> m_constraintGenerations;
            AZStd::vector<AZ::u32> m_vehicleGenerations;
            AZStd::vector<AZ::u32> m_virtualCharacterGenerations;
            AZStd::vector<AZ::u32> m_hairGenerations;
            AZStd::vector<NativeHair::State> m_hairStates;
            AZStd::vector<WorldTransform> m_hairWorldTransforms;
            AZStd::vector<AZ::u8> m_hairInitialized;
            AZStd::vector<AZ::u8> m_hairTeleported;
            AZStd::vector<GroupFilterState> m_groupFilterStates;
            AZStd::vector<IndexedCallbackState> m_virtualCharacterCallbackStates;
            AZStd::vector<IndexedCallbackState> m_vehicleCallbackStates;
            AZStd::vector<RollbackParticipantState> m_stepListenerStates;
            AZStd::vector<AZ::u32> m_filteredBodyIds;
            AZStd::vector<FilteredBodyTopologyState> m_filteredBodyTopologyStates;
            AZStd::vector<AZ::u64> m_filteredConstraintAddresses;
            AZStd::vector<FilteredConstraintTopologyState> m_filteredConstraintTopologyStates;
            RollbackParticipantState m_bodyPairColliderState;
            RollbackParticipantState m_contactCallbackState;
            RollbackParticipantState m_simulationShapeFilterState;
            RollbackParticipantState m_softBodyContactCallbackState;
            AZ::u32 m_filteredBodyStateCount = 0;
            AZ::u32 m_filteredConstraintStateCount = 0;

            AZStd::vector<AZ::u32> m_characterStateIndices;
            AZStd::vector<AZ::u32> m_virtualCharacterStateIndices;

            StateSnapshotConfiguration m_configuration;
            AZ::u64 m_partitionBatchId = 0;
            AZ::u64 m_configurationRevision = 0;
            AZ::u64 m_eventSequence = 0;
            AZ::u64 m_globalConfigurationRevision = 0;
            AZ::u32 m_generation = 1;
        };

        struct ContactPairHasher final
        {
            size_t operator()(const JPH::SubShapeIDPair& pair) const;
        };

        [[nodiscard]]
        const ShapeSlot* FindShape(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        ShapeSlot* FindShape(ShapeHandle shapeHandle);

        [[nodiscard]]
        ShapeHandle StoreShape(
            const JPH::Shape* shape,
            AZStd::vector<MaterialHandle> materialHandles,
            AZStd::vector<ShapeHandle> childHandles,
            CookedShapeHandle cookedShapeHandle = CookedShapeHandle::Invalid,
            bool ownsChildHandles = false);

        [[nodiscard]]
        const JPH::CompoundShape* FindCompound(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        const JPH::ConvexHullShape* FindConvexHull(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        const JPH::HeightFieldShape* FindHeightfield(ShapeHandle shapeHandle) const;

        [[nodiscard]]
        JPH::HeightFieldShape* FindHeightfield(ShapeHandle shapeHandle);

        [[nodiscard]]
        JPH::MutableCompoundShape* FindMutableCompound(ShapeHandle shapeHandle);

        [[nodiscard]]
        const JPH::MutableCompoundShape* FindMutableCompound(ShapeHandle shapeHandle) const;

        void NotifyShapeChanged(
            ShapeHandle shapeHandle,
            const JPH::Vec3& previousCenterOfMass,
            bool updateMassProperties,
            bool activateBodies);

        [[nodiscard]]
        const BodySlot* FindBody(BodyHandle bodyHandle) const;

        [[nodiscard]]
        BodySlot* FindBody(BodyHandle bodyHandle);

        BodyHandle CreateBodyUnlocked(
            BodyId requestedBodyId,
            const BodyConfiguration& configuration);

        BodyHandle CreateSoftBodyUnlocked(
            BodyId requestedBodyId,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        BodyHandle ReserveBodySlot(AZ::u32& bodyIndex);

        void ReleaseBodySlot(
            BodyHandle bodyHandle,
            BodySlot& slot);

        [[nodiscard]]
        const ConstraintSlot* FindConstraint(ConstraintHandle constraintHandle) const;

        [[nodiscard]]
        ConstraintSlot* FindConstraint(ConstraintHandle constraintHandle);

        void ReleaseConstraintReferences(ConstraintSlot& slot);

        void ReleaseConstraintSlot(
            ConstraintHandle constraintHandle,
            ConstraintSlot& slot);

        [[nodiscard]]
        const VirtualCharacterSlot* FindVirtualCharacter(VirtualCharacterHandle characterHandle) const;

        [[nodiscard]]
        VirtualCharacterSlot* FindVirtualCharacter(VirtualCharacterHandle characterHandle);

        [[nodiscard]]
        const CharacterSlot* FindCharacter(CharacterHandle characterHandle) const;

        [[nodiscard]]
        CharacterSlot* FindCharacter(CharacterHandle characterHandle);

        [[nodiscard]]
        const VehicleSlot* FindVehicle(VehicleHandle vehicleHandle) const;

        [[nodiscard]]
        VehicleSlot* FindVehicle(VehicleHandle vehicleHandle);

        void ActivateVehicleBody(const VehicleSlot& slot);

        [[nodiscard]]
        const BodySnapshotSlot* FindBodySnapshot(BodySnapshotHandle snapshotHandle) const;

        [[nodiscard]]
        BodySnapshotSlot* FindBodySnapshot(BodySnapshotHandle snapshotHandle);

        [[nodiscard]]
        const StateSnapshotSlot* FindStateSnapshot(StateSnapshotHandle snapshotHandle) const;

        [[nodiscard]]
        StateSnapshotSlot* FindStateSnapshot(StateSnapshotHandle snapshotHandle);

        [[nodiscard]]
        bool CaptureStateUnlocked(
            StateSnapshotSlot& snapshot,
            const StateSnapshotConfiguration& configuration,
            AZStd::span<const BodyHandle> bodyHandles);

        [[nodiscard]]
        bool ReserveStateSnapshotSlot(AZ::u32& snapshotIndex);

        void ReleaseStateSnapshotSlot(AZ::u32 snapshotIndex);

        bool RestoreStateUnlocked(
            const StateSnapshotSlot& snapshot,
            bool isLastPart);

        void ClearEventState();

        [[nodiscard]]
        bool GetFilteredBodyTopologyState(
            BodyHandle bodyHandle,
            FilteredBodyTopologyState& state) const;

        [[nodiscard]]
        bool ValidateSnapshotTopologyUnlocked(const StateSnapshotSlot& snapshot);

        [[nodiscard]]
        bool CaptureRollbackParticipantState(
            const IRollbackParticipant* participant,
            RollbackParticipantState& state) const;

        [[nodiscard]]
        bool IsRollbackParticipantCompatible(
            const IRollbackParticipant* participant,
            const RollbackParticipantState& state) const;

        [[nodiscard]]
        bool RestoreRollbackParticipantState(
            const IRollbackParticipant* participant,
            const RollbackParticipantState& state) const;

        [[nodiscard]]
        bool RestoreRollbackParticipants(const StateSnapshotSlot& snapshot) const;

        [[nodiscard]]
        bool AreGroupFilterStatesCompatible(
            AZStd::span<const GroupFilterState> capturedStates,
            AZStd::span<const GroupFilterState> currentStates) const;

        [[nodiscard]]
        bool RequiresFailureRecovery(const StateSnapshotSlot& snapshot) const;

        [[nodiscard]]
        bool ValidateStatePartsUnlocked(
            AZStd::span<const StateSnapshotSlot* const> snapshots);

        void WriteStateSnapshot(
            Internal::StateArchiveWriter& writer,
            const StateSnapshotSlot& snapshot) const;

        [[nodiscard]]
        bool ReadStateSnapshot(
            Internal::StateArchiveReader& reader,
            StateSnapshotSlot& snapshot);

        [[nodiscard]]
        const RagdollDefinitionSlot* FindRagdollDefinition(
            RagdollDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        RagdollDefinitionSlot* FindRagdollDefinition(RagdollDefinitionHandle definitionHandle);

        [[nodiscard]]
        const RagdollSlot* FindRagdoll(RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        RagdollSlot* FindRagdoll(RagdollHandle ragdollHandle);

        [[nodiscard]]
        const HairSlot* FindHair(HairHandle hairHandle) const;

        [[nodiscard]]
        HairSlot* FindHair(HairHandle hairHandle);

        [[nodiscard]]
        bool CollectGroupFilterStates(
            AZStd::vector<GroupFilterState>& states,
            AZStd::span<const AZ::u32> filteredBodyIds = {},
            bool filterBodies = false) const;

        [[nodiscard]]
        bool AdvanceConfigurationRevision();

        [[nodiscard]]
        bool AdvanceGlobalConfigurationRevision();

        [[nodiscard]]
        bool AdvanceBodyConfigurationRevision(BodySlot& slot);

        [[nodiscard]]
        bool AdvanceCharacterConfigurationRevision(CharacterSlot& slot);

        [[nodiscard]]
        bool AdvanceConstraintConfigurationRevision(ConstraintSlot& slot);

        [[nodiscard]]
        bool AdvanceShapeConfigurationRevision(ShapeSlot& slot);

        [[nodiscard]]
        bool AdvanceVehicleConfigurationRevision(VehicleSlot& slot);

        [[nodiscard]]
        NativeShapeStatistics GatherNativeShapeStatistics() const;

        void InvalidateAllContactCaches();

        void CollideBodies(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            const JPH::Mat44& firstCenterOfMassTransform,
            const JPH::Mat44& secondCenterOfMassTransform,
            JPH::CollideShapeSettings& settings,
            JPH::CollideShapeCollector& collector,
            const JPH::ShapeFilter& shapeFilter);

        [[nodiscard]]
        SceneInstanceSlot* FindSceneInstance(SceneInstanceHandle instanceHandle);

        [[nodiscard]]
        const SceneInstanceSlot* FindSceneInstance(SceneInstanceHandle instanceHandle) const;

        [[nodiscard]]
        bool BuildRaycastHit(
            const JPH::RRayCast& ray,
            const JPH::RayCastResult& nativeHit,
            RaycastHit& hit,
            const JPH::Body* body = nullptr) const;

        [[nodiscard]]
        bool BuildShapeRaycastHit(
            const JPH::Shape& shape,
            const ShapeRaycastRequest& request,
            const JPH::RayCastResult& nativeHit,
            ShapeRaycastHit& hit) const;

        [[nodiscard]]
        bool BuildTransformedShapeRaycastHit(
            const TransformedShape& shape,
            const JPH::RRayCast& ray,
            const JPH::RayCastResult& nativeHit,
            RaycastHit& hit) const;

        [[nodiscard]]
        const JPH::TransformedShape* GetNativeTransformedShape(
            const TransformedShape& shape) const;

        [[nodiscard]]
        bool InitializeTransformedShape(
            const JPH::TransformedShape& nativeShape,
            ShapeHandle rootShapeHandle,
            BodyHandle bodyHandle,
            const WorldPosition& worldOrigin,
            TransformedShape& shape) const;

        [[nodiscard]]
        bool BuildOverlapHit(
            const JPH::CollidePointResult& nativeHit,
            OverlapHit& hit) const;

        [[nodiscard]]
        bool ExecuteShapeOverlap(
            const ShapeOverlapRequest& request,
            const JPH::RVec3& baseOffset,
            JPH::CollideShapeCollector& collector) const;

        [[nodiscard]]
        const ShapeSlot* FindOverlapShape(
            const ShapeOverlapRequest& request) const;

        [[nodiscard]]
        const ShapeSlot* FindCastShape(
            const ShapeCastRequest& request) const;

        [[nodiscard]]
        bool RaycastClosestUnlocked(
            const RaycastRequest& request,
            RaycastHit& hit) const;

        [[nodiscard]]
        bool RaycastClosestDefaultUnlocked(
            const JPH::RRayCast& ray,
            ObjectLayer collisionLayer,
            RaycastHit& hit) const;

        [[nodiscard]]
        AZ_FORCE_INLINE bool RaycastClosestDefaultCoreUnlocked(
            const JPH::RRayCast& ray,
            ObjectLayer collisionLayer,
            RaycastHit& hit) const;

        [[nodiscard]]
        bool RaycastClosestFilteredUnlocked(
            const RaycastRequest& request,
            const JPH::RRayCast& ray,
            RaycastHit& hit) const;

        void ProcessRaycastBatchRange(
            AZStd::span<const RaycastRequest> requests,
            AZStd::span<ClosestRaycastResult> results,
            size_t beginIndex,
            size_t endIndex) const;

        [[nodiscard]]
        bool BuildShapeCastHit(
            const JPH::ShapeCastResult& nativeHit,
            const JPH::RVec3& baseOffset,
            AZ::u32 hitIndex,
            ShapeCastHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        bool BuildShapeOverlapHit(
            const JPH::CollideShapeResult& nativeHit,
            const JPH::RVec3& baseOffset,
            AZ::u32 hitIndex,
            ShapeOverlapHit& hit,
            const ShapeQueryFaceBuffers& faceBuffers) const;

        [[nodiscard]]
        bool FindBodyHandle(
            const JPH::BodyID& bodyId,
            BodyHandle& bodyHandle) const;

        [[nodiscard]]
        bool BuildVirtualCharacterContact(
            const JPH::CharacterContact& nativeContact,
            VirtualCharacterContact& contact) const;

        [[nodiscard]]
        VirtualCharacterHandle GetVirtualCharacterHandle(
            const JPH::CharacterVirtual& character) const;

        [[nodiscard]]
        VirtualCharacterSlot* FindVirtualCharacter(
            const JPH::CharacterVirtual& character,
            VirtualCharacterHandle& characterHandle);

        void RecordVirtualCharacterContactProvenance(
            VirtualCharacterSlot& slot,
            const JPH::CharacterContact& contact);

        [[nodiscard]]
        VirtualCharacterSlot::ContactProvenance TakeVirtualCharacterContactProvenance(
            VirtualCharacterSlot& slot,
            AZ::u32 nativeId,
            AZ::u32 subShapeId,
            bool isCharacter);

        bool WalkVirtualCharacterStairsUnlocked(
            VirtualCharacterSlot& slot,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter);

        bool WalkVirtualCharacterStairsWithDebugCapture(
            VirtualCharacterSlot& slot,
            const VirtualCharacterStairConfiguration& configuration,
            const IQueryFilter* filter,
            DebugRenderer* debugRenderer);

        bool StickVirtualCharacterToFloorUnlocked(
            VirtualCharacterSlot& slot,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter);

        bool StickVirtualCharacterToFloorWithDebugCapture(
            VirtualCharacterSlot& slot,
            const AZ::Vector3& stepDown,
            const IQueryFilter* filter,
            DebugRenderer* debugRenderer);

        bool UpdateVirtualCharacterUnlocked(
            VirtualCharacterSlot& slot,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration);

        bool UpdateVirtualCharacterWithDebugCapture(
            VirtualCharacterSlot& slot,
            float deltaTime,
            const VirtualCharacterUpdateConfiguration& configuration,
            DebugRenderer* debugRenderer);

        bool ApplyBuoyancyImpulseUnlocked(
            const BodySlot& slot,
            const BuoyancyConfiguration& configuration);

        bool ApplyBuoyancyImpulseWithDebugCapture(
            const BodySlot& slot,
            const BuoyancyConfiguration& configuration,
            DebugRenderer* debugRenderer);

        bool UpdateHairUnlocked(
            HairSlot& slot,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        AZ_FORCE_INLINE void MaintainBroadPhaseAfterBodyRemovals(
            const AZ::u32 bodyCount)
        {
            if (bodyCount == 0)
            {
                return;
            }

            m_bodyRemovalsSinceBroadPhaseMaintenance += bodyCount;
            if (m_bodyRemovalsSinceBroadPhaseMaintenance < m_configuration.m_capacity.m_maxBodies)
            {
                return;
            }

            m_physicsSystem.OptimizeBroadPhase();
            m_bodyRemovalsSinceBroadPhaseMaintenance = 0;
        }

        void OnBodyActivated(
            const JPH::BodyID& bodyId,
            JPH::uint64 bodyUserData) override;

        void OnBodyDeactivated(
            const JPH::BodyID& bodyId,
            JPH::uint64 bodyUserData) override;

        void OnAdjustBodyVelocity(
            const JPH::CharacterVirtual* character,
            const JPH::Body& body,
            JPH::Vec3& linearVelocity,
            JPH::Vec3& angularVelocity) override;

        [[nodiscard]]
        bool OnContactValidate(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact) override;

        void OnContactAdded(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact,
            JPH::CharacterContactSettings& settings) override;

        void OnContactPersisted(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact,
            JPH::CharacterContactSettings& settings) override;

        void OnContactRemoved(
            const JPH::CharacterVirtual* character,
            const JPH::BodyID& bodyId,
            const JPH::SubShapeID& subShapeId) override;

        void OnContactSolve(
            const JPH::CharacterVirtual* character,
            const JPH::BodyID& bodyId,
            const JPH::SubShapeID& subShapeId,
            JPH::RVec3Arg contactPosition,
            JPH::Vec3Arg contactNormal,
            JPH::Vec3Arg contactVelocity,
            const JPH::PhysicsMaterial* contactMaterial,
            JPH::Vec3Arg characterVelocity,
            JPH::Vec3& newCharacterVelocity) override;

        [[nodiscard]]
        bool OnCharacterContactValidate(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact) override;

        void OnCharacterContactAdded(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact,
            JPH::CharacterContactSettings& settings) override;

        void OnCharacterContactPersisted(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterContact& contact,
            JPH::CharacterContactSettings& settings) override;

        void OnCharacterContactRemoved(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterID& otherCharacterId,
            const JPH::SubShapeID& subShapeId) override;

        void OnCharacterContactSolve(
            const JPH::CharacterVirtual* character,
            const JPH::CharacterVirtual* otherCharacter,
            const JPH::SubShapeID& subShapeId,
            JPH::RVec3Arg contactPosition,
            JPH::Vec3Arg contactNormal,
            JPH::Vec3Arg contactVelocity,
            const JPH::PhysicsMaterial* contactMaterial,
            JPH::Vec3Arg characterVelocity,
            JPH::Vec3& newCharacterVelocity) override;

        JPH::ValidateResult OnContactValidate(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            JPH::RVec3Arg baseOffset,
            const JPH::CollideShapeResult& collision) override;

        void OnContactAdded(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings) override;

        void OnContactPersisted(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings) override;

        void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override;

        JPH::SoftBodyValidateResult OnSoftBodyContactValidate(
            const JPH::Body& softBody,
            const JPH::Body& otherBody,
            JPH::SoftBodyContactSettings& settings) override;

        void OnSoftBodyContactAdded(
            const JPH::Body& softBody,
            const JPH::SoftBodyManifold& manifold) override;

        void OnStep(const JPH::PhysicsStepListenerContext& context) override;

        [[nodiscard]]
        bool ShouldCollide(
            const JPH::Body& firstBody,
            const JPH::Shape* firstShape,
            const JPH::SubShapeID& firstSubShapeId,
            const JPH::Body& secondBody,
            const JPH::Shape* secondShape,
            const JPH::SubShapeID& secondSubShapeId) const override;

        void ProcessContact(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings,
            EventPhase phase);

        void RecordContact(
            const JPH::Body& firstBody,
            const JPH::Body& secondBody,
            const JPH::ContactManifold& manifold,
            EventPhase phase);

        void PublishEvents();

        [[nodiscard]]
        SimulationResult Update(float fixedTimeStep);

        [[nodiscard]]
        SimulationResult StepWithDebugCapture(
            float fixedTimeStep,
            DebugRenderer* debugRenderer);

        [[nodiscard]]
        SimulationResult RunAutomaticUpdates(double fixedTimeStep);

        [[nodiscard]]
        SimulationResult RunAutomaticUpdatesWithDebugCapture(
            double fixedTimeStep,
            DebugRenderer* debugRenderer);

        [[nodiscard]]
        bool IsPerformanceStatisticsEnabled(PerformanceStatisticsFlags flag) const;

        void ResetPerformanceStatistics();

        void CaptureSupplementalDebug(DebugRenderer& debugRenderer);

        void CaptureRaycastDebug(
            const RaycastRequest& request,
            const RaycastHit* hit) const;

        mutable DeterministicWorldMutex m_mutex;
        mutable AZStd::atomic_bool m_simulationInProgress{false};

        RuntimeImplementation& m_system;
        WorldConfiguration m_configuration;
        AZ::JobContext* m_jobContext = nullptr;
        WorldHandle m_handle;
        AZ::u32 m_worldIndex = 0;

        AZStd::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> m_broadPhaseLayers;
        AZStd::unique_ptr<JPH::ObjectLayerPairFilterTable> m_layerPairs;
        AZStd::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> m_layerVsBroadPhase;
        JPH::PhysicsSystem m_physicsSystem;
        AZStd::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
        AZStd::unique_ptr<JPH::JobSystem> m_jobSystem;
        AZStd::unique_ptr<DebugCapture> m_debugCapture;
        mutable AZStd::mutex m_raycastBatchWorkspaceMutex;
        mutable AZStd::vector<AZStd::unique_ptr<RaycastBatchWorkspace>> m_raycastBatchWorkspaces;
        mutable AZStd::mutex m_transformedShapeLeaseMutex;
        TransformedShapeLeaseState* m_transformedShapeLeaseState = nullptr;
        JPH::Array<JPH::Mat44> m_softBodySkinTransforms;
        JPH::Array<JPH::Mat44> m_hairJointTransforms;

        AZStd::vector<ShapeSlot> m_shapeSlots;
        AZStd::vector<AZ::u32> m_freeShapeSlots;
        mutable AZStd::vector<const JPH::Shape*> m_statisticsShapePointers;

        AZStd::vector<BodySlot> m_bodySlots;
        AZStd::vector<AZ::u32> m_freeBodySlots;
        AZ::u32 m_bodyRemovalsSinceBroadPhaseMaintenance = 0;
        AZStd::vector<BodyHandle> m_bodyMoveHandles;
        AZStd::vector<BodyHandle> m_contactCacheInvalidBodyHandles;
        JPH::Array<JPH::BodyID> m_bodyIdScratch;

        AZStd::vector<ConstraintSlot> m_constraintSlots;
        AZStd::vector<AZ::u32> m_freeConstraintSlots;
        JPH::Array<JPH::Constraint*> m_constraintScratch;

        AZStd::vector<SceneInstanceSlot> m_sceneInstanceSlots;
        AZStd::vector<AZ::u32> m_freeSceneInstanceSlots;

        AZStd::vector<RagdollDefinitionSlot> m_ragdollDefinitionSlots;
        AZStd::vector<AZ::u32> m_freeRagdollDefinitionSlots;

        AZStd::vector<RagdollSlot> m_ragdollSlots;
        AZStd::vector<AZ::u32> m_freeRagdollSlots;
        AZStd::unordered_map<AZ::u32, RagdollHandle> m_ragdollHandlesByGroupId;
        AZ::u32 m_nextRagdollGroupId = 1;

        AZStd::vector<HairSlot> m_hairSlots;
        AZStd::vector<AZ::u32> m_freeHairSlots;
        JPH::Ref<JPH::ComputeQueue> m_hairComputeQueue;

        JPH::CharacterVsCharacterCollisionSimple m_characterCollision;
        AZStd::vector<VirtualCharacterSlot> m_virtualCharacterSlots;
        AZStd::vector<AZ::u32> m_freeVirtualCharacterSlots;
        AZStd::unordered_map<AZ::u32, VirtualCharacterHandle> m_virtualCharacterHandlesById;
        AZ::u32 m_nextCharacterId = 1;

        AZStd::vector<CharacterSlot> m_characterSlots;
        AZStd::vector<AZ::u32> m_freeCharacterSlots;

        AZStd::vector<VehicleSlot> m_vehicleSlots;
        AZStd::vector<AZ::u32> m_freeVehicleSlots;

        AZStd::vector<BodySnapshotSlot> m_bodySnapshotSlots;
        AZStd::vector<AZ::u32> m_freeBodySnapshotSlots;
        AZStd::vector<AZ::u8> m_bodySnapshotScratch;

        AZStd::vector<StateSnapshotSlot> m_stateSnapshotSlots;
        AZStd::vector<AZ::u32> m_freeStateSnapshotSlots;
        StateSnapshotSlot m_stateSnapshotScratch;

        AZStd::vector<AZ::u8> m_currentHairStateScratch;
        AZStd::vector<AZ::u8> m_snapshotHairStateScratch;
        AZStd::vector<BodyHandle> m_snapshotBodyHandleScratch;
        AZStd::vector<AZ::u64> m_snapshotConstraintAddressScratch;
        AZStd::vector<FilteredConstraintTopologyState> m_snapshotConstraintTopologyScratch;
        AZStd::vector<AZ::u32> m_snapshotPartBodyIdScratch;
        AZ::u64 m_nextSnapshotPartitionBatchId = 1;

        AZStd::vector<GroupFilterState> m_groupFilterScratch;

        mutable AZStd::mutex m_eventMutex;
        AZStd::unordered_map<JPH::SubShapeIDPair, ContactEvent, ContactPairHasher> m_contactCache;
        AZStd::vector<ContactEvent> m_pendingContactEvents;
        AZStd::vector<ContactPoint> m_pendingContactPoints;
        AZStd::vector<ActivationEvent> m_pendingActivationEvents;
        AZStd::vector<BodyMoveEvent> m_pendingBodyMoveEvents;
        AZStd::vector<VirtualCharacterMoveEvent> m_pendingVirtualCharacterMoveEvents;
        AZStd::vector<ContactEvent> m_contactEvents;
        AZStd::vector<ContactPoint> m_contactPoints;
        AZStd::vector<ActivationEvent> m_activationEvents;
        AZStd::vector<BodyMoveEvent> m_bodyMoveEvents;
        AZStd::vector<VirtualCharacterMoveEvent> m_virtualCharacterMoveEvents;
        AZ::u64 m_eventSequence = 0;

        IContactCallbacks* m_contactCallbacks = nullptr;
        IBodyPairCollider* m_bodyPairCollider = nullptr;
        ISimulationShapeFilter* m_simulationShapeFilter = nullptr;
        ISoftBodyContactCallbacks* m_softBodyContactCallbacks = nullptr;
        AZStd::vector<IStepListener*> m_stepListeners;

        double m_accumulatedTime = 0.0;
        AZ::u64 m_configurationRevision = 1;
        AZ::u64 m_globalConfigurationRevision = 1;
        AZ::u64 m_lastUpdateNanoseconds = 0;
        JobSystem::UpdateStatistics m_lastUpdateJobStatistics;
        SimulationError m_lastUpdateErrors = SimulationError::None;
        mutable WorldPerformanceAccumulator m_performanceStatistics;
        AZStd::atomic_uint16_t m_performanceStatisticsFlags{0};
        AZ::u64 m_performanceStatisticsStartNanoseconds = 0;
        AZStd::atomic_bool m_dispatchingStepListeners = false;
        bool m_initialized = false;
    };
} // namespace Jolt
