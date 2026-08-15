/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/BodyConfiguration.h>
#include <Jolt/BodyCollision.h>
#include <Jolt/Collision.h>
#include <Jolt/ColliderBus.h>
#include <Jolt/Constraint.h>
#include <Jolt/Cooking.h>
#include <Jolt/DebugDraw.h>
#include <Jolt/Event.h>
#include <Jolt/Handle.h>
#include <Jolt/Material.h>
#include <Jolt/Path.h>
#include <Jolt/Ragdoll.h>
#include <Jolt/Skeleton.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/SoftBody.h>
#include <Jolt/WorldTypes.h>

#include <Jolt/ConstraintComponent.h>
#include <Jolt/HairComponent.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/VehicleComponents.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/span.h>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

#include <type_traits>

static_assert(std::is_trivially_copyable_v<AZ::Vector3>);
static_assert(std::is_trivially_copyable_v<JPH::Vec3>);
static_assert(sizeof(AZ::Vector3) == sizeof(JPH::Vec3));
static_assert(alignof(AZ::Vector3) == alignof(JPH::Vec3));
static_assert(sizeof(Jolt::BodyHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::BodyId) == sizeof(AZ::u32));
static_assert(sizeof(Jolt::CollisionGroup) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::CookedShapeHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::SceneDefinitionHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::SceneInstanceHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::SkeletalAnimationHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::SkeletonPoseHandle) == sizeof(AZ::u64));
static_assert(sizeof(Jolt::CompoundChildConfiguration) == 48);
static_assert(sizeof(Jolt::CookedCompoundChildConfiguration) == 48);
static_assert(sizeof(Jolt::ColliderShapeConfiguration) <= 272);
static_assert(sizeof(Jolt::ConvexHullState) == 12);
static_assert(sizeof(Jolt::ConvexHullTopology) <= 128);
static_assert(sizeof(Jolt::CookedDecoratedShapeConfiguration) <= 80);
static_assert(sizeof(Jolt::ShapeConfiguration) <= 192);
static_assert(sizeof(Jolt::PrimitiveShapeState) <= 96);
static_assert(sizeof(Jolt::HeightfieldRegion) == 16);
static_assert(sizeof(Jolt::HeightfieldState) <= 24);
static_assert(sizeof(Jolt::HeightfieldSubShapeCoordinates) == 12);
static_assert(sizeof(Jolt::SubShapeTransform) == 64);
static_assert(sizeof(Jolt::HeightfieldUpdateConfiguration) == 8);
static_assert(sizeof(Jolt::MaterialConfiguration) <= 48);
static_assert(sizeof(Jolt::MaterialCollection) == 40);
static_assert(sizeof(Jolt::HeightfieldMaterialIndexCollection) == 40);
static_assert(sizeof(Jolt::HeightfieldSampleCollection) == 40);
static_assert(sizeof(Jolt::ShapeProperties) <= 128);
static_assert(sizeof(Jolt::ShapeStats) <= 48);
static_assert(sizeof(Jolt::DebugHairDrawSettings) <= 12);
static_assert(sizeof(Jolt::DebugCaptureConfiguration) <= 24);
static_assert(sizeof(Jolt::DebugCaptureStatistics) <= 36);
static_assert(sizeof(Jolt::DebugDrawSettings) <= 48);
static_assert(sizeof(Jolt::DebugVertex) == 36);
static_assert(sizeof(Jolt::DebugGeometryConfiguration) <= 96);
static_assert(sizeof(Jolt::WorldPosition) == sizeof(double) * 3);
static_assert(sizeof(Jolt::WorldTransform) == 48);
static_assert(sizeof(Jolt::ActivationEvent) <= 16);
static_assert(sizeof(Jolt::BodyMoveEvent) == 64);
static_assert(sizeof(Jolt::ContactEvent) <= 112);
static_assert(sizeof(Jolt::ContactPoint) == sizeof(Jolt::WorldPosition) * 2);
static_assert(sizeof(Jolt::ContactPointView) == sizeof(AZStd::span<const Jolt::ContactPoint>));
static_assert(sizeof(Jolt::BodyConfiguration) == 304);
static_assert(sizeof(Jolt::BodyState) == 176);
static_assert(sizeof(Jolt::BodyCollisionParticipant) <= 96);
static_assert(sizeof(Jolt::BodyPairCollisionHit) <= 112);
static_assert(sizeof(Jolt::BodyPairCollisionInput) <= 192);
static_assert(sizeof(Jolt::BodyPairCollisionSettings) <= 48);
static_assert(sizeof(Jolt::ConstraintConfiguration) == 672);
static_assert(sizeof(Jolt::ConstraintState) == 96);
static_assert(sizeof(Jolt::RagdollConstraintBodyPair) == 8);
static_assert(sizeof(Jolt::SkeletonMapperChainState) == 8);
static_assert(sizeof(Jolt::SkeletonMapperLockedTranslation) <= 32);
static_assert(sizeof(Jolt::SkeletonMapperMappingState) <= 112);
static_assert(sizeof(Jolt::SkeletonMapperState) <= 40);
static_assert(sizeof(Jolt::SkeletonMapperUnmappedJoint) == 8);
static_assert(sizeof(Jolt::HairDefinitionState) <= 80);
static_assert(sizeof(Jolt::HairGridCellState) == 16);
static_assert(sizeof(Jolt::HairReadbackBuffers) == sizeof(AZStd::span<AZ::u8>) * 4);
static_assert(sizeof(Jolt::HairReadbackResult) == sizeof(Jolt::QueryResult) * 4);
static_assert(sizeof(Jolt::PathSample) <= 80);
static_assert(sizeof(Jolt::PathState) <= 8);
static_assert(sizeof(Jolt::SoftBodyDefinitionState) <= 48);
static_assert(sizeof(Jolt::VehicleCollisionConfiguration) <= 48);
static_assert(sizeof(Jolt::VehiclePowertrainControl) <= 16);
static_assert(sizeof(Jolt::VehiclePowertrainState) <= 32);
static_assert(sizeof(Jolt::VehicleFrictionCalculation) == 40);
static_assert(sizeof(Jolt::VehicleTireImpulseCalculation) == 48);
static_assert(sizeof(Jolt::WheelBasis) == 48);
static_assert(sizeof(Jolt::ConstraintComponent) <= 256);
static_assert(sizeof(Jolt::HairComponent) <= 192);
static_assert(sizeof(Jolt::RagdollComponent) <= 256);
static_assert(sizeof(Jolt::SoftBodyComponent) <= 240);
static_assert(sizeof(Jolt::WheeledVehicleComponent) <= 128);
static_assert(sizeof(Jolt::MotorcycleComponent) <= 128);
static_assert(sizeof(Jolt::TrackedVehicleComponent) <= 128);
