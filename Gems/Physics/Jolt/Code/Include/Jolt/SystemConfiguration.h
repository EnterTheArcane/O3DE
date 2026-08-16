/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/TypeIds.h>
#include <Jolt/Collision.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    enum class MaterialCombineMode : AZ::u8
    {
        None = 0,
        Average,
        GeometricMean,
        Maximum,
        Minimum,
        Multiply,
    };

    struct SimulationConfiguration final
    {
        AZ_TYPE_INFO(SimulationConfiguration, SimulationConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr bool operator==(const SimulationConfiguration&) const = default;

        float m_baumgarte = 0.2f;
        float m_bodyPairCacheMaximumDeltaPosition = 0.001f;
        float m_bodyPairCacheMaximumDeltaRotation = 0.034906585f;
        float m_contactNormalMaximumDeltaRotation = 0.08726646f;
        float m_contactPointPreserveLambdaMaximumDistance = 0.01f;
        float m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        float m_linearCastMaximumPenetration = 0.25f;
        float m_linearCastThreshold = 0.75f;
        float m_manifoldTolerance = 1.0e-3f;
        float m_maximumPenetrationDistance = 0.2f;
        float m_minimumVelocityForRestitution = 1.0f;
        float m_penetrationSlop = 0.02f;
        float m_pointVelocitySleepThreshold = 0.03f;
        float m_speculativeContactDistance = 0.02f;
        float m_timeBeforeSleep = 0.5f;

        AZ::u32 m_maximumInFlightBodyPairs = 16'384;
        AZ::u32 m_positionStepCount = 2;
        AZ::u32 m_stepListenerBatchSize = 8;
        AZ::u32 m_stepListenerBatchesPerJob = 1;
        AZ::u32 m_velocityStepCount = 10;

        bool m_allowSleeping = true;
        bool m_checkActiveEdges = true;
        bool m_constraintWarmStart = true;
        bool m_useBodyPairContactCache = true;
        bool m_useLargeIslandSplitter = true;
        bool m_useManifoldReduction = true;
    };

    struct WorldCapacity final
    {
        AZ_TYPE_INFO(WorldCapacity, WorldCapacityTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZ::u32 m_maxBodies = 65'536;
        AZ::u32 m_bodyMutexCount = 0;
        AZ::u32 m_maxBodyPairs = 65'536;
        AZ::u32 m_maxContactConstraints = 10'240;
        AZ::u32 m_maxJobs = 2'048;
        AZ::u32 m_maxBarriers = 8;
        AZ::u64 m_tempAllocatorBytes = 16 * 1024 * 1024;
    };

    struct WorldRuntimeConfiguration final
    {
        AZ_TYPE_INFO(WorldRuntimeConfiguration, WorldRuntimeConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr bool operator==(const WorldRuntimeConfiguration&) const = default;

        MaterialCombineMode m_frictionCombineMode = MaterialCombineMode::GeometricMean;
        MaterialCombineMode m_restitutionCombineMode = MaterialCombineMode::Maximum;

        float m_fixedTimeStep = 1.0f / 60.0f;
        AZ::u32 m_collisionStepCount = 1;
        AZ::u32 m_maximumCatchUpSteps = 4;
        AZ::u32 m_workerCount = 4;

        bool m_autoSimulate = true;
        bool m_collectActivationEvents = false;
        bool m_collectContactEvents = false;
        bool m_enabled = true;
    };

    struct WorldConfiguration final
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO(WorldConfiguration, WorldConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZ::Name m_name;
        WorldPosition m_origin;
        AZ::Vector3 m_gravity = AZ::Vector3(0.0f, 0.0f, -9.81f);
        WorldCapacity m_capacity;
        SimulationConfiguration m_simulation;
        AZStd::vector<BroadPhaseLayerConfiguration> m_broadPhaseLayers = {
            {.m_name = AZ::Name::FromStringLiteral("NonMoving", nullptr)},
            {.m_name = AZ::Name::FromStringLiteral("Moving", nullptr)},
        };
        AZStd::vector<ObjectLayerConfiguration> m_objectLayers = {
            {
                .m_name = AZ::Name::FromStringLiteral("NonMoving", nullptr),
                .m_collidesWith = {DefaultLayers::Moving},
                .m_broadPhaseLayer = DefaultBroadPhaseLayers::NonMoving,
            },
            {
                .m_name = AZ::Name::FromStringLiteral("Moving", nullptr),
                .m_collidesWith = {DefaultLayers::NonMoving, DefaultLayers::Moving},
                .m_broadPhaseLayer = DefaultBroadPhaseLayers::Moving,
            },
        };

        MaterialCombineMode m_frictionCombineMode = MaterialCombineMode::GeometricMean;
        MaterialCombineMode m_restitutionCombineMode = MaterialCombineMode::Maximum;

        float m_fixedTimeStep = 1.0f / 60.0f;
        AZ::u32 m_collisionStepCount = 1;
        AZ::u32 m_maximumCatchUpSteps = 4;
        AZ::u32 m_workerCount = 4;

        bool m_autoSimulate = true;
        bool m_collectActivationEvents = false;
        bool m_collectContactEvents = false;
        bool m_enabled = true;
    };

    struct SystemConfiguration final
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO(SystemConfiguration, SystemConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        WorldConfiguration m_defaultWorld;

        float m_softBodyTriangleThickness = 0.1f;

        bool m_createDefaultWorld = true;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::MaterialCombineMode, "{9580D0CB-D536-4789-A67B-D3B8380CD5B7}");
