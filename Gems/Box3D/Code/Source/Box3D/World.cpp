/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/World.h>

#include <Box3D/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>

#include <bit>
#include <cmath>
#include <cstdint>

namespace Box3D
{
    AZ_CVAR(
        bool,
        box3d_profileSimulationDatapoints,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Expose Box3D simulation counters to the profiler.");

    namespace
    {
        constexpr AZ::u64 DigestOffset = 14695981039346656037ULL;
        constexpr AZ::u64 DigestPrime = 1099511628211ULL;

        void ReportProfilerDataPoints(const WorldId worldId)
        {
#if !defined(AZ_RELEASE_BUILD)
            if (!box3d_profileSimulationDatapoints)
            {
                return;
            }

            AZ_PROFILE_SCOPE(Physics, "Box3D::World::Statistics");
            WorldStatistics statistics;
            if (!Box3D::GetStatistics(worldId, StatisticsFlags::Counters, statistics))
            {
                return;
            }

            const SimulationCounters& counters = statistics.m_counters;
            AZ_PROFILE_DATAPOINT(Physics, counters.m_bodyCount, L"Box3D/Objects/Bodies");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_awakeBodyCount, L"Box3D/Objects/AwakeBodies");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_shapeCount, L"Box3D/Objects/Shapes");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_jointCount, L"Box3D/Objects/Joints");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_islandCount, L"Box3D/Solver/Islands");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_contactCount, L"Box3D/Collisions/Contacts");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_awakeContactCount, L"Box3D/Collisions/AwakeContacts");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_taskCount, L"Box3D/Jobs/Tasks");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_stackHighWaterBytes, L"Box3D/Memory/StackHighWaterBytes");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_largestWorkerArenaPeakBytes, L"Box3D/Memory/LargestWorkerArenaPeakBytes");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_globalAllocatedBytes, L"Box3D/Memory/GlobalAllocatedBytes");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_separatingAxisCallCount, L"Box3D/Collisions/SeparatingAxisCalls");
            AZ_PROFILE_DATAPOINT(Physics, counters.m_separatingAxisCacheHitCount, L"Box3D/Collisions/SeparatingAxisCacheHits");
#else
            AZ_UNUSED(worldId);
#endif
        }

        void* EncodeSlotIndex(const AZ::u32 index)
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(index) + 1);
        }

        bool DecodeSlotIndex(void* userData, AZ::u32& index)
        {
            const uintptr_t encoded = reinterpret_cast<uintptr_t>(userData);
            if (encoded == 0 || encoded - 1 > AZStd::numeric_limits<AZ::u32>::max())
            {
                return false;
            }
            index = static_cast<AZ::u32>(encoded - 1);
            return true;
        }

        NativeBodyType ToNativeBodyType(const BodyType bodyType)
        {
            switch (bodyType)
            {
            case BodyType::Kinematic:
                return NativeBodyType::Kinematic;
            case BodyType::Dynamic:
                return NativeBodyType::Dynamic;
            case BodyType::Static:
            default:
                return NativeBodyType::Static;
            }
        }

        BodyType FromNativeBodyType(const NativeBodyType bodyType)
        {
            switch (bodyType)
            {
            case NativeBodyType::Kinematic:
                return BodyType::Kinematic;
            case NativeBodyType::Dynamic:
                return BodyType::Dynamic;
            case NativeBodyType::Static:
            default:
                return BodyType::Static;
            }
        }

        QueryBodyTypes GetQueryBodyType(const NativeBodyType bodyType)
        {
            switch (bodyType)
            {
            case NativeBodyType::Static:
                return QueryBodyTypes::Static;
            case NativeBodyType::Kinematic:
                return QueryBodyTypes::Kinematic;
            case NativeBodyType::Dynamic:
                return QueryBodyTypes::Dynamic;
            default:
                return QueryBodyTypes::None;
            }
        }

        bool IncludesBodyType(const QueryBodyTypes bodyTypes, const NativeBodyType bodyType)
        {
            return (bodyTypes & GetQueryBodyType(bodyType)) != QueryBodyTypes::None;
        }

        bool IsNormalized(const AZ::Quaternion& rotation)
        {
            return rotation.IsFinite() && AZ::IsClose(rotation.GetLengthSq(), 1.0f, 20.0f * AZ::Constants::FloatEpsilon);
        }

        bool IsValidTransform(const AZ::Transform& transform)
        {
            const float uniformScale = transform.GetUniformScale();
            return transform.GetTranslation().IsFinite() && IsNormalized(transform.GetRotation()) && AZ::IsFiniteFloat(uniformScale) &&
                uniformScale > 0.0f;
        }

        bool HasUnitScale(const AZ::Transform& transform)
        {
            return AZ::IsClose(transform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance);
        }

        bool IsValidBodyType(const BodyType bodyType)
        {
            switch (bodyType)
            {
            case BodyType::Static:
            case BodyType::Kinematic:
            case BodyType::Dynamic:
                return true;
            }
            return false;
        }

        bool IsNonNegative(const float value)
        {
            return AZ::IsFiniteFloat(value) && value >= 0.0f;
        }

        bool IsValidRigidBodyConfiguration(const RigidBodyConfiguration& configuration)
        {
            return IsValidTransform(configuration.m_transform) && HasUnitScale(configuration.m_transform) &&
                configuration.m_linearVelocity.IsFinite() && configuration.m_angularVelocity.IsFinite() &&
                IsNonNegative(configuration.m_linearDamping) && IsNonNegative(configuration.m_angularDamping) &&
                AZ::IsFiniteFloat(configuration.m_gravityScale) && IsNonNegative(configuration.m_sleepThreshold) &&
                IsValidBodyType(configuration.m_bodyType);
        }

        bool IsValidCharacterConfiguration(const CharacterConfiguration& configuration)
        {
            return configuration.m_basePosition.IsFinite() && IsNormalized(configuration.m_rotation) &&
                configuration.m_upDirection.IsFinite() && !configuration.m_upDirection.IsZero() &&
                AZ::IsFiniteFloat(configuration.m_height) && AZ::IsFiniteFloat(configuration.m_radius) && configuration.m_radius > 0.0f &&
                configuration.m_height >= 2.0f * configuration.m_radius && IsNonNegative(configuration.m_maximumSlopeAngle) &&
                configuration.m_maximumSlopeAngle <= 90.0f && IsNonNegative(configuration.m_stepHeight) &&
                IsNonNegative(configuration.m_minimumMovementDistance) && IsNonNegative(configuration.m_maximumSpeed) &&
                IsNonNegative(configuration.m_groundStickDistance) && IsNonNegative(configuration.m_interactionScale) &&
                configuration.m_maximumIterations > 0 &&
                configuration.m_maximumIterations <= CharacterConfiguration::MaximumIterationCount &&
                configuration.m_maximumContactPlanes > 0 &&
                configuration.m_maximumContactPlanes <= CharacterConfiguration::MaximumContactPlaneCount;
        }

        const JointCommonConfiguration& GetCommonJointConfiguration(const JointConfiguration& configuration);

        bool IsValidJointConfiguration(const JointConfiguration& configuration)
        {
            const JointCommonConfiguration& common = GetCommonJointConfiguration(configuration);
            if (!common.m_parentBody || !common.m_childBody || common.m_parentBody == common.m_childBody ||
                !IsValidTransform(common.m_parentLocalFrame) || !HasUnitScale(common.m_parentLocalFrame) ||
                !IsValidTransform(common.m_childLocalFrame) || !HasUnitScale(common.m_childLocalFrame) ||
                !IsNonNegative(common.m_forceThreshold) || !IsNonNegative(common.m_torqueThreshold) ||
                !IsNonNegative(common.m_constraintHertz) || !IsNonNegative(common.m_constraintDampingRatio) ||
                !IsNonNegative(common.m_drawScale))
            {
                return false;
            }

            return AZStd::visit(
                [](const auto& typedConfiguration)
                {
                    using Configuration = AZStd::remove_cvref_t<decltype(typedConfiguration)>;
                    if constexpr (AZStd::is_same_v<Configuration, ParallelJointConfiguration>)
                    {
                        return IsNonNegative(typedConfiguration.m_hertz) && IsNonNegative(typedConfiguration.m_dampingRatio) &&
                            IsNonNegative(typedConfiguration.m_maxTorque);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, DistanceJointConfiguration>)
                    {
                        return AZ::IsFiniteFloat(typedConfiguration.m_length) && typedConfiguration.m_length > 0.0f &&
                            AZ::IsFiniteFloat(typedConfiguration.m_lowerSpringForce) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperSpringForce) &&
                            typedConfiguration.m_lowerSpringForce <= typedConfiguration.m_upperSpringForce &&
                            IsNonNegative(typedConfiguration.m_hertz) && IsNonNegative(typedConfiguration.m_dampingRatio) &&
                            IsNonNegative(typedConfiguration.m_minLength) && IsNonNegative(typedConfiguration.m_maxLength) &&
                            typedConfiguration.m_minLength <= typedConfiguration.m_maxLength &&
                            IsNonNegative(typedConfiguration.m_maxMotorForce) && AZ::IsFiniteFloat(typedConfiguration.m_motorSpeed);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, FilterJointConfiguration>)
                    {
                        return true;
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, MotorJointConfiguration>)
                    {
                        return typedConfiguration.m_linearVelocity.IsFinite() && typedConfiguration.m_angularVelocity.IsFinite() &&
                            IsNonNegative(typedConfiguration.m_maxVelocityForce) && IsNonNegative(typedConfiguration.m_maxVelocityTorque) &&
                            IsNonNegative(typedConfiguration.m_linearHertz) && IsNonNegative(typedConfiguration.m_linearDampingRatio) &&
                            IsNonNegative(typedConfiguration.m_maxSpringForce) && IsNonNegative(typedConfiguration.m_angularHertz) &&
                            IsNonNegative(typedConfiguration.m_angularDampingRatio) && IsNonNegative(typedConfiguration.m_maxSpringTorque);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, PrismaticJointConfiguration>)
                    {
                        return IsNonNegative(typedConfiguration.m_hertz) && IsNonNegative(typedConfiguration.m_dampingRatio) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_targetTranslation) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_lowerTranslation) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperTranslation) &&
                            typedConfiguration.m_lowerTranslation <= typedConfiguration.m_upperTranslation &&
                            IsNonNegative(typedConfiguration.m_maxMotorForce) && AZ::IsFiniteFloat(typedConfiguration.m_motorSpeed);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, RevoluteJointConfiguration>)
                    {
                        return AZ::IsFiniteFloat(typedConfiguration.m_targetAngle) && IsNonNegative(typedConfiguration.m_hertz) &&
                            IsNonNegative(typedConfiguration.m_dampingRatio) && AZ::IsFiniteFloat(typedConfiguration.m_lowerAngle) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperAngle) &&
                            typedConfiguration.m_lowerAngle <= typedConfiguration.m_upperAngle &&
                            IsNonNegative(typedConfiguration.m_maxMotorTorque) && AZ::IsFiniteFloat(typedConfiguration.m_motorSpeed);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, SphericalJointConfiguration>)
                    {
                        return IsNormalized(typedConfiguration.m_targetRotation) && typedConfiguration.m_motorVelocity.IsFinite() &&
                            IsNonNegative(typedConfiguration.m_hertz) && IsNonNegative(typedConfiguration.m_dampingRatio) &&
                            IsNonNegative(typedConfiguration.m_coneAngle) &&
                            typedConfiguration.m_coneAngle <= SphericalJointConfiguration::MaximumConeAngle &&
                            AZ::IsFiniteFloat(typedConfiguration.m_lowerTwistAngle) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperTwistAngle) &&
                            typedConfiguration.m_lowerTwistAngle <= typedConfiguration.m_upperTwistAngle &&
                            IsNonNegative(typedConfiguration.m_maxMotorTorque);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, WeldJointConfiguration>)
                    {
                        return IsNonNegative(typedConfiguration.m_linearHertz) && IsNonNegative(typedConfiguration.m_angularHertz) &&
                            IsNonNegative(typedConfiguration.m_linearDampingRatio) &&
                            IsNonNegative(typedConfiguration.m_angularDampingRatio);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, WheelJointConfiguration>)
                    {
                        return IsNonNegative(typedConfiguration.m_suspensionHertz) &&
                            IsNonNegative(typedConfiguration.m_suspensionDampingRatio) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_lowerSuspensionLimit) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperSuspensionLimit) &&
                            typedConfiguration.m_lowerSuspensionLimit <= typedConfiguration.m_upperSuspensionLimit &&
                            IsNonNegative(typedConfiguration.m_maxSpinTorque) && AZ::IsFiniteFloat(typedConfiguration.m_spinSpeed) &&
                            IsNonNegative(typedConfiguration.m_steeringHertz) && IsNonNegative(typedConfiguration.m_steeringDampingRatio) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_targetSteeringAngle) &&
                            IsNonNegative(typedConfiguration.m_maxSteeringTorque) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_lowerSteeringLimit) &&
                            AZ::IsFiniteFloat(typedConfiguration.m_upperSteeringLimit) &&
                            typedConfiguration.m_lowerSteeringLimit <= typedConfiguration.m_upperSteeringLimit;
                    }
                },
                configuration);
        }

        void HashU32(AZ::u64& digest, const AZ::u32 value)
        {
            for (AZ::u32 byteIndex = 0; byteIndex < sizeof(value); ++byteIndex)
            {
                digest ^= static_cast<AZ::u8>(value >> (byteIndex * 8));
                digest *= DigestPrime;
            }
        }

        void HashFloat(AZ::u64& digest, const float value)
        {
            HashU32(digest, std::bit_cast<AZ::u32>(value));
        }

        void HashVector(AZ::u64& digest, const AZ::Vector3& value)
        {
            HashFloat(digest, value.GetX());
            HashFloat(digest, value.GetY());
            HashFloat(digest, value.GetZ());
        }

        void HashQuaternion(AZ::u64& digest, const AZ::Quaternion& value)
        {
            HashFloat(digest, value.GetX());
            HashFloat(digest, value.GetY());
            HashFloat(digest, value.GetZ());
            HashFloat(digest, value.GetW());
        }

        struct QueryProxy final
        {
            void AddPoint(const AZ::Vector3& point)
            {
                if (m_overflowPoints.empty() && m_inlinePoints.size() < m_inlinePoints.max_size())
                {
                    m_inlinePoints.push_back(point);
                    return;
                }
                if (m_overflowPoints.empty())
                {
                    m_overflowPoints.reserve(MaximumPointCount);
                    m_overflowPoints.assign(m_inlinePoints.begin(), m_inlinePoints.end());
                    m_inlinePoints.clear();
                }
                m_overflowPoints.push_back(point);
            }

            [[nodiscard]] AZStd::span<const AZ::Vector3> GetPoints() const
            {
                return m_overflowPoints.empty() ? AZStd::span<const AZ::Vector3>(m_inlinePoints)
                                                : AZStd::span<const AZ::Vector3>(m_overflowPoints);
            }

            static constexpr size_t MaximumPointCount = 64;
            AZStd::fixed_vector<AZ::Vector3, 8> m_inlinePoints;
            AZStd::vector<AZ::Vector3> m_overflowPoints;
            float m_radius = 0.0f;
        };

        bool BuildQueryProxy(const QueryGeometry& geometry, const AZ::Transform& transform, QueryProxy& proxy)
        {
            proxy = {};
            if (!IsValidTransform(transform))
            {
                return false;
            }
            return AZStd::visit(
                [&transform, &proxy](const auto& configuration)
                {
                    using Configuration = AZStd::remove_cvref_t<decltype(configuration)>;
                    if constexpr (AZStd::is_same_v<Configuration, SphereShapeConfiguration>)
                    {
                        proxy.m_radius = configuration.m_radius * transform.GetUniformScale();
                        proxy.AddPoint(AZ::Vector3::CreateZero());
                        return AZ::IsFiniteFloat(proxy.m_radius) && proxy.m_radius > 0.0f;
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, CapsuleShapeConfiguration>)
                    {
                        proxy.m_radius = configuration.m_radius * transform.GetUniformScale();
                        const float halfSegment = AZStd::max(0.5f * configuration.m_height - configuration.m_radius, 0.0f);
                        const AZ::Vector3 axis = transform.TransformVector(AZ::Vector3::CreateAxisZ(halfSegment));
                        proxy.AddPoint(-axis);
                        proxy.AddPoint(axis);
                        return AZ::IsFiniteFloat(configuration.m_height) && configuration.m_height > 0.0f &&
                            AZ::IsFiniteFloat(proxy.m_radius) && proxy.m_radius > 0.0f;
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, BoxShapeConfiguration>)
                    {
                        if (!configuration.m_halfExtents.IsFinite() || configuration.m_halfExtents.GetMinElement() <= 0.0f)
                        {
                            return false;
                        }
                        for (float x : { -configuration.m_halfExtents.GetX(), configuration.m_halfExtents.GetX() })
                        {
                            for (float y : { -configuration.m_halfExtents.GetY(), configuration.m_halfExtents.GetY() })
                            {
                                for (float z : { -configuration.m_halfExtents.GetZ(), configuration.m_halfExtents.GetZ() })
                                {
                                    proxy.AddPoint(transform.TransformVector(AZ::Vector3(x, y, z)));
                                }
                            }
                        }
                        return true;
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, CylinderShapeConfiguration>)
                    {
                        if (configuration.m_sideCount < 3 || configuration.m_sideCount > 32 || !AZ::IsFiniteFloat(configuration.m_height) ||
                            configuration.m_height <= 0.0f || !AZ::IsFiniteFloat(configuration.m_radius) || configuration.m_radius <= 0.0f)
                        {
                            return false;
                        }
                        const float halfHeight = 0.5f * configuration.m_height;
                        for (AZ::u32 side = 0; side < configuration.m_sideCount; ++side)
                        {
                            const float angle =
                                2.0f * AZ::Constants::Pi * aznumeric_cast<float>(side) / aznumeric_cast<float>(configuration.m_sideCount);
                            float sine = 0.0f;
                            float cosine = 0.0f;
                            AZ::SinCos(angle, sine, cosine);
                            const AZ::Vector3 radial(configuration.m_radius * cosine, configuration.m_radius * sine, 0.0f);
                            proxy.AddPoint(transform.TransformVector(radial - AZ::Vector3::CreateAxisZ(halfHeight)));
                            proxy.AddPoint(transform.TransformVector(radial + AZ::Vector3::CreateAxisZ(halfHeight)));
                        }
                        return true;
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, ConvexHullShapeConfiguration>)
                    {
                        if (configuration.m_vertices.size() < 4 || configuration.m_vertices.size() > QueryProxy::MaximumPointCount)
                        {
                            return false;
                        }
                        for (const AZ::Vector3& vertex : configuration.m_vertices)
                        {
                            if (!vertex.IsFinite())
                            {
                                return false;
                            }
                            proxy.AddPoint(transform.TransformVector(vertex));
                        }
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                },
                geometry);
        }

        const JointCommonConfiguration& GetCommonJointConfiguration(const JointConfiguration& configuration)
        {
            return AZStd::visit(
                [](const auto& typedConfiguration) -> const JointCommonConfiguration&
                {
                    return typedConfiguration.m_common;
                },
                configuration);
        }
    } // namespace

    World::World(
        System& system,
        const AZ::u32 worldIndex,
        const WorldHandle worldHandle,
        const WorldConfiguration& configuration,
        const SystemConfiguration& systemConfiguration,
        Internal::GenerationSource& bodyGenerations,
        Internal::GenerationSource& shapeGenerations,
        Internal::GenerationSource& jointGenerations,
        Internal::GenerationSource& characterGenerations)
        : m_system(system)
        , m_bodyGenerations(bodyGenerations)
        , m_shapeGenerations(shapeGenerations)
        , m_jointGenerations(jointGenerations)
        , m_characterGenerations(characterGenerations)
        , m_configuration(configuration)
        , m_systemConfiguration(systemConfiguration)
        , m_worldHandle(worldHandle)
        , m_worldIndex(worldIndex)
    {
        const DeterministicFloatScope floatScope;
        m_nativeTaskContext.m_jobContext = configuration.m_jobContext;
        m_nativeTaskContext.m_workerCount.store(systemConfiguration.m_workerCount, AZStd::memory_order_release);
        NativeWorldConfiguration nativeConfiguration;
        nativeConfiguration.m_gravity = configuration.m_gravity;
        nativeConfiguration.m_restitutionThreshold = systemConfiguration.m_restitutionThreshold;
        nativeConfiguration.m_hitEventThreshold = systemConfiguration.m_hitEventThreshold;
        nativeConfiguration.m_contactHertz = systemConfiguration.m_contactHertz;
        nativeConfiguration.m_contactDampingRatio = systemConfiguration.m_contactDampingRatio;
        nativeConfiguration.m_contactSpeed = systemConfiguration.m_contactSpeed;
        nativeConfiguration.m_contactRecycleDistance = systemConfiguration.m_contactRecycleDistance;
        nativeConfiguration.m_maximumLinearSpeed = systemConfiguration.m_maximumLinearSpeed;
        nativeConfiguration.m_workerCount = systemConfiguration.m_workerCount;
        nativeConfiguration.m_taskContext = configuration.m_jobContext != nullptr ? &m_nativeTaskContext : nullptr;
        nativeConfiguration.m_frictionCallback = systemConfiguration.m_frictionCallback;
        nativeConfiguration.m_restitutionCallback = systemConfiguration.m_restitutionCallback;
        nativeConfiguration.m_staticShapeCapacity = systemConfiguration.m_staticShapeCapacity;
        nativeConfiguration.m_dynamicShapeCapacity = systemConfiguration.m_dynamicShapeCapacity;
        nativeConfiguration.m_staticBodyCapacity = systemConfiguration.m_staticBodyCapacity;
        nativeConfiguration.m_dynamicBodyCapacity = systemConfiguration.m_dynamicBodyCapacity;
        nativeConfiguration.m_contactCapacity = systemConfiguration.m_contactCapacity;
        nativeConfiguration.m_enableSleep = systemConfiguration.m_enableSleep;
        nativeConfiguration.m_enableContinuous = systemConfiguration.m_enableContinuous;
        nativeConfiguration.m_enableWarmStarting = systemConfiguration.m_enableWarmStarting;
        nativeConfiguration.m_enableSpeculative = systemConfiguration.m_enableSpeculative;
        m_nativeId = Box3D::CreateWorld(nativeConfiguration);
    }

    World::~World()
    {
        AZStd::lock_guard lock(m_mutex);
        m_recording.reset();
        Box3D::DestroyWorld(m_nativeId);
    }

    bool World::IsValid() const
    {
        AZStd::lock_guard lock(m_mutex);
        return Box3D::IsValid(m_nativeId);
    }

    WorldHandle World::GetHandle() const
    {
        return m_worldHandle;
    }

    const AZ::Name& World::GetName() const
    {
        return m_configuration.m_name;
    }

    WorldConfiguration World::GetConfiguration() const
    {
        AZStd::lock_guard lock(m_mutex);
        return m_configuration;
    }

    AZ::Aabb World::GetAabb() const
    {
        AZStd::lock_guard lock(m_mutex);
        return Box3D::GetWorldAabb(m_nativeId);
    }

    void World::Reconfigure(const SystemConfiguration& systemConfiguration)
    {
        AZStd::lock_guard lock(m_mutex);
        m_systemConfiguration = systemConfiguration;
        NativeWorldConfiguration nativeConfiguration;
        nativeConfiguration.m_restitutionThreshold = systemConfiguration.m_restitutionThreshold;
        nativeConfiguration.m_hitEventThreshold = systemConfiguration.m_hitEventThreshold;
        nativeConfiguration.m_contactHertz = systemConfiguration.m_contactHertz;
        nativeConfiguration.m_contactDampingRatio = systemConfiguration.m_contactDampingRatio;
        nativeConfiguration.m_contactSpeed = systemConfiguration.m_contactSpeed;
        nativeConfiguration.m_contactRecycleDistance = systemConfiguration.m_contactRecycleDistance;
        nativeConfiguration.m_maximumLinearSpeed = systemConfiguration.m_maximumLinearSpeed;
        nativeConfiguration.m_workerCount = systemConfiguration.m_workerCount;
        nativeConfiguration.m_frictionCallback = systemConfiguration.m_frictionCallback;
        nativeConfiguration.m_restitutionCallback = systemConfiguration.m_restitutionCallback;
        nativeConfiguration.m_enableSleep = systemConfiguration.m_enableSleep;
        nativeConfiguration.m_enableContinuous = systemConfiguration.m_enableContinuous;
        nativeConfiguration.m_enableWarmStarting = systemConfiguration.m_enableWarmStarting;
        nativeConfiguration.m_enableSpeculative = systemConfiguration.m_enableSpeculative;
        m_nativeTaskContext.m_workerCount.store(systemConfiguration.m_workerCount, AZStd::memory_order_release);
        Box3D::ReconfigureWorld(m_nativeId, nativeConfiguration);
    }

    bool World::SetEnabled(const bool enabled)
    {
        AZStd::lock_guard lock(m_mutex);
        m_configuration.m_enabled = enabled;
        return true;
    }

    bool World::IsEnabled() const
    {
        AZStd::lock_guard lock(m_mutex);
        return m_configuration.m_enabled;
    }

    bool World::SetGravity(const AZ::Vector3& gravity)
    {
        AZStd::lock_guard lock(m_mutex);
        if (!gravity.IsFinite())
        {
            return false;
        }
        m_configuration.m_gravity = gravity;
        Box3D::SetGravity(m_nativeId, gravity);
        return true;
    }

    bool World::GetGravity(AZ::Vector3& gravity) const
    {
        AZStd::lock_guard lock(m_mutex);
        if (!Box3D::IsValid(m_nativeId))
        {
            return false;
        }
        gravity = Box3D::GetGravity(m_nativeId);
        return true;
    }

    bool World::Step(const float fixedTimeStep, const AZ::u32 subStepCount)
    {
        AZStd::lock_guard lock(m_mutex);
        if (!m_configuration.m_enabled || !AZ::IsFiniteFloat(fixedTimeStep) || fixedTimeStep <= 0.0f || subStepCount == 0)
        {
            return false;
        }

        AZ_PROFILE_SCOPE(Physics, "Box3D::World::Step");
        m_entityCharacterMoves.clear();
        if (!ApplyPendingCharacterMoves(fixedTimeStep, true))
        {
            return false;
        }
        {
            AZ_PROFILE_SCOPE(Physics, "Box3D::World::NativeStep");
            Box3D::Step(m_nativeId, fixedTimeStep, subStepCount);
        }
        ++m_lastCompletedTick;
        GatherStepEvents(false);
        ReportProfilerDataPoints(m_nativeId);
        return true;
    }

    void World::StepAutomatically(const float deltaTime, const AZ::u32 subStepCount)
    {
        AZStd::lock_guard lock(m_mutex);
        if (!m_configuration.m_autoSimulate || !m_configuration.m_enabled || !AZ::IsFiniteFloat(deltaTime) || deltaTime <= 0.0f ||
            !AZ::IsFiniteFloat(m_configuration.m_fixedTimeStep) || m_configuration.m_fixedTimeStep <= 0.0f)
        {
            return;
        }

        m_accumulatedTime += deltaTime;
        m_entityCharacterMoves.clear();
        AZ::u32 stepCount = 0;
        while (m_accumulatedTime >= m_configuration.m_fixedTimeStep && stepCount < m_configuration.m_maximumCatchUpSteps)
        {
            if (!ApplyPendingCharacterMoves(m_configuration.m_fixedTimeStep, false))
            {
                break;
            }
            {
                AZ_PROFILE_SCOPE(Physics, "Box3D::World::NativeStep");
                Box3D::Step(m_nativeId, m_configuration.m_fixedTimeStep, subStepCount);
            }
            GatherStepEvents(stepCount > 0);
            ReportProfilerDataPoints(m_nativeId);
            m_accumulatedTime -= m_configuration.m_fixedTimeStep;
            ++m_lastCompletedTick;
            ++stepCount;
        }
        if (stepCount > 0)
        {
            for (CharacterSlot& slot : m_characterSlots)
            {
                slot.m_hasPendingMove = false;
            }
        }
    }

    SimulationTick World::GetLastCompletedTick() const
    {
        AZStd::lock_guard lock(m_mutex);
        return m_lastCompletedTick;
    }

    AZ::u64 World::GetStateDigest() const
    {
        AZStd::lock_guard lock(m_mutex);
        AZ::u64 digest = DigestOffset;
        HashVector(digest, Box3D::GetGravity(m_nativeId));
        for (AZ::u32 bodyIndex = 0; bodyIndex < m_bodySlots.size(); ++bodyIndex)
        {
            const BodySlot& slot = m_bodySlots[bodyIndex];
            if (!Box3D::IsValid(slot.m_nativeId))
            {
                continue;
            }
            HashU32(digest, bodyIndex);
            HashU32(digest, slot.m_generation);
            HashU32(digest, static_cast<AZ::u32>(Box3D::GetBodyType(slot.m_nativeId)));
            const AZ::Transform transform = Box3D::GetBodyTransform(slot.m_nativeId);
            HashVector(digest, transform.GetTranslation());
            HashQuaternion(digest, transform.GetRotation());
            HashVector(digest, Box3D::GetLinearVelocity(slot.m_nativeId));
            HashVector(digest, Box3D::GetAngularVelocity(slot.m_nativeId));
            HashU32(digest, Box3D::IsAwake(slot.m_nativeId));
        }
        for (AZ::u32 characterIndex = 0; characterIndex < m_characterSlots.size(); ++characterIndex)
        {
            const CharacterSlot& slot = m_characterSlots[characterIndex];
            if (slot.m_generation == 0)
            {
                continue;
            }
            HashU32(digest, characterIndex);
            HashU32(digest, slot.m_generation);
            const CharacterState& state = m_characterResources[characterIndex].m_state;
            HashVector(digest, state.m_basePosition);
            HashVector(digest, state.m_velocity);
            HashU32(digest, static_cast<AZ::u32>(state.m_support.m_state));
            HashVector(digest, state.m_support.m_position);
            HashVector(digest, state.m_support.m_normal);
        }
        return digest;
    }

    BodyHandle World::CreateBody(const RigidBodyConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::CreateBody");
        if (!IsValidRigidBodyConfiguration(configuration))
        {
            return {};
        }

        AZ::u32 bodyIndex = 0;
        if (!m_freeBodySlots.empty())
        {
            bodyIndex = m_freeBodySlots.back();
            m_freeBodySlots.pop_back();
        }
        else
        {
            if (m_bodySlots.size() > Internal::MaximumWorldMemberIndex)
            {
                return {};
            }
            bodyIndex = aznumeric_cast<AZ::u32>(m_bodySlots.size());
            m_bodySlots.emplace_back();
            m_bodyNames.emplace_back();
        }

        BodySlot& slot = m_bodySlots[bodyIndex];
        slot.m_generation = m_bodyGenerations.Acquire();
        if (slot.m_generation == 0)
        {
            return {};
        }

        BodyConfiguration nativeConfiguration;
        nativeConfiguration.m_transform = configuration.m_transform;
        nativeConfiguration.m_linearVelocity = configuration.m_linearVelocity;
        nativeConfiguration.m_angularVelocity = configuration.m_angularVelocity;
        nativeConfiguration.m_name = configuration.m_name.GetStringView();
        nativeConfiguration.m_userData = EncodeSlotIndex(bodyIndex);
        nativeConfiguration.m_linearDamping = configuration.m_linearDamping;
        nativeConfiguration.m_angularDamping = configuration.m_angularDamping;
        nativeConfiguration.m_gravityScale = configuration.m_gravityScale;
        nativeConfiguration.m_sleepThreshold = configuration.m_sleepThreshold;
        nativeConfiguration.m_motionLocks = { configuration.m_motionLocks.m_linearX,  configuration.m_motionLocks.m_linearY,
                                              configuration.m_motionLocks.m_linearZ,  configuration.m_motionLocks.m_angularX,
                                              configuration.m_motionLocks.m_angularY, configuration.m_motionLocks.m_angularZ };
        nativeConfiguration.m_type = ToNativeBodyType(configuration.m_bodyType);
        nativeConfiguration.m_enableSleep = configuration.m_enableSleep;
        nativeConfiguration.m_isAwake = configuration.m_startAwake;
        nativeConfiguration.m_isBullet = configuration.m_isBullet;
        nativeConfiguration.m_isEnabled = configuration.m_isEnabled;
        nativeConfiguration.m_allowFastRotation = configuration.m_allowFastRotation;
        nativeConfiguration.m_enableContactRecycling = configuration.m_enableContactRecycling;
        slot.m_nativeId = Box3D::CreateBody(m_nativeId, nativeConfiguration);
        if (!Box3D::IsValid(slot.m_nativeId))
        {
            slot = BodySlot{};
            m_freeBodySlots.push_back(bodyIndex);
            return {};
        }
        slot.m_entityId = configuration.m_entityId;
        m_entityBodyCount += configuration.m_entityId.IsValid() ? 1 : 0;
        m_bodyNames[bodyIndex] = configuration.m_name;
        slot.m_firstShapeIndex = InvalidSlotIndex;
        slot.m_type = nativeConfiguration.m_type;
        slot.m_allowFastRotation = configuration.m_allowFastRotation;
        return Internal::MakeWorldMemberHandle<BodyHandle>(m_worldIndex, bodyIndex, slot.m_generation);
    }

    bool World::DestroyBody(const BodyHandle bodyHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::DestroyBody");
        Internal::WorldMemberHandleParts parts;
        BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr || !Internal::DecodeWorldMemberHandle(bodyHandle, parts))
        {
            return false;
        }

        for (AZ::u32 shapeIndex = bodySlot->m_firstShapeIndex; shapeIndex != InvalidSlotIndex;
             shapeIndex = m_shapeSlots[shapeIndex].m_nextShapeIndex)
        {
            const ShapeSlot& shapeSlot = m_shapeSlots[shapeIndex];
            UnregisterShape(shapeSlot.m_nativeId);
            m_retiredShapes.push_back(
                { shapeSlot.m_nativeId,
                  bodyHandle,
                  Internal::MakeWorldMemberHandle<ShapeHandle>(m_worldIndex, shapeIndex, shapeSlot.m_generation) });
        }
        Box3D::DestroyBody(bodySlot->m_nativeId);
        AZ::u32 shapeIndex = bodySlot->m_firstShapeIndex;
        while (shapeIndex != InvalidSlotIndex)
        {
            ShapeSlot& shapeSlot = m_shapeSlots[shapeIndex];
            const AZ::u32 nextShapeIndex = shapeSlot.m_nextShapeIndex;
            m_sensorShapeCount -= shapeSlot.m_isSensor ? 1 : 0;
            shapeSlot = ShapeSlot{};
            m_shapeResources[shapeIndex] = ShapeResources{};
            m_freeShapeSlots.push_back(shapeIndex);
            shapeIndex = nextShapeIndex;
        }
        for (AZ::u32 jointIndex = 0; jointIndex < m_jointSlots.size(); ++jointIndex)
        {
            JointSlot& jointSlot = m_jointSlots[jointIndex];
            if (jointSlot.m_generation == 0)
            {
                continue;
            }
            const JointCommonConfiguration& jointConfiguration = GetCommonJointConfiguration(m_jointConfigurations[jointIndex]);
            if (jointConfiguration.m_parentBody == bodyHandle || jointConfiguration.m_childBody == bodyHandle)
            {
                m_entityJointCount -= jointSlot.m_entityId.IsValid() ? 1 : 0;
                jointSlot = JointSlot{};
                m_jointConfigurations[jointIndex] = JointConfiguration{};
                m_freeJointSlots.push_back(jointIndex);
            }
        }
        m_entityBodyCount -= bodySlot->m_entityId.IsValid() ? 1 : 0;
        *bodySlot = BodySlot{};
        m_bodyNames[parts.m_index] = AZ::Name{};
        m_freeBodySlots.push_back(parts.m_index);
        return true;
    }

    bool World::GetBodyState(const BodyHandle bodyHandle, BodyState& state) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        Internal::WorldMemberHandleParts parts;
        if (slot == nullptr || !Internal::DecodeWorldMemberHandle(bodyHandle, parts))
        {
            return false;
        }
        state.m_transform = Box3D::GetBodyTransform(slot->m_nativeId);
        state.m_linearVelocity = Box3D::GetLinearVelocity(slot->m_nativeId);
        state.m_angularVelocity = Box3D::GetAngularVelocity(slot->m_nativeId);
        state.m_entityId = slot->m_entityId;
        state.m_name = m_bodyNames[parts.m_index];
        state.m_bodyType = FromNativeBodyType(slot->m_type);
        state.m_isAwake = Box3D::IsAwake(slot->m_nativeId);
        state.m_isEnabled = Box3D::IsBodyEnabled(slot->m_nativeId);
        state.m_allowFastRotation = slot->m_allowFastRotation;
        return true;
    }

    AZ::Name World::GetBodyName(const BodyHandle bodyHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        Internal::WorldMemberHandleParts parts;
        return slot != nullptr && Internal::DecodeWorldMemberHandle(bodyHandle, parts) ? m_bodyNames[parts.m_index] : AZ::Name{};
    }

    bool World::SetBodyName(const BodyHandle bodyHandle, AZ::Name name)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        Internal::WorldMemberHandleParts parts;
        if (slot == nullptr || !Internal::DecodeWorldMemberHandle(bodyHandle, parts))
        {
            return false;
        }
        Box3D::SetBodyName(slot->m_nativeId, name);
        m_bodyNames[parts.m_index] = name;
        return true;
    }

    bool World::GetBodyProperties(const BodyHandle bodyHandle, BodyProperties& properties) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }

        const NativeMotionLocks nativeLocks = Box3D::GetMotionLocks(slot->m_nativeId);
        properties.m_motionLocks = { nativeLocks.m_linearX,  nativeLocks.m_linearY,  nativeLocks.m_linearZ,
                                     nativeLocks.m_angularX, nativeLocks.m_angularY, nativeLocks.m_angularZ };
        properties.m_bodyType = FromNativeBodyType(slot->m_type);
        properties.m_linearDamping = Box3D::GetLinearDamping(slot->m_nativeId);
        properties.m_angularDamping = Box3D::GetAngularDamping(slot->m_nativeId);
        properties.m_gravityScale = Box3D::GetGravityScale(slot->m_nativeId);
        properties.m_sleepThreshold = Box3D::GetSleepThreshold(slot->m_nativeId);
        properties.m_isBullet = Box3D::IsBullet(slot->m_nativeId);
        properties.m_enableSleep = Box3D::IsBodySleepEnabled(slot->m_nativeId);
        properties.m_enableContactRecycling = Box3D::IsBodyContactRecyclingEnabled(slot->m_nativeId);
        return true;
    }

    bool World::SetBodyProperties(const BodyHandle bodyHandle, const BodyProperties& properties)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !IsValidBodyType(properties.m_bodyType) || !AZ::IsFiniteFloat(properties.m_linearDamping) ||
            properties.m_linearDamping < 0.0f || !AZ::IsFiniteFloat(properties.m_angularDamping) || properties.m_angularDamping < 0.0f ||
            !AZ::IsFiniteFloat(properties.m_gravityScale) || !AZ::IsFiniteFloat(properties.m_sleepThreshold) ||
            properties.m_sleepThreshold < 0.0f)
        {
            return false;
        }

        if (properties.m_bodyType != BodyType::Static)
        {
            for (AZ::u32 shapeIndex = slot->m_firstShapeIndex; shapeIndex != InvalidSlotIndex;
                 shapeIndex = m_shapeSlots[shapeIndex].m_nextShapeIndex)
            {
                NativeShapeState shapeState;
                if (!Box3D::GetShapeState(m_shapeSlots[shapeIndex].m_nativeId, shapeState) ||
                    shapeState.m_type == NativeShapeType::Compound || shapeState.m_type == NativeShapeType::Heightfield)
                {
                    return false;
                }
            }
        }

        if (!Box3D::SetBodyType(slot->m_nativeId, ToNativeBodyType(properties.m_bodyType)))
        {
            return false;
        }
        slot->m_type = ToNativeBodyType(properties.m_bodyType);
        Box3D::SetMotionLocks(
            slot->m_nativeId,
            { properties.m_motionLocks.m_linearX,
              properties.m_motionLocks.m_linearY,
              properties.m_motionLocks.m_linearZ,
              properties.m_motionLocks.m_angularX,
              properties.m_motionLocks.m_angularY,
              properties.m_motionLocks.m_angularZ });
        Box3D::SetLinearDamping(slot->m_nativeId, properties.m_linearDamping);
        Box3D::SetAngularDamping(slot->m_nativeId, properties.m_angularDamping);
        Box3D::SetGravityScale(slot->m_nativeId, properties.m_gravityScale);
        Box3D::SetSleepThreshold(slot->m_nativeId, properties.m_sleepThreshold);
        Box3D::SetBullet(slot->m_nativeId, properties.m_isBullet);
        Box3D::SetBodySleepEnabled(slot->m_nativeId, properties.m_enableSleep);
        Box3D::SetBodyContactRecyclingEnabled(slot->m_nativeId, properties.m_enableContactRecycling);
        return true;
    }

    bool World::SetBodyAwake(const BodyHandle bodyHandle, const bool awake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::SetAwake(slot->m_nativeId, awake);
        return true;
    }

    bool World::SetBodyEnabled(const BodyHandle bodyHandle, const bool enabled)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::SetBodyEnabled(slot->m_nativeId, enabled);
        return true;
    }

    bool World::SetBodyHitEventsEnabled(const BodyHandle bodyHandle, const bool enabled)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::SetBodyHitEventsEnabled(slot->m_nativeId, enabled);
        return true;
    }

    bool World::SetBodyTransform(const BodyHandle bodyHandle, const AZ::Transform& transform)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !IsValidTransform(transform) || !HasUnitScale(transform))
        {
            return false;
        }
        Box3D::SetBodyTransform(slot->m_nativeId, transform);
        return true;
    }

    bool World::GetBodyLocalPoint(const BodyHandle bodyHandle, const AZ::Vector3& worldPoint, AZ::Vector3& localPoint) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !worldPoint.IsFinite())
        {
            return false;
        }
        localPoint = Box3D::GetBodyLocalPoint(slot->m_nativeId, worldPoint);
        return true;
    }

    bool World::GetBodyWorldPoint(const BodyHandle bodyHandle, const AZ::Vector3& localPoint, AZ::Vector3& worldPoint) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !localPoint.IsFinite())
        {
            return false;
        }
        worldPoint = Box3D::GetBodyWorldPoint(slot->m_nativeId, localPoint);
        return true;
    }

    bool World::GetBodyLocalVector(const BodyHandle bodyHandle, const AZ::Vector3& worldVector, AZ::Vector3& localVector) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !worldVector.IsFinite())
        {
            return false;
        }
        localVector = Box3D::GetBodyLocalVector(slot->m_nativeId, worldVector);
        return true;
    }

    bool World::GetBodyWorldVector(const BodyHandle bodyHandle, const AZ::Vector3& localVector, AZ::Vector3& worldVector) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !localVector.IsFinite())
        {
            return false;
        }
        worldVector = Box3D::GetBodyWorldVector(slot->m_nativeId, localVector);
        return true;
    }

    bool World::SetLinearVelocity(const BodyHandle bodyHandle, const AZ::Vector3& velocity)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !velocity.IsFinite())
        {
            return false;
        }
        Box3D::SetLinearVelocity(slot->m_nativeId, velocity);
        return true;
    }

    bool World::SetAngularVelocity(const BodyHandle bodyHandle, const AZ::Vector3& velocity)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !velocity.IsFinite())
        {
            return false;
        }
        Box3D::SetAngularVelocity(slot->m_nativeId, velocity);
        return true;
    }

    AZ::Vector3 World::GetLinearVelocityAtLocalPoint(const BodyHandle bodyHandle, const AZ::Vector3& localPoint) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr && localPoint.IsFinite() ? Box3D::GetLinearVelocityAtLocalPoint(slot->m_nativeId, localPoint)
                                                        : AZ::Vector3::CreateZero();
    }

    AZ::Vector3 World::GetLinearVelocityAtWorldPoint(const BodyHandle bodyHandle, const AZ::Vector3& worldPoint) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr && worldPoint.IsFinite() ? Box3D::GetLinearVelocityAtWorldPoint(slot->m_nativeId, worldPoint)
                                                        : AZ::Vector3::CreateZero();
    }

    bool World::SetKinematicTarget(const BodyHandle bodyHandle, const AZ::Transform& transform, const float fixedTimeStep, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !IsValidTransform(transform) || !HasUnitScale(transform) || !AZ::IsFiniteFloat(fixedTimeStep) ||
            fixedTimeStep <= 0.0f)
        {
            return false;
        }
        Box3D::SetKinematicTarget(slot->m_nativeId, transform, fixedTimeStep, wake);
        return true;
    }

    bool World::ApplyLinearImpulse(const BodyHandle bodyHandle, const AZ::Vector3& impulse, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !impulse.IsFinite())
        {
            return false;
        }
        Box3D::ApplyLinearImpulse(slot->m_nativeId, impulse, wake);
        return true;
    }

    bool World::ApplyLinearImpulseAtWorldPoint(const BodyHandle bodyHandle, const AZ::Vector3& impulse, const AZ::Vector3& worldPoint, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !impulse.IsFinite() || !worldPoint.IsFinite())
        {
            return false;
        }
        Box3D::ApplyLinearImpulse(slot->m_nativeId, impulse, worldPoint, wake);
        return true;
    }

    bool World::ApplyAngularImpulse(const BodyHandle bodyHandle, const AZ::Vector3& impulse, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !impulse.IsFinite())
        {
            return false;
        }
        Box3D::ApplyAngularImpulse(slot->m_nativeId, impulse, wake);
        return true;
    }

    bool World::ApplyForce(const BodyHandle bodyHandle, const AZ::Vector3& force, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !force.IsFinite())
        {
            return false;
        }
        Box3D::ApplyForce(slot->m_nativeId, force, wake);
        return true;
    }

    bool World::ApplyForceAtWorldPoint(const BodyHandle bodyHandle, const AZ::Vector3& force, const AZ::Vector3& worldPoint, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !force.IsFinite() || !worldPoint.IsFinite())
        {
            return false;
        }
        Box3D::ApplyForce(slot->m_nativeId, force, worldPoint, wake);
        return true;
    }

    bool World::ApplyTorque(const BodyHandle bodyHandle, const AZ::Vector3& torque, const bool wake)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !torque.IsFinite())
        {
            return false;
        }
        Box3D::ApplyTorque(slot->m_nativeId, torque, wake);
        return true;
    }

    bool World::GetMassProperties(const BodyHandle bodyHandle, MassProperties& properties) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }
        const NativeMassProperties nativeProperties = Box3D::GetMassProperties(slot->m_nativeId);
        properties.m_inertia = nativeProperties.m_inertia;
        properties.m_center = nativeProperties.m_center;
        properties.m_mass = nativeProperties.m_mass;
        return true;
    }

    bool World::SetMassProperties(const BodyHandle bodyHandle, const MassProperties& properties)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr || !properties.m_inertia.IsFinite() || !properties.m_center.IsFinite() ||
            !AZ::IsFiniteFloat(properties.m_mass) || properties.m_mass < 0.0f)
        {
            return false;
        }
        Box3D::SetMassProperties(slot->m_nativeId, { properties.m_inertia, properties.m_center, properties.m_mass });
        return true;
    }

    bool World::RecomputeMassFromShapes(const BodyHandle bodyHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        BodySlot* slot = FindBodySlot(bodyHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::ApplyMassFromShapes(slot->m_nativeId);
        return true;
    }

    AZ::Matrix3x3 World::GetWorldInverseInertia(const BodyHandle bodyHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr ? Box3D::GetWorldInverseInertia(slot->m_nativeId) : AZ::Matrix3x3::CreateZero();
    }

    AZ::Vector3 World::GetWorldCenterOfMass(const BodyHandle bodyHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr ? Box3D::GetWorldCenterOfMass(slot->m_nativeId) : AZ::Vector3::CreateZero();
    }

    bool World::GetBodyClosestPoint(const BodyHandle bodyHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr && target.IsFinite() && Box3D::GetBodyClosestPoint(slot->m_nativeId, target, position, distance);
    }

    AZ::Aabb World::GetBodyAabb(const BodyHandle bodyHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* slot = FindBodySlot(bodyHandle);
        return slot != nullptr ? Box3D::GetBodyAabb(slot->m_nativeId) : AZ::Aabb::CreateNull();
    }

    BufferResult World::GetBodyShapes(const BodyHandle bodyHandle, AZStd::span<ShapeHandle> shapeHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        BufferResult result;
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr)
        {
            return result;
        }

        AZ::u32 shapeIndex = bodySlot->m_firstShapeIndex;
        while (shapeIndex != InvalidSlotIndex && shapeIndex < m_shapeSlots.size())
        {
            const ShapeSlot& shapeSlot = m_shapeSlots[shapeIndex];
            if (Box3D::IsValid(shapeSlot.m_nativeId))
            {
                if (result.m_count < shapeHandles.size())
                {
                    shapeHandles[result.m_count++] =
                        Internal::MakeWorldMemberHandle<ShapeHandle>(m_worldIndex, shapeIndex, shapeSlot.m_generation);
                }
                ++result.m_requiredCount;
            }
            shapeIndex = shapeSlot.m_nextShapeIndex;
        }
        return result;
    }

    BufferResult World::GetBodyJoints(const BodyHandle bodyHandle, AZStd::span<JointHandle> jointHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        BufferResult result;
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr || !Box3D::GetBodyJoints(bodySlot->m_nativeId, m_nativeBodyJoints))
        {
            return result;
        }

        for (JointId nativeJointId : m_nativeBodyJoints.GetJoints())
        {
            AZ::u32 jointIndex = 0;
            if (!DecodeSlotIndex(Box3D::GetJointUserData(nativeJointId), jointIndex) || jointIndex >= m_jointSlots.size())
            {
                continue;
            }
            const JointSlot& jointSlot = m_jointSlots[jointIndex];
            if (jointSlot.m_nativeId != nativeJointId)
            {
                continue;
            }
            if (result.m_count < jointHandles.size())
            {
                jointHandles[result.m_count++] =
                    Internal::MakeWorldMemberHandle<JointHandle>(m_worldIndex, jointIndex, jointSlot.m_generation);
            }
            ++result.m_requiredCount;
        }
        return result;
    }

    ContactSnapshotResult World::GetBodyContacts(
        const BodyHandle bodyHandle, const AZStd::span<ContactSnapshot> contacts, const AZStd::span<ContactPoint> points) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr ||
            !Box3D::GetTouchingContacts(bodySlot->m_nativeId, m_nativeContactScratch, m_nativeContactSnapshots, m_nativeContactPoints))
        {
            return {};
        }
        return CopyContactSnapshots(m_nativeContactSnapshots, contacts, points);
    }

    BufferResult World::GetBodySensorOverlaps(const BodyHandle bodyHandle, const AZStd::span<SensorOverlap> overlaps) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr || !Box3D::GetSensorOverlaps(bodySlot->m_nativeId, m_nativeSensorScratch, m_nativeSensorOverlaps))
        {
            return {};
        }
        return CopySensorOverlaps(m_nativeSensorOverlaps, overlaps);
    }

    bool World::RaycastBody(const BodyHandle bodyHandle, const BodyRaycastRequest& request, QueryHit& hit) const
    {
        AZStd::lock_guard lock(m_mutex);
        hit = {};
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr || !request.m_start.IsFinite() || !request.m_direction.IsFinite() || request.m_direction.IsZero() ||
            !AZ::IsFiniteFloat(request.m_distance) || request.m_distance <= 0.0f)
        {
            return false;
        }

        CastHit nativeHit;
        if (!Box3D::CastRay(
                bodySlot->m_nativeId,
                request.m_start,
                request.m_direction.GetNormalized() * request.m_distance,
                request.m_filter.m_categoryBits,
                request.m_filter.m_maskBits,
                nativeHit))
        {
            return false;
        }

        ResolvedShape resolvedShape;
        if (!ResolveShape(nativeHit.m_shapeUserData, nativeHit.m_shapeId, resolvedShape) ||
            resolvedShape.m_slot->m_bodyHandle != bodyHandle)
        {
            return false;
        }

        hit.m_bodyHandle = bodyHandle;
        hit.m_shapeHandle = resolvedShape.m_shapeHandle;
        hit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
        hit.m_position = nativeHit.m_position;
        hit.m_normal = nativeHit.m_normal;
        hit.m_fraction = nativeHit.m_fraction;
        hit.m_distance = nativeHit.m_fraction * request.m_distance;
        hit.m_faceIndex = nativeHit.m_triangleIndex;
        hit.m_childIndex = nativeHit.m_childIndex;
        return true;
    }

    bool World::ShapeCastBody(const BodyHandle bodyHandle, const BodyShapeCastRequest& request, QueryHit& hit) const
    {
        AZStd::lock_guard lock(m_mutex);
        hit = {};
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        QueryProxy proxy;
        if (bodySlot == nullptr || !request.m_translation.IsFinite() || request.m_translation.IsZero() ||
            !BuildQueryProxy(request.m_geometry, request.m_start, proxy))
        {
            return false;
        }

        CastHit nativeHit;
        if (!Box3D::CastShape(
                bodySlot->m_nativeId,
                request.m_start.GetTranslation(),
                proxy.GetPoints(),
                proxy.m_radius,
                request.m_translation,
                request.m_filter.m_categoryBits,
                request.m_filter.m_maskBits,
                nativeHit))
        {
            return false;
        }

        ResolvedShape resolvedShape;
        if (!ResolveShape(nativeHit.m_shapeUserData, nativeHit.m_shapeId, resolvedShape) ||
            resolvedShape.m_slot->m_bodyHandle != bodyHandle)
        {
            return false;
        }

        hit.m_bodyHandle = bodyHandle;
        hit.m_shapeHandle = resolvedShape.m_shapeHandle;
        hit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
        hit.m_position = nativeHit.m_position;
        hit.m_normal = nativeHit.m_normal;
        hit.m_fraction = nativeHit.m_fraction;
        hit.m_distance = nativeHit.m_fraction * request.m_translation.GetLength();
        hit.m_faceIndex = nativeHit.m_triangleIndex;
        hit.m_childIndex = nativeHit.m_childIndex;
        return true;
    }

    bool World::OverlapBody(const BodyHandle bodyHandle, const BodyOverlapRequest& request) const
    {
        AZStd::lock_guard lock(m_mutex);
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        QueryProxy proxy;
        return bodySlot != nullptr && BuildQueryProxy(request.m_geometry, request.m_transform, proxy) &&
            Box3D::OverlapShape(
                   bodySlot->m_nativeId,
                   request.m_transform.GetTranslation(),
                   proxy.GetPoints(),
                   proxy.m_radius,
                   request.m_filter.m_categoryBits,
                   request.m_filter.m_maskBits);
    }

    ShapeHandle World::CreateShape(const BodyHandle bodyHandle, const ShapeConfiguration& configuration)
    {
        return CreateShape(bodyHandle, configuration, 1.0f);
    }

    ShapeHandle World::CreateShape(const BodyHandle bodyHandle, const ShapeConfiguration& configuration, const float uniformScale)
    {
        return AttachShape(bodyHandle, configuration.m_properties, &configuration.m_geometry, nullptr, uniformScale);
    }

    ShapeHandle World::CreateShapeFromCooked(const BodyHandle bodyHandle, const NativeGeometry& geometry, const ShapeProperties& properties)
    {
        return AttachShape(bodyHandle, properties, nullptr, &geometry);
    }

    ShapeHandle World::AttachShape(
        const BodyHandle bodyHandle,
        const ShapeProperties& properties,
        const ShapeGeometry* geometry,
        const NativeGeometry* cookedGeometry,
        const float uniformScale)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::CreateShape");
        Internal::WorldMemberHandleParts bodyParts;
        BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr || !Internal::DecodeWorldMemberHandle(bodyHandle, bodyParts) ||
            (geometry == nullptr) == (cookedGeometry == nullptr) || !AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f)
        {
            return {};
        }

        AZ::u32 shapeIndex = 0;
        if (!m_freeShapeSlots.empty())
        {
            shapeIndex = m_freeShapeSlots.back();
            m_freeShapeSlots.pop_back();
        }
        else
        {
            if (m_shapeSlots.size() > Internal::MaximumWorldMemberIndex)
            {
                return {};
            }
            shapeIndex = aznumeric_cast<AZ::u32>(m_shapeSlots.size());
            m_shapeSlots.emplace_back();
            m_shapeResources.emplace_back();
        }

        ShapeSlot& shapeSlot = m_shapeSlots[shapeIndex];
        ShapeResources& resources = m_shapeResources[shapeIndex];
        resources = {};
        shapeSlot.m_generation = m_shapeGenerations.Acquire();
        if (shapeSlot.m_generation == 0)
        {
            return {};
        }
        shapeSlot.m_bodyHandle = bodyHandle;
        const ShapeHandle shapeHandle = Internal::MakeWorldMemberHandle<ShapeHandle>(m_worldIndex, shapeIndex, shapeSlot.m_generation);
        shapeSlot.m_explosionScale = properties.m_explosionScale;
        shapeSlot.m_isSensor = properties.m_isSensor;
        shapeSlot.m_enableCustomFiltering = properties.m_enableCustomFiltering;
        shapeSlot.m_nativeId =
            CreateNativeShape(shapeIndex, bodySlot->m_nativeId, shapeSlot, resources, properties, geometry, cookedGeometry, uniformScale);
        if (!Box3D::IsValid(shapeSlot.m_nativeId))
        {
            shapeSlot = ShapeSlot{};
            resources = {};
            m_freeShapeSlots.push_back(shapeIndex);
            return {};
        }
        RegisterShape(shapeSlot.m_nativeId, shapeIndex);
        shapeSlot.m_nextShapeIndex = bodySlot->m_firstShapeIndex;
        bodySlot->m_firstShapeIndex = shapeIndex;
        m_sensorShapeCount += shapeSlot.m_isSensor ? 1 : 0;
        return shapeHandle;
    }

    bool World::UpdateShape(const ShapeHandle shapeHandle, const ShapeConfiguration& configuration)
    {
        return UpdateShape(shapeHandle, configuration, 1.0f);
    }

    bool World::UpdateShape(ShapeHandle shapeHandle, const ShapeConfiguration& configuration, float uniformScale)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::UpdateShape");
        Internal::WorldMemberHandleParts shapeParts;
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, shapeParts) || !AZ::IsFiniteFloat(uniformScale) ||
            uniformScale <= 0.0f)
        {
            return false;
        }
        const BodySlot* bodySlot = FindBodySlot(shapeSlot->m_bodyHandle);
        if (bodySlot == nullptr)
        {
            return false;
        }
        ShapeResources& resources = m_shapeResources[shapeParts.m_index];

        if (shapeSlot->m_isSensor == configuration.m_properties.m_isSensor &&
            shapeSlot->m_enableCustomFiltering == configuration.m_properties.m_enableCustomFiltering &&
            shapeSlot->m_explosionScale == configuration.m_properties.m_explosionScale &&
            resources.m_materials == configuration.m_properties.m_materials)
        {
            AZStd::vector<SurfaceMaterial> nativeMaterials;
            nativeMaterials.reserve(configuration.m_properties.m_materials.size());
            float density = configuration.m_properties.m_density;
            for (MaterialHandle materialHandle : configuration.m_properties.m_materials)
            {
                MaterialConfiguration materialConfiguration;
                if (!m_system.GetMaterial(materialHandle, materialConfiguration))
                {
                    return false;
                }
                if (nativeMaterials.empty())
                {
                    density = materialConfiguration.m_density;
                }
                nativeMaterials.push_back(m_system.ResolveMaterial(materialHandle));
            }

            NativeGeometry geometry = CookGeometry(
                configuration.m_geometry, GeometryTransform{ configuration.m_properties.m_localTransform, uniformScale }, nativeMaterials);
            if (Box3D::SetShapeGeometry(shapeSlot->m_nativeId, geometry))
            {
                Box3D::SetShapeFilter(
                    shapeSlot->m_nativeId,
                    configuration.m_properties.m_collisionFilter.m_categoryBits,
                    configuration.m_properties.m_collisionFilter.m_maskBits,
                    configuration.m_properties.m_collisionFilter.m_groupIndex);
                if (!Box3D::SetShapeDensity(shapeSlot->m_nativeId, density, false) ||
                    !Box3D::SetShapeEventSubscriptions(
                        shapeSlot->m_nativeId,
                        configuration.m_properties.m_enableSensorEvents,
                        configuration.m_properties.m_enableContactEvents,
                        configuration.m_properties.m_enableHitEvents,
                        configuration.m_properties.m_enablePreSolveEvents))
                {
                    return false;
                }
                resources.m_geometry = AZStd::move(geometry);
                shapeSlot->m_isCompound = AZStd::holds_alternative<AZStd::shared_ptr<const CompoundGeometry>>(resources.m_geometry);
                if (configuration.m_properties.m_updateBodyMass)
                {
                    Box3D::ApplyMassFromShapes(bodySlot->m_nativeId);
                }
                return true;
            }
        }

        ShapeSlot replacement = *shapeSlot;
        ShapeResources replacementResources = resources;
        replacementResources.m_materials.clear();
        replacement.m_explosionScale = configuration.m_properties.m_explosionScale;
        replacement.m_isSensor = configuration.m_properties.m_isSensor;
        replacement.m_enableCustomFiltering = configuration.m_properties.m_enableCustomFiltering;
        replacement.m_nativeId = CreateNativeShape(
            shapeParts.m_index,
            bodySlot->m_nativeId,
            replacement,
            replacementResources,
            configuration.m_properties,
            &configuration.m_geometry,
            nullptr,
            uniformScale);
        if (!Box3D::IsValid(replacement.m_nativeId))
        {
            return false;
        }
        m_retiredShapes.push_back({ shapeSlot->m_nativeId, shapeSlot->m_bodyHandle, shapeHandle });
        UnregisterShape(shapeSlot->m_nativeId);
        Box3D::DestroyShape(shapeSlot->m_nativeId, false);
        if (shapeSlot->m_isSensor != replacement.m_isSensor)
        {
            if (replacement.m_isSensor)
            {
                ++m_sensorShapeCount;
            }
            else
            {
                --m_sensorShapeCount;
            }
        }
        *shapeSlot = AZStd::move(replacement);
        resources = AZStd::move(replacementResources);
        RegisterShape(shapeSlot->m_nativeId, shapeParts.m_index);
        if (configuration.m_properties.m_updateBodyMass)
        {
            Box3D::ApplyMassFromShapes(bodySlot->m_nativeId);
        }
        return true;
    }

    bool World::DestroyShape(const ShapeHandle shapeHandle, const bool updateBodyMass)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::DestroyShape");
        Internal::WorldMemberHandleParts shapeParts;
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, shapeParts))
        {
            return false;
        }
        BodySlot* bodySlot = FindBodySlot(shapeSlot->m_bodyHandle);
        if (bodySlot == nullptr)
        {
            return false;
        }

        AZ::u32* shapeLink = &bodySlot->m_firstShapeIndex;
        while (*shapeLink != InvalidSlotIndex && *shapeLink != shapeParts.m_index)
        {
            shapeLink = &m_shapeSlots[*shapeLink].m_nextShapeIndex;
        }
        if (*shapeLink == shapeParts.m_index)
        {
            *shapeLink = shapeSlot->m_nextShapeIndex;
        }
        m_retiredShapes.push_back({ shapeSlot->m_nativeId, shapeSlot->m_bodyHandle, shapeHandle });
        UnregisterShape(shapeSlot->m_nativeId);
        Box3D::DestroyShape(shapeSlot->m_nativeId, updateBodyMass);
        m_sensorShapeCount -= shapeSlot->m_isSensor ? 1 : 0;
        *shapeSlot = ShapeSlot{};
        m_shapeResources[shapeParts.m_index] = ShapeResources{};
        m_freeShapeSlots.push_back(shapeParts.m_index);
        return true;
    }

    bool World::SetShapeCollisionFilter(const ShapeHandle shapeHandle, const CollisionFilter& collisionFilter)
    {
        AZStd::lock_guard lock(m_mutex);
        ShapeSlot* slot = FindShapeSlot(shapeHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::SetShapeFilter(slot->m_nativeId, collisionFilter.m_categoryBits, collisionFilter.m_maskBits, collisionFilter.m_groupIndex);
        return true;
    }

    bool World::SetShapeMaterials(const ShapeHandle shapeHandle, AZStd::span<const MaterialHandle> materials)
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        ShapeSlot* slot = FindShapeSlot(shapeHandle);
        if (slot == nullptr || materials.empty() || !Internal::DecodeWorldMemberHandle(shapeHandle, parts))
        {
            return false;
        }
        ShapeResources& resources = m_shapeResources[parts.m_index];
        AZStd::vector<SurfaceMaterial> nativeMaterials;
        nativeMaterials.reserve(materials.size());
        float density = 0.0f;
        float explosionScale = slot->m_explosionScale;
        for (MaterialHandle materialHandle : materials)
        {
            MaterialConfiguration materialConfiguration;
            if (!m_system.GetMaterial(materialHandle, materialConfiguration))
            {
                return false;
            }
            if (nativeMaterials.empty())
            {
                density = materialConfiguration.m_density;
                explosionScale = materialConfiguration.m_explosionScale;
            }
            nativeMaterials.push_back(m_system.ResolveMaterial(materialHandle));
        }
        NativeShapeState state;
        if (!Box3D::GetShapeState(slot->m_nativeId, state))
        {
            return false;
        }
        if (state.m_type == NativeShapeType::Compound || m_recording != nullptr)
        {
            return RecreateShapeWithMaterials(shapeHandle, *slot, resources, nativeMaterials, density, explosionScale, materials);
        }
        if (!Box3D::SetShapeMaterials(slot->m_nativeId, nativeMaterials))
        {
            return false;
        }
        resources.m_materials.assign(materials.begin(), materials.end());
        slot->m_explosionScale = explosionScale;
        return true;
    }

    AZ::Aabb World::GetShapeAabb(const ShapeHandle shapeHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* slot = FindShapeSlot(shapeHandle);
        return slot != nullptr ? Box3D::GetShapeAabb(slot->m_nativeId) : AZ::Aabb::CreateNull();
    }

    bool World::GetShapeState(const ShapeHandle shapeHandle, ShapeState& state) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        NativeShapeState nativeState;
        if (shapeSlot == nullptr || !Box3D::GetShapeState(shapeSlot->m_nativeId, nativeState))
        {
            return false;
        }

        state = {};
        state.m_bodyHandle = shapeSlot->m_bodyHandle;
        state.m_collisionFilter = { nativeState.m_categoryBits, nativeState.m_maskBits, nativeState.m_groupIndex };
        switch (nativeState.m_type)
        {
        case NativeShapeType::Sphere:
            state.m_type = ShapeType::Sphere;
            break;
        case NativeShapeType::Capsule:
            state.m_type = ShapeType::Capsule;
            break;
        case NativeShapeType::Hull:
            state.m_type = ShapeType::Hull;
            break;
        case NativeShapeType::Mesh:
            state.m_type = ShapeType::Mesh;
            break;
        case NativeShapeType::Heightfield:
            state.m_type = ShapeType::Heightfield;
            break;
        case NativeShapeType::Compound:
            state.m_type = ShapeType::Compound;
            break;
        }
        state.m_density = nativeState.m_density;
        state.m_friction = nativeState.m_friction;
        state.m_restitution = nativeState.m_restitution;
        state.m_isSensor = nativeState.m_isSensor;
        state.m_enableSensorEvents = nativeState.m_enableSensorEvents;
        state.m_enableContactEvents = nativeState.m_enableContactEvents;
        state.m_enableHitEvents = nativeState.m_enableHitEvents;
        state.m_enablePreSolveEvents = nativeState.m_enablePreSolveEvents;
        return true;
    }

    BufferResult World::GetShapeMaterials(const ShapeHandle shapeHandle, AZStd::span<MaterialHandle> materialHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, parts))
        {
            return {};
        }

        const AZStd::vector<MaterialHandle>& materials = m_shapeResources[parts.m_index].m_materials;
        const size_t materialCount = AZStd::min(materials.size(), materialHandles.size());
        for (size_t materialIndex = 0; materialIndex < materialCount; ++materialIndex)
        {
            materialHandles[materialIndex] = materials[materialIndex];
        }
        return { materialCount, materials.size() };
    }

    bool World::SetShapeDensity(const ShapeHandle shapeHandle, const float density, const bool updateBodyMass)
    {
        AZStd::lock_guard lock(m_mutex);
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        return shapeSlot != nullptr && Box3D::SetShapeDensity(shapeSlot->m_nativeId, density, updateBodyMass);
    }

    bool World::SetShapeFriction(const ShapeHandle shapeHandle, const float friction)
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, parts) || !AZ::IsFiniteFloat(friction) ||
            friction < 0.0f)
        {
            return false;
        }
        NativeShapeState state;
        if (!Box3D::GetShapeState(shapeSlot->m_nativeId, state))
        {
            return false;
        }
        if (state.m_type != NativeShapeType::Compound)
        {
            return Box3D::SetShapeFriction(shapeSlot->m_nativeId, friction);
        }
        AZStd::vector<SurfaceMaterial> materials;
        ShapeResources& resources = m_shapeResources[parts.m_index];
        if (!GetCompoundSourceMaterials(*shapeSlot, resources, materials))
        {
            return false;
        }
        for (SurfaceMaterial& material : materials)
        {
            material.m_friction = friction;
        }
        return RecreateShapeWithMaterials(
            shapeHandle, *shapeSlot, resources, materials, state.m_density, shapeSlot->m_explosionScale, resources.m_materials);
    }

    bool World::SetShapeRestitution(const ShapeHandle shapeHandle, const float restitution)
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, parts) || !AZ::IsFiniteFloat(restitution) ||
            restitution < 0.0f)
        {
            return false;
        }
        NativeShapeState state;
        if (!Box3D::GetShapeState(shapeSlot->m_nativeId, state))
        {
            return false;
        }
        if (state.m_type != NativeShapeType::Compound)
        {
            return Box3D::SetShapeRestitution(shapeSlot->m_nativeId, restitution);
        }
        AZStd::vector<SurfaceMaterial> materials;
        ShapeResources& resources = m_shapeResources[parts.m_index];
        if (!GetCompoundSourceMaterials(*shapeSlot, resources, materials))
        {
            return false;
        }
        for (SurfaceMaterial& material : materials)
        {
            material.m_restitution = restitution;
        }
        return RecreateShapeWithMaterials(
            shapeHandle, *shapeSlot, resources, materials, state.m_density, shapeSlot->m_explosionScale, resources.m_materials);
    }

    bool World::SetShapeEventSubscriptions(
        const ShapeHandle shapeHandle, const bool sensorEvents, const bool contactEvents, const bool hitEvents, const bool preSolveEvents)
    {
        AZStd::lock_guard lock(m_mutex);
        ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        return shapeSlot != nullptr &&
            Box3D::SetShapeEventSubscriptions(shapeSlot->m_nativeId, sensorEvents, contactEvents, hitEvents, preSolveEvents);
    }

    bool World::GetShapeMassProperties(const ShapeHandle shapeHandle, MassProperties& properties) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr)
        {
            return false;
        }
        const NativeMassProperties nativeProperties = Box3D::GetShapeMassProperties(shapeSlot->m_nativeId);
        properties = { nativeProperties.m_inertia, nativeProperties.m_center, nativeProperties.m_mass };
        return true;
    }

    bool World::GetShapeClosestPoint(const ShapeHandle shapeHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Box3D::GetShapeClosestPoint(shapeSlot->m_nativeId, target, position))
        {
            return false;
        }
        distance = position.GetDistance(target);
        return true;
    }

    bool World::RaycastShape(
        const ShapeHandle shapeHandle, const AZ::Vector3& start, const AZ::Vector3& direction, const float distance, QueryHit& hit) const
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Internal::DecodeWorldMemberHandle(shapeHandle, parts) || !start.IsFinite() || !direction.IsFinite() ||
            direction.IsZero() || !AZ::IsFiniteFloat(distance) || distance <= 0.0f)
        {
            return false;
        }

        GeometryCastHit nativeHit;
        if (!Box3D::CastRay(shapeSlot->m_nativeId, start, direction.GetNormalized() * distance, nativeHit))
        {
            return false;
        }

        hit = {};
        hit.m_bodyHandle = shapeSlot->m_bodyHandle;
        hit.m_shapeHandle = shapeHandle;
        hit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
        const AZStd::vector<MaterialHandle>& materials = m_shapeResources[parts.m_index].m_materials;
        if (!hit.m_materialHandle.IsValid() && nativeHit.m_materialIndex >= 0 &&
            aznumeric_cast<size_t>(nativeHit.m_materialIndex) < materials.size())
        {
            hit.m_materialHandle = materials[aznumeric_cast<size_t>(nativeHit.m_materialIndex)];
        }
        else if (materials.size() == 1)
        {
            hit.m_materialHandle = materials.front();
        }
        hit.m_position = nativeHit.m_position;
        hit.m_normal = nativeHit.m_normal;
        hit.m_fraction = nativeHit.m_fraction;
        hit.m_distance = nativeHit.m_fraction * distance;
        hit.m_faceIndex = nativeHit.m_triangleIndex;
        hit.m_childIndex = nativeHit.m_childIndex;
        return true;
    }

    ContactSnapshotResult World::GetShapeContacts(
        const ShapeHandle shapeHandle, const AZStd::span<ContactSnapshot> contacts, const AZStd::span<ContactPoint> points) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr ||
            !Box3D::GetTouchingContacts(shapeSlot->m_nativeId, m_nativeContactScratch, m_nativeContactSnapshots, m_nativeContactPoints))
        {
            return {};
        }
        return CopyContactSnapshots(m_nativeContactSnapshots, contacts, points);
    }

    BufferResult World::GetShapeSensorOverlaps(const ShapeHandle shapeHandle, const AZStd::span<SensorOverlap> overlaps) const
    {
        AZStd::lock_guard lock(m_mutex);
        const ShapeSlot* shapeSlot = FindShapeSlot(shapeHandle);
        if (shapeSlot == nullptr || !Box3D::GetSensorOverlaps(shapeSlot->m_nativeId, m_nativeSensorScratch, m_nativeSensorOverlaps))
        {
            return {};
        }
        return CopySensorOverlaps(m_nativeSensorOverlaps, overlaps);
    }

    bool World::UsesMaterial(const MaterialHandle materialHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        for (size_t shapeIndex = 0; shapeIndex < m_shapeSlots.size(); ++shapeIndex)
        {
            if (m_shapeSlots[shapeIndex].m_generation != 0)
            {
                const AZStd::vector<MaterialHandle>& materials = m_shapeResources[shapeIndex].m_materials;
                if (AZStd::find(materials.begin(), materials.end(), materialHandle) != materials.end())
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool World::RefreshMaterial(const MaterialHandle materialHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        for (AZ::u32 shapeIndex = 0; shapeIndex < m_shapeSlots.size(); ++shapeIndex)
        {
            ShapeSlot& slot = m_shapeSlots[shapeIndex];
            ShapeResources& resources = m_shapeResources[shapeIndex];
            if (slot.m_generation == 0 ||
                AZStd::find(resources.m_materials.begin(), resources.m_materials.end(), materialHandle) == resources.m_materials.end())
            {
                continue;
            }

            AZStd::vector<SurfaceMaterial> nativeMaterials;
            nativeMaterials.reserve(resources.m_materials.size());
            float density = 0.0f;
            float explosionScale = slot.m_explosionScale;
            for (MaterialHandle referencedMaterial : resources.m_materials)
            {
                MaterialConfiguration materialConfiguration;
                if (!m_system.GetMaterial(referencedMaterial, materialConfiguration))
                {
                    return false;
                }
                if (nativeMaterials.empty())
                {
                    density = materialConfiguration.m_density;
                    explosionScale = materialConfiguration.m_explosionScale;
                }
                nativeMaterials.push_back(m_system.ResolveMaterial(referencedMaterial));
            }
            NativeShapeState state;
            if (!Box3D::GetShapeState(slot.m_nativeId, state))
            {
                return false;
            }
            if (state.m_type == NativeShapeType::Compound || m_recording != nullptr)
            {
                const ShapeHandle shapeHandle = Internal::MakeWorldMemberHandle<ShapeHandle>(m_worldIndex, shapeIndex, slot.m_generation);
                if (!RecreateShapeWithMaterials(
                        shapeHandle, slot, resources, nativeMaterials, density, explosionScale, resources.m_materials))
                {
                    return false;
                }
                continue;
            }
            if (!Box3D::SetShapeMaterials(slot.m_nativeId, nativeMaterials))
            {
                return false;
            }
            slot.m_explosionScale = explosionScale;
        }
        return true;
    }

    bool World::BuildNativeJointConfiguration(
        const AZ::u32 jointIndex, const JointConfiguration& configuration, NativeJointConfiguration& nativeConfiguration) const
    {
        if (!IsValidJointConfiguration(configuration))
        {
            return false;
        }

        const JointCommonConfiguration& common = GetCommonJointConfiguration(configuration);
        const BodySlot* parentBody = FindBodySlot(common.m_parentBody);
        const BodySlot* childBody = FindBodySlot(common.m_childBody);
        if (parentBody == nullptr || childBody == nullptr)
        {
            return false;
        }

        nativeConfiguration = {};
        nativeConfiguration.m_bodyA = parentBody != nullptr ? parentBody->m_nativeId : BodyId{};
        nativeConfiguration.m_bodyB = childBody->m_nativeId;
        nativeConfiguration.m_localFrameA = common.m_parentLocalFrame;
        nativeConfiguration.m_localFrameB = common.m_childLocalFrame;
        nativeConfiguration.m_userData = EncodeSlotIndex(jointIndex);
        nativeConfiguration.m_forceThreshold = common.m_forceThreshold;
        nativeConfiguration.m_torqueThreshold = common.m_torqueThreshold;
        nativeConfiguration.m_constraintHertz = common.m_constraintHertz;
        nativeConfiguration.m_constraintDampingRatio = common.m_constraintDampingRatio;
        nativeConfiguration.m_drawScale = common.m_drawScale;
        nativeConfiguration.m_collideConnected = common.m_collideConnected;
        AZStd::visit(
            [&nativeConfiguration](const auto& typedConfiguration)
            {
                using Configuration = AZStd::remove_cvref_t<decltype(typedConfiguration)>;
                if constexpr (AZStd::is_same_v<Configuration, ParallelJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Parallel;
                    nativeConfiguration.m_parallel = { typedConfiguration.m_hertz,
                                                       typedConfiguration.m_dampingRatio,
                                                       typedConfiguration.m_maxTorque };
                }
                else if constexpr (AZStd::is_same_v<Configuration, DistanceJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Distance;
                    nativeConfiguration.m_distance = { typedConfiguration.m_length,           typedConfiguration.m_lowerSpringForce,
                                                       typedConfiguration.m_upperSpringForce, typedConfiguration.m_hertz,
                                                       typedConfiguration.m_dampingRatio,     typedConfiguration.m_minLength,
                                                       typedConfiguration.m_maxLength,        typedConfiguration.m_maxMotorForce,
                                                       typedConfiguration.m_motorSpeed,       typedConfiguration.m_enableSpring,
                                                       typedConfiguration.m_enableLimit,      typedConfiguration.m_enableMotor };
                }
                else if constexpr (AZStd::is_same_v<Configuration, FilterJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Filter;
                }
                else if constexpr (AZStd::is_same_v<Configuration, MotorJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Motor;
                    nativeConfiguration.m_motor = { typedConfiguration.m_linearVelocity,      typedConfiguration.m_angularVelocity,
                                                    typedConfiguration.m_maxVelocityForce,    typedConfiguration.m_maxVelocityTorque,
                                                    typedConfiguration.m_linearHertz,         typedConfiguration.m_linearDampingRatio,
                                                    typedConfiguration.m_maxSpringForce,      typedConfiguration.m_angularHertz,
                                                    typedConfiguration.m_angularDampingRatio, typedConfiguration.m_maxSpringTorque };
                }
                else if constexpr (AZStd::is_same_v<Configuration, PrismaticJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Prismatic;
                    nativeConfiguration.m_prismatic = { typedConfiguration.m_hertz,
                                                        typedConfiguration.m_dampingRatio,
                                                        typedConfiguration.m_targetTranslation,
                                                        typedConfiguration.m_lowerTranslation,
                                                        typedConfiguration.m_upperTranslation,
                                                        typedConfiguration.m_maxMotorForce,
                                                        typedConfiguration.m_motorSpeed,
                                                        typedConfiguration.m_enableSpring,
                                                        typedConfiguration.m_enableLimit,
                                                        typedConfiguration.m_enableMotor };
                }
                else if constexpr (AZStd::is_same_v<Configuration, RevoluteJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Revolute;
                    nativeConfiguration.m_revolute = { typedConfiguration.m_targetAngle,  typedConfiguration.m_hertz,
                                                       typedConfiguration.m_dampingRatio, typedConfiguration.m_lowerAngle,
                                                       typedConfiguration.m_upperAngle,   typedConfiguration.m_maxMotorTorque,
                                                       typedConfiguration.m_motorSpeed,   typedConfiguration.m_enableSpring,
                                                       typedConfiguration.m_enableLimit,  typedConfiguration.m_enableMotor };
                }
                else if constexpr (AZStd::is_same_v<Configuration, SphericalJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Spherical;
                    nativeConfiguration.m_spherical = {
                        typedConfiguration.m_targetRotation,  typedConfiguration.m_motorVelocity,    typedConfiguration.m_hertz,
                        typedConfiguration.m_dampingRatio,    typedConfiguration.m_coneAngle,        typedConfiguration.m_lowerTwistAngle,
                        typedConfiguration.m_upperTwistAngle, typedConfiguration.m_maxMotorTorque,   typedConfiguration.m_enableSpring,
                        typedConfiguration.m_enableConeLimit, typedConfiguration.m_enableTwistLimit, typedConfiguration.m_enableMotor
                    };
                }
                else if constexpr (AZStd::is_same_v<Configuration, WeldJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Weld;
                    nativeConfiguration.m_weld = { typedConfiguration.m_linearHertz,
                                                   typedConfiguration.m_angularHertz,
                                                   typedConfiguration.m_linearDampingRatio,
                                                   typedConfiguration.m_angularDampingRatio };
                }
                else if constexpr (AZStd::is_same_v<Configuration, WheelJointConfiguration>)
                {
                    nativeConfiguration.m_kind = JointKind::Wheel;
                    nativeConfiguration.m_wheel = {
                        typedConfiguration.m_suspensionHertz,        typedConfiguration.m_suspensionDampingRatio,
                        typedConfiguration.m_lowerSuspensionLimit,   typedConfiguration.m_upperSuspensionLimit,
                        typedConfiguration.m_maxSpinTorque,          typedConfiguration.m_spinSpeed,
                        typedConfiguration.m_steeringHertz,          typedConfiguration.m_steeringDampingRatio,
                        typedConfiguration.m_targetSteeringAngle,    typedConfiguration.m_maxSteeringTorque,
                        typedConfiguration.m_lowerSteeringLimit,     typedConfiguration.m_upperSteeringLimit,
                        typedConfiguration.m_enableSuspensionSpring, typedConfiguration.m_enableSuspensionLimit,
                        typedConfiguration.m_enableSpinMotor,        typedConfiguration.m_enableSteering,
                        typedConfiguration.m_enableSteeringLimit
                    };
                }
            },
            configuration);
        return true;
    }

    JointHandle World::CreateJoint(const JointConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::CreateJoint");
        AZ::u32 jointIndex = 0;
        if (!m_freeJointSlots.empty())
        {
            jointIndex = m_freeJointSlots.back();
            m_freeJointSlots.pop_back();
        }
        else
        {
            if (m_jointSlots.size() > Internal::MaximumWorldMemberIndex)
            {
                return {};
            }
            jointIndex = aznumeric_cast<AZ::u32>(m_jointSlots.size());
            m_jointSlots.emplace_back();
            m_jointConfigurations.emplace_back();
        }

        NativeJointConfiguration nativeConfiguration;
        if (!BuildNativeJointConfiguration(jointIndex, configuration, nativeConfiguration))
        {
            m_jointConfigurations[jointIndex] = JointConfiguration{};
            m_freeJointSlots.push_back(jointIndex);
            return {};
        }

        JointSlot& slot = m_jointSlots[jointIndex];
        slot.m_generation = m_jointGenerations.Acquire();
        if (slot.m_generation == 0)
        {
            m_jointConfigurations[jointIndex] = JointConfiguration{};
            m_freeJointSlots.push_back(jointIndex);
            return {};
        }
        slot.m_kind = nativeConfiguration.m_kind;
        m_jointConfigurations[jointIndex] = configuration;
        slot.m_nativeId = Box3D::CreateJoint(m_nativeId, nativeConfiguration);
        if (!Box3D::IsValid(slot.m_nativeId))
        {
            slot = JointSlot{};
            m_jointConfigurations[jointIndex] = JointConfiguration{};
            m_freeJointSlots.push_back(jointIndex);
            return {};
        }
        return Internal::MakeWorldMemberHandle<JointHandle>(m_worldIndex, jointIndex, slot.m_generation);
    }

    bool World::UpdateJoint(const JointHandle jointHandle, const JointConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::UpdateJoint");
        JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr)
        {
            return false;
        }

        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(jointHandle, parts))
        {
            return false;
        }
        NativeJointConfiguration nativeConfiguration;
        if (!BuildNativeJointConfiguration(parts.m_index, configuration, nativeConfiguration))
        {
            return false;
        }

        if (slot->m_kind == nativeConfiguration.m_kind)
        {
            if (!Box3D::UpdateJoint(slot->m_nativeId, nativeConfiguration))
            {
                return false;
            }
            m_jointConfigurations[parts.m_index] = configuration;
            return true;
        }

        const JointId replacementNativeId = Box3D::CreateJoint(m_nativeId, nativeConfiguration);
        if (!Box3D::IsValid(replacementNativeId))
        {
            return false;
        }
        Box3D::DestroyJoint(slot->m_nativeId);
        slot->m_nativeId = replacementNativeId;
        slot->m_kind = nativeConfiguration.m_kind;
        m_jointConfigurations[parts.m_index] = configuration;
        return true;
    }

    bool World::SetJointEntityId(const JointHandle jointHandle, const AZ::EntityId entityId)
    {
        AZStd::lock_guard lock(m_mutex);
        JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr)
        {
            return false;
        }
        m_entityJointCount -= slot->m_entityId.IsValid() ? 1 : 0;
        slot->m_entityId = entityId;
        m_entityJointCount += entityId.IsValid() ? 1 : 0;
        return true;
    }

    bool World::DestroyJoint(const JointHandle jointHandle, const bool wakeAttachedBodies)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::DestroyJoint");
        Internal::WorldMemberHandleParts parts;
        JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr || !Internal::DecodeWorldMemberHandle(jointHandle, parts))
        {
            return false;
        }
        Box3D::DestroyJoint(slot->m_nativeId, wakeAttachedBodies);
        m_entityJointCount -= slot->m_entityId.IsValid() ? 1 : 0;
        *slot = JointSlot{};
        m_jointConfigurations[parts.m_index] = JointConfiguration{};
        m_freeJointSlots.push_back(parts.m_index);
        return true;
    }

    bool World::WakeJointBodies(const JointHandle jointHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr)
        {
            return false;
        }
        Box3D::WakeJointBodies(slot->m_nativeId);
        return true;
    }

    bool World::GetJointConfiguration(const JointHandle jointHandle, JointConfiguration& configuration) const
    {
        AZStd::lock_guard lock(m_mutex);
        Internal::WorldMemberHandleParts parts;
        const JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr || !Internal::DecodeWorldMemberHandle(jointHandle, parts))
        {
            return false;
        }
        configuration = m_jointConfigurations[parts.m_index];
        return true;
    }

    bool World::GetJointMeasurements(const JointHandle jointHandle, JointMeasurements& measurements) const
    {
        AZStd::lock_guard lock(m_mutex);
        const JointSlot* slot = FindJointSlot(jointHandle);
        if (slot == nullptr)
        {
            return false;
        }
        measurements.m_constraintForce = Box3D::GetJointConstraintForce(slot->m_nativeId);
        measurements.m_constraintTorque = Box3D::GetJointConstraintTorque(slot->m_nativeId);
        measurements.m_linearSeparation = Box3D::GetJointLinearSeparation(slot->m_nativeId);
        measurements.m_angularSeparation = Box3D::GetJointAngularSeparation(slot->m_nativeId);
        measurements.m_state = Box3D::GetJointState(slot->m_nativeId, slot->m_kind);
        return true;
    }

    void World::EnsureCharacterScratchCapacity(size_t maximumPlaneCount)
    {
        if (maximumPlaneCount <= m_characterScratchCapacity)
        {
            return;
        }

        m_characterScratch = AZStd::make_unique<MoverScratch>(maximumPlaneCount);
        m_characterStepScratch = AZStd::make_unique<MoverScratch>(maximumPlaneCount);
        m_characterScratchCapacity = maximumPlaneCount;
    }

    CharacterHandle World::CreateCharacter(const CharacterConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::CreateCharacter");
        if (!IsValidCharacterConfiguration(configuration))
        {
            return {};
        }
        EnsureCharacterScratchCapacity(configuration.m_maximumContactPlanes);

        AZ::u32 characterIndex = 0;
        if (!m_freeCharacterSlots.empty())
        {
            characterIndex = m_freeCharacterSlots.back();
            m_freeCharacterSlots.pop_back();
        }
        else
        {
            if (m_characterSlots.size() > Internal::MaximumWorldMemberIndex)
            {
                return {};
            }
            characterIndex = aznumeric_cast<AZ::u32>(m_characterSlots.size());
            m_characterSlots.emplace_back();
            m_characterResources.emplace_back();
        }

        CharacterSlot& slot = m_characterSlots[characterIndex];
        slot.m_generation = m_characterGenerations.Acquire();
        if (slot.m_generation == 0)
        {
            m_characterResources[characterIndex] = CharacterResources{};
            m_freeCharacterSlots.push_back(characterIndex);
            return {};
        }
        CharacterResources& resources = m_characterResources[characterIndex];
        resources.m_configuration = configuration;
        resources.m_configuration.m_upDirection.Normalize();
        resources.m_state.m_basePosition = configuration.m_basePosition;
        resources.m_state.m_centerPosition =
            configuration.m_basePosition + 0.5f * configuration.m_height * resources.m_configuration.m_upDirection;
        return Internal::MakeWorldMemberHandle<CharacterHandle>(m_worldIndex, characterIndex, slot.m_generation);
    }

    bool World::UpdateCharacter(const CharacterHandle characterHandle, const CharacterConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::UpdateCharacter");
        AZ::u32 characterIndex = 0;
        CharacterSlot* slot = FindCharacterSlot(characterHandle, &characterIndex);
        if (slot == nullptr || !IsValidCharacterConfiguration(configuration))
        {
            return false;
        }
        EnsureCharacterScratchCapacity(configuration.m_maximumContactPlanes);

        CharacterResources& resources = m_characterResources[characterIndex];
        resources.m_configuration = configuration;
        resources.m_configuration.m_upDirection.Normalize();
        resources.m_state.m_basePosition = configuration.m_basePosition;
        resources.m_state.m_centerPosition =
            configuration.m_basePosition + 0.5f * configuration.m_height * resources.m_configuration.m_upDirection;
        resources.m_state.m_velocity = AZ::Vector3::CreateZero();
        resources.m_state.m_support = {};
        slot->m_pendingVelocity = AZ::Vector3::CreateZero();
        slot->m_hasPendingMove = false;
        return true;
    }

    bool World::DestroyCharacter(const CharacterHandle characterHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::DestroyCharacter");
        AZ::u32 characterIndex = 0;
        CharacterSlot* slot = FindCharacterSlot(characterHandle, &characterIndex);
        if (slot == nullptr)
        {
            return false;
        }
        *slot = CharacterSlot{};
        m_characterResources[characterIndex] = CharacterResources{};
        m_freeCharacterSlots.push_back(characterIndex);
        return true;
    }

    bool World::MoveCharacter(const CharacterHandle characterHandle, const AZ::Vector3& velocity, const float fixedTimeStep)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ::u32 characterIndex = 0;
        CharacterSlot* slot = FindCharacterSlot(characterHandle, &characterIndex);
        if (slot == nullptr || !velocity.IsFinite() || !AZ::IsFiniteFloat(fixedTimeStep) || fixedTimeStep <= 0.0f)
        {
            return false;
        }

        CharacterResources& resources = m_characterResources[characterIndex];
        if (resources.m_configuration.m_applyMoveOnFixedTick)
        {
            slot->m_pendingVelocity = velocity;
            slot->m_hasPendingMove = true;
            return true;
        }
        return MoveCharacterImmediately(resources, velocity, fixedTimeStep);
    }

    bool World::MoveCharacterImmediately(CharacterResources& resources, const AZ::Vector3& velocity, float fixedTimeStep)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::MoveCharacter");
        const CharacterConfiguration& configuration = resources.m_configuration;
        const AZ::Vector3 up = configuration.m_upDirection;
        const AZ::Vector3 clampedVelocity =
            velocity.GetLength() > configuration.m_maximumSpeed ? velocity.GetNormalized() * configuration.m_maximumSpeed : velocity;
        const AZ::Vector3 translation = clampedVelocity * fixedTimeStep;
        const AZ::Vector3 lowerCenter = configuration.m_radius * up;
        const AZ::Vector3 upperCenter = (configuration.m_height - configuration.m_radius) * up;
        const AZ::Vector3 initialPosition = resources.m_state.m_basePosition;
        const auto move =
            [this, &configuration, &lowerCenter, &upperCenter, fixedTimeStep](
                const AZ::Vector3& position, const AZ::Vector3& requestedTranslation, MoverScratch& scratch, MoverResult& result)
        {
            return Box3D::MoveCapsule(
                m_nativeId,
                position,
                lowerCenter,
                upperCenter,
                configuration.m_radius,
                requestedTranslation,
                fixedTimeStep,
                configuration.m_collisionFilter.m_categoryBits,
                configuration.m_collisionFilter.m_maskBits,
                {},
                {},
                configuration.m_maximumIterations,
                configuration.m_minimumMovementDistance,
                scratch,
                result);
        };
        MoverResult result;
        if (!move(initialPosition, translation, *m_characterScratch, result))
        {
            return false;
        }

        const float walkableDot = std::cos(AZ::DegToRad(configuration.m_maximumSlopeAngle));
        const auto hasWalkableSupport = [&up, walkableDot](const MoverScratch& scratch)
        {
            return AZStd::find_if(
                       scratch.GetPlanes().begin(),
                       scratch.GetPlanes().end(),
                       [&up, walkableDot](const MoverPlane& plane)
                       {
                           return plane.m_normal.Dot(up) >= walkableDot;
                       }) != scratch.GetPlanes().end();
        };

        const AZ::Vector3 lateralTranslation = translation - translation.Dot(up) * up;
        const float requestedLateralDistance = lateralTranslation.GetLength();
        bool stepped = false;
        if (configuration.m_stepHeight > 0.0f && requestedLateralDistance > configuration.m_minimumMovementDistance)
        {
            const AZ::Vector3 lateralDirection = lateralTranslation / requestedLateralDistance;
            const float directProgress = (result.m_position - initialPosition).Dot(lateralDirection);
            if (directProgress + configuration.m_minimumMovementDistance < requestedLateralDistance)
            {
                MoverResult raisedResult;
                MoverResult advancedResult;
                MoverResult settledResult;
                const AZ::Vector3 stepUp = configuration.m_stepHeight * up;
                const bool raised = move(initialPosition, stepUp, *m_characterStepScratch, raisedResult) &&
                    (raisedResult.m_position - initialPosition).Dot(up) + configuration.m_minimumMovementDistance >=
                        configuration.m_stepHeight;
                const bool advanced = raised && move(raisedResult.m_position, translation, *m_characterStepScratch, advancedResult);
                const bool settled = advanced &&
                    move(advancedResult.m_position,
                         -(configuration.m_stepHeight + configuration.m_groundStickDistance) * up,
                         *m_characterStepScratch,
                         settledResult);
                const float steppedProgress = (settledResult.m_position - initialPosition).Dot(lateralDirection);
                if (settled && hasWalkableSupport(*m_characterStepScratch) &&
                    steppedProgress > directProgress + configuration.m_minimumMovementDistance)
                {
                    result = settledResult;
                    m_characterScratch.swap(m_characterStepScratch);
                    stepped = true;
                }
            }
        }

        if (!stepped && configuration.m_groundStickDistance > 0.0f && clampedVelocity.Dot(up) <= 0.0f)
        {
            MoverResult groundedResult;
            if (move(result.m_position, -configuration.m_groundStickDistance * up, *m_characterStepScratch, groundedResult) &&
                hasWalkableSupport(*m_characterStepScratch))
            {
                result = groundedResult;
                m_characterScratch.swap(m_characterStepScratch);
            }
        }

        if (configuration.m_interactionScale > 0.0f)
        {
            for (const MoverPlane& plane : m_characterScratch->GetPlanes())
            {
                AZ::u32 bodyIndex = 0;
                if (!DecodeSlotIndex(plane.m_bodyUserData, bodyIndex) || bodyIndex >= m_bodySlots.size())
                {
                    continue;
                }
                const BodySlot& bodySlot = m_bodySlots[bodyIndex];
                const float approachSpeed = -clampedVelocity.Dot(plane.m_normal);
                if (approachSpeed <= 0.0f || Box3D::GetBodyType(bodySlot.m_nativeId) != NativeBodyType::Dynamic)
                {
                    continue;
                }
                const NativeMassProperties massProperties = Box3D::GetMassProperties(bodySlot.m_nativeId);
                if (massProperties.m_mass > 0.0f)
                {
                    Box3D::ApplyLinearImpulse(
                        bodySlot.m_nativeId,
                        -configuration.m_interactionScale * massProperties.m_mass * approachSpeed * plane.m_normal,
                        plane.m_point,
                        true);
                }
            }
        }

        resources.m_state.m_velocity = (result.m_position - initialPosition) / fixedTimeStep;
        resources.m_state.m_basePosition = result.m_position;
        resources.m_state.m_centerPosition = result.m_position + 0.5f * configuration.m_height * up;
        resources.m_state.m_support = {};
        float bestUpDot = 0.0f;
        for (const MoverPlane& plane : m_characterScratch->GetPlanes())
        {
            const float upDot = plane.m_normal.Dot(up);
            if (upDot <= bestUpDot)
            {
                continue;
            }
            AZ::u32 bodyIndex = 0;
            ResolvedShape resolvedShape;
            if (!DecodeSlotIndex(plane.m_bodyUserData, bodyIndex) || bodyIndex >= m_bodySlots.size() ||
                !ResolveShape(plane.m_shapeUserData, plane.m_shapeId, resolvedShape))
            {
                continue;
            }
            const BodySlot& bodySlot = m_bodySlots[bodyIndex];
            if (!Box3D::IsValid(bodySlot.m_nativeId))
            {
                continue;
            }
            bestUpDot = upDot;
            CharacterSupport& support = resources.m_state.m_support;
            support.m_state = upDot >= walkableDot ? CharacterSupportState::Supported : CharacterSupportState::Sliding;
            support.m_bodyHandle = Internal::MakeWorldMemberHandle<BodyHandle>(m_worldIndex, bodyIndex, bodySlot.m_generation);
            support.m_shapeHandle = resolvedShape.m_shapeHandle;
            support.m_position = plane.m_point;
            support.m_normal = plane.m_normal;
            support.m_velocity = Box3D::GetLinearVelocityAtWorldPoint(bodySlot.m_nativeId, plane.m_point);
        }
        return true;
    }

    bool World::ApplyPendingCharacterMoves(const float fixedTimeStep, const bool consume)
    {
        bool movedAllCharacters = true;
        for (size_t characterIndex = 0; characterIndex < m_characterSlots.size(); ++characterIndex)
        {
            CharacterSlot& slot = m_characterSlots[characterIndex];
            if (!slot.m_hasPendingMove)
            {
                continue;
            }
            CharacterResources& resources = m_characterResources[characterIndex];
            const bool moved = MoveCharacterImmediately(resources, slot.m_pendingVelocity, fixedTimeStep);
            movedAllCharacters = moved && movedAllCharacters;
            if (moved && resources.m_configuration.m_entityId.IsValid())
            {
                m_entityCharacterMoves.push_back({ resources.m_configuration.m_entityId, resources.m_state });
            }
            if (consume)
            {
                slot.m_hasPendingMove = false;
            }
        }
        return movedAllCharacters;
    }

    bool World::GetCharacterState(const CharacterHandle characterHandle, CharacterState& state) const
    {
        AZStd::lock_guard lock(m_mutex);
        AZ::u32 characterIndex = 0;
        const CharacterSlot* slot = FindCharacterSlot(characterHandle, &characterIndex);
        if (slot == nullptr)
        {
            return false;
        }
        state = m_characterResources[characterIndex].m_state;
        return true;
    }

    bool World::GetCharacterConfiguration(const CharacterHandle characterHandle, CharacterConfiguration& configuration) const
    {
        AZStd::lock_guard lock(m_mutex);
        AZ::u32 characterIndex = 0;
        const CharacterSlot* slot = FindCharacterSlot(characterHandle, &characterIndex);
        if (slot == nullptr)
        {
            return false;
        }
        configuration = m_characterResources[characterIndex].m_configuration;
        return true;
    }

    bool World::RaycastClosest(const RaycastRequest& request, QueryHit& hit) const
    {
#if !defined(AZ_RELEASE_BUILD)
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::RaycastClosest");
#endif
        AZStd::lock_guard lock(m_mutex);
        return CanUseClosestRaycast(request.m_filter) ? RaycastClosestDefaultUnlocked(request, hit)
                                                      : RaycastClosestFilteredUnlocked(request, hit);
    }

    BufferResult World::RaycastClosestBatch(AZStd::span<const RaycastRequest> requests, AZStd::span<ClosestQueryResult> results) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::RaycastClosestBatch");
        AZStd::lock_guard lock(m_mutex);
        const size_t requestCount = AZStd::min(requests.size(), results.size());
        if (requestCount == 0)
        {
            return { 0, requests.size() };
        }

        constexpr size_t minimumRaycastsPerJob = 128;
        const size_t jobCount = AZStd::min(requestCount / minimumRaycastsPerJob, static_cast<size_t>(m_systemConfiguration.m_workerCount));
        const bool canRunConcurrently = m_recording == nullptr && m_configuration.m_jobContext != nullptr && jobCount > 1 &&
            AZStd::all_of(requests.begin(),
                          requests.begin() + requestCount,
                          [this](const RaycastRequest& request)
                          {
                              return CanUseClosestRaycast(request.m_filter);
                          });
        if (!canRunConcurrently)
        {
            for (size_t requestIndex = 0; requestIndex < requestCount; ++requestIndex)
            {
                results[requestIndex] = {};
                results[requestIndex].m_found = CanUseClosestRaycast(requests[requestIndex].m_filter)
                    ? RaycastClosestDefaultUnlocked(requests[requestIndex], results[requestIndex].m_hit)
                    : RaycastClosestFilteredUnlocked(requests[requestIndex], results[requestIndex].m_hit);
            }
            return { requestCount, requests.size() };
        }

        const size_t requestsPerJob = (requestCount + jobCount - 1) / jobCount;
        const auto processRequests = [this, requests, results](const size_t beginIndex, const size_t endIndex)
        {
            DeterministicFloatScope floatScope;
            for (size_t requestIndex = beginIndex; requestIndex < endIndex; ++requestIndex)
            {
                results[requestIndex] = {};
                results[requestIndex].m_found = RaycastClosestDefaultUnlocked(requests[requestIndex], results[requestIndex].m_hit);
            }
        };
        AZ::JobCompletion completion(m_configuration.m_jobContext);
        completion.Reset(true);
        for (size_t jobIndex = 1; jobIndex < jobCount; ++jobIndex)
        {
            const size_t beginIndex = jobIndex * requestsPerJob;
            const size_t endIndex = AZStd::min(beginIndex + requestsPerJob, requestCount);
            if (beginIndex == endIndex)
            {
                continue;
            }

            AZ::Job* job = AZ::CreateJobFunction(
                [processRequests, beginIndex, endIndex]
                {
                    processRequests(beginIndex, endIndex);
                },
                true,
                m_configuration.m_jobContext);
            job->SetDependent(&completion);
            job->Start();
        }
        processRequests(0, AZStd::min(requestsPerJob, requestCount));
        completion.StartAndWaitForCompletion();
        return { requestCount, requests.size() };
    }

    bool World::CanUseClosestRaycast(const QueryFilter& filter) const
    {
        return filter.m_callback == nullptr && filter.m_bodyTypes == QueryBodyTypes::All &&
            (filter.m_includeSensors || m_sensorShapeCount == 0);
    }

    bool World::RaycastClosestDefaultUnlocked(const RaycastRequest& request, QueryHit& hit) const
    {
        if (!request.m_start.IsFinite() || !request.m_direction.IsFinite() || request.m_direction.IsZero() ||
            !AZ::IsFiniteFloat(request.m_distance) || request.m_distance <= 0.0f)
        {
            return false;
        }

        ClosestCastHit nativeHit;
        if (!Box3D::CastRayClosest(
                m_nativeId,
                request.m_start,
                (request.m_direction.IsNormalized() ? request.m_direction : request.m_direction.GetNormalized()) * request.m_distance,
                request.m_filter.m_collisionFilter.m_categoryBits,
                request.m_filter.m_collisionFilter.m_maskBits,
                nativeHit))
        {
            return false;
        }

        ResolvedShape resolvedShape;
        if (!ResolveShape(nativeHit.m_shapeId, resolvedShape))
        {
            return false;
        }
        const ShapeSlot& shapeSlot = *resolvedShape.m_slot;
        hit.m_bodyHandle = shapeSlot.m_bodyHandle;
        hit.m_shapeHandle = resolvedShape.m_shapeHandle;
        hit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
        hit.m_position = nativeHit.m_position;
        hit.m_normal = nativeHit.m_normal;
        hit.m_distance = nativeHit.m_fraction * request.m_distance;
        hit.m_fraction = nativeHit.m_fraction;
        hit.m_faceIndex = nativeHit.m_triangleIndex;
        hit.m_childIndex = shapeSlot.m_isCompound ? nativeHit.m_childIndex : -1;
        return true;
    }

    bool World::RaycastClosestFilteredUnlocked(const RaycastRequest& request, QueryHit& hit) const
    {
        AZStd::array<QueryHit, 1> hits;
        struct FilterContext final
        {
            QueryFilterCallback m_callback = nullptr;
            void* m_userData = nullptr;
        } filterContext{ request.m_filter.m_callback, request.m_filter.m_userData };
        RaycastRequest filteredRequest = request;
        filteredRequest.m_filter.m_callback = [](const QueryHit& candidate, void* userData)
        {
            const auto& context = *static_cast<const FilterContext*>(userData);
            return candidate.m_fraction > 0.0f && (context.m_callback == nullptr || context.m_callback(candidate, context.m_userData));
        };
        filteredRequest.m_filter.m_userData = &filterContext;
        const QueryResult result = Raycast(filteredRequest, hits);
        if (result.m_hitCount == 0)
        {
            return false;
        }
        hit = hits[0];
        return true;
    }

    QueryResult World::Raycast(const RaycastRequest& request, AZStd::span<QueryHit> hits) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::Raycast");
        AZStd::lock_guard lock(m_mutex);
        QueryResult result;
        if (!request.m_start.IsFinite() || !request.m_direction.IsFinite() || request.m_direction.IsZero() ||
            !AZ::IsFiniteFloat(request.m_distance) || request.m_distance <= 0.0f)
        {
            return result;
        }

        struct Context final
        {
            const World& m_world;
            const QueryFilter& m_filter;
            AZStd::span<QueryHit> m_hits;
            float m_distance;
            size_t m_requiredHitCount = 0;
            size_t m_hitCount = 0;
        } context{ *this, request.m_filter, hits, request.m_distance };

        const auto callback = [](const CastHit& nativeHit, void* opaqueContext)
        {
            auto& queryContext = *static_cast<Context*>(opaqueContext);
            ResolvedShape resolvedShape;
            if (!queryContext.m_world.ResolveShape(nativeHit.m_shapeUserData, nativeHit.m_shapeId, resolvedShape))
            {
                return 1.0f;
            }
            const ShapeSlot& shapeSlot = *resolvedShape.m_slot;
            const BodySlot* bodySlot = queryContext.m_world.FindBodySlot(shapeSlot.m_bodyHandle);
            if (bodySlot == nullptr || (!queryContext.m_filter.m_includeSensors && shapeSlot.m_isSensor) ||
                !IncludesBodyType(queryContext.m_filter.m_bodyTypes, bodySlot->m_type))
            {
                return 1.0f;
            }

            QueryHit queryHit;
            queryHit.m_bodyHandle = shapeSlot.m_bodyHandle;
            queryHit.m_shapeHandle = resolvedShape.m_shapeHandle;
            queryHit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
            queryHit.m_position = nativeHit.m_position;
            queryHit.m_normal = nativeHit.m_normal;
            queryHit.m_fraction = nativeHit.m_fraction;
            queryHit.m_distance = nativeHit.m_fraction * queryContext.m_distance;
            queryHit.m_faceIndex = nativeHit.m_triangleIndex;
            queryHit.m_childIndex = shapeSlot.m_isCompound ? nativeHit.m_childIndex : -1;
            if (queryContext.m_filter.m_callback != nullptr &&
                !queryContext.m_filter.m_callback(queryHit, queryContext.m_filter.m_userData))
            {
                return 1.0f;
            }

            ++queryContext.m_requiredHitCount;
            if (queryContext.m_hits.empty())
            {
                return 1.0f;
            }
            if (queryContext.m_hitCount < queryContext.m_hits.size())
            {
                queryContext.m_hits[queryContext.m_hitCount++] = queryHit;
            }
            else
            {
                auto farthest = AZStd::minmax_element(
                                    queryContext.m_hits.begin(),
                                    queryContext.m_hits.end(),
                                    [](const QueryHit& left, const QueryHit& right)
                                    {
                                        return left.m_fraction < right.m_fraction;
                                    })
                                    .second;
                if (queryHit.m_fraction < farthest->m_fraction)
                {
                    *farthest = queryHit;
                }
            }
            return 1.0f;
        };

        Box3D::CastRay(
            m_nativeId,
            request.m_start,
            request.m_direction.GetNormalized() * request.m_distance,
            request.m_filter.m_collisionFilter.m_categoryBits,
            request.m_filter.m_collisionFilter.m_maskBits,
            callback,
            &context);
        result.m_hitCount = context.m_hitCount;
        result.m_requiredHitCount = context.m_requiredHitCount;
        AZStd::sort(
            hits.begin(),
            hits.begin() + result.m_hitCount,
            [](const QueryHit& left, const QueryHit& right)
            {
                if (left.m_fraction != right.m_fraction)
                {
                    return left.m_fraction < right.m_fraction;
                }
                if (left.m_bodyHandle != right.m_bodyHandle)
                {
                    return left.m_bodyHandle < right.m_bodyHandle;
                }
                return left.m_shapeHandle < right.m_shapeHandle;
            });
        return result;
    }

    QueryResult World::ShapeCast(const ShapeCastRequest& request, AZStd::span<QueryHit> hits) const
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::ShapeCast");
        QueryProxy proxy;
        if (!request.m_translation.IsFinite() || request.m_translation.IsZero() ||
            !BuildQueryProxy(request.m_geometry, request.m_start, proxy))
        {
            return {};
        }

        const RaycastRequest raycastTemplate{
            request.m_start.GetTranslation(), request.m_translation.GetNormalized(), request.m_filter, request.m_translation.GetLength()
        };
        struct Context final
        {
            const World& m_world;
            const RaycastRequest& m_request;
            AZStd::span<QueryHit> m_hits;
            size_t m_requiredHitCount = 0;
            size_t m_hitCount = 0;
        } context{ *this, raycastTemplate, hits };
        const auto callback = [](const CastHit& nativeHit, void* opaqueContext)
        {
            auto& queryContext = *static_cast<Context*>(opaqueContext);
            ResolvedShape resolvedShape;
            if (!queryContext.m_world.ResolveShape(nativeHit.m_shapeUserData, nativeHit.m_shapeId, resolvedShape))
            {
                return 1.0f;
            }
            const ShapeSlot& shapeSlot = *resolvedShape.m_slot;
            const BodySlot* bodySlot = queryContext.m_world.FindBodySlot(shapeSlot.m_bodyHandle);
            const QueryFilter& filter = queryContext.m_request.m_filter;
            if (bodySlot == nullptr || (!filter.m_includeSensors && shapeSlot.m_isSensor) ||
                !IncludesBodyType(filter.m_bodyTypes, bodySlot->m_type))
            {
                return 1.0f;
            }
            QueryHit queryHit;
            queryHit.m_bodyHandle = shapeSlot.m_bodyHandle;
            queryHit.m_shapeHandle = resolvedShape.m_shapeHandle;
            queryHit.m_materialHandle = Internal::HandleAccess::Create<MaterialHandle>(nativeHit.m_userMaterialId);
            queryHit.m_position = nativeHit.m_position;
            queryHit.m_normal = nativeHit.m_normal;
            queryHit.m_fraction = nativeHit.m_fraction;
            queryHit.m_distance = nativeHit.m_fraction * queryContext.m_request.m_distance;
            queryHit.m_faceIndex = nativeHit.m_triangleIndex;
            queryHit.m_childIndex = shapeSlot.m_isCompound ? nativeHit.m_childIndex : -1;
            if (filter.m_callback != nullptr && !filter.m_callback(queryHit, filter.m_userData))
            {
                return 1.0f;
            }
            ++queryContext.m_requiredHitCount;
            if (queryContext.m_hitCount < queryContext.m_hits.size())
            {
                queryContext.m_hits[queryContext.m_hitCount++] = queryHit;
            }
            else if (!queryContext.m_hits.empty())
            {
                const auto farther = [](const QueryHit& left, const QueryHit& right)
                {
                    if (left.m_fraction != right.m_fraction)
                    {
                        return left.m_fraction < right.m_fraction;
                    }
                    if (left.m_bodyHandle != right.m_bodyHandle)
                    {
                        return left.m_bodyHandle < right.m_bodyHandle;
                    }
                    return left.m_shapeHandle < right.m_shapeHandle;
                };
                QueryHit& farthestHit = *AZStd::minmax_element(queryContext.m_hits.begin(), queryContext.m_hits.end(), farther).second;
                if (farther(queryHit, farthestHit))
                {
                    farthestHit = queryHit;
                }
            }
            return 1.0f;
        };
        Box3D::CastShape(
            m_nativeId,
            request.m_start.GetTranslation(),
            proxy.GetPoints(),
            proxy.m_radius,
            request.m_translation,
            request.m_filter.m_collisionFilter.m_categoryBits,
            request.m_filter.m_collisionFilter.m_maskBits,
            callback,
            &context);
        AZStd::sort(
            hits.begin(),
            hits.begin() + context.m_hitCount,
            [](const QueryHit& left, const QueryHit& right)
            {
                if (left.m_fraction != right.m_fraction)
                {
                    return left.m_fraction < right.m_fraction;
                }
                if (left.m_bodyHandle != right.m_bodyHandle)
                {
                    return left.m_bodyHandle < right.m_bodyHandle;
                }
                return left.m_shapeHandle < right.m_shapeHandle;
            });
        return { context.m_hitCount, context.m_requiredHitCount };
    }

    template<class Hit>
    QueryResult World::OverlapImpl(const OverlapRequest& request, AZStd::span<Hit> hits) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::Overlap");
        AZStd::lock_guard lock(m_mutex);
        QueryProxy proxy;
        AZStd::array spherePoint{ AZ::Vector3::CreateZero() };
        AZStd::span<const AZ::Vector3> points;
        float radius = 0.0f;
        if (const auto* sphere = AZStd::get_if<SphereShapeConfiguration>(&request.m_geometry))
        {
            const float scale = request.m_transform.GetUniformScale();
            if (!IsValidTransform(request.m_transform) || !AZ::IsFiniteFloat(sphere->m_radius) || sphere->m_radius <= 0.0f)
            {
                return {};
            }
            points = spherePoint;
            radius = sphere->m_radius * scale;
            if (!AZ::IsFiniteFloat(radius))
            {
                return {};
            }
        }
        else if (BuildQueryProxy(request.m_geometry, request.m_transform, proxy))
        {
            points = proxy.GetPoints();
            radius = proxy.m_radius;
        }
        else
        {
            return {};
        }

        struct Context final
        {
            const World& m_world;
            const QueryFilter& m_filter;
            AZStd::span<Hit> m_hits;
            size_t m_requiredHitCount = 0;
            size_t m_hitCount = 0;
            bool m_filterSensors = false;
            bool m_filterBodyTypes = false;
        } context{ *this,
                   request.m_filter,
                   hits,
                   0,
                   0,
                   !request.m_filter.m_includeSensors && m_sensorShapeCount > 0,
                   request.m_filter.m_bodyTypes != QueryBodyTypes::All };
        const auto callback = [](const OverlapCandidate& candidate, void* opaqueContext)
        {
            auto& queryContext = *static_cast<Context*>(opaqueContext);
            ResolvedShape resolvedShape;
            if (!queryContext.m_world.ResolveShape(candidate.m_shapeUserData, candidate.m_shapeId, resolvedShape) ||
                (queryContext.m_filterSensors && resolvedShape.m_slot->m_isSensor))
            {
                return true;
            }
            if (queryContext.m_filterBodyTypes)
            {
                const BodySlot* bodySlot = queryContext.m_world.FindBodySlot(resolvedShape.m_slot->m_bodyHandle);
                if (bodySlot == nullptr || !IncludesBodyType(queryContext.m_filter.m_bodyTypes, bodySlot->m_type))
                {
                    return true;
                }
            }
            const BodyHandle bodyHandle = resolvedShape.m_slot->m_bodyHandle;
            const ShapeHandle shapeHandle = resolvedShape.m_shapeHandle;
            if (queryContext.m_filter.m_callback != nullptr)
            {
                QueryHit filterHit;
                filterHit.m_bodyHandle = bodyHandle;
                filterHit.m_shapeHandle = shapeHandle;
                if (!queryContext.m_filter.m_callback(filterHit, queryContext.m_filter.m_userData))
                {
                    return true;
                }
            }
            ++queryContext.m_requiredHitCount;
            if (queryContext.m_hitCount < queryContext.m_hits.size())
            {
                Hit& hit = queryContext.m_hits[queryContext.m_hitCount++];
                hit.m_bodyHandle = bodyHandle;
                hit.m_shapeHandle = shapeHandle;
            }
            return true;
        };
        {
            AZ_PROFILE_SCOPE(Physics, "Box3D::World::Overlap::Native");
            Box3D::OverlapShape(
                m_nativeId,
                request.m_transform.GetTranslation(),
                points,
                radius,
                request.m_filter.m_collisionFilter.m_categoryBits,
                request.m_filter.m_collisionFilter.m_maskBits,
                callback,
                &context);
        }
        {
            AZ_PROFILE_SCOPE(Physics, "Box3D::World::Overlap::Sort");
            if (context.m_hitCount > 1)
            {
                AZStd::sort(
                    hits.begin(),
                    hits.begin() + context.m_hitCount,
                    [](const Hit& left, const Hit& right)
                    {
                        return left.m_bodyHandle != right.m_bodyHandle ? left.m_bodyHandle < right.m_bodyHandle
                                                                       : left.m_shapeHandle < right.m_shapeHandle;
                    });
            }
        }
        return { context.m_hitCount, context.m_requiredHitCount };
    }

    QueryResult World::Overlap(const OverlapRequest& request, const AZStd::span<OverlapHit> hits) const
    {
        return OverlapImpl(request, hits);
    }

    QueryResult World::Overlap(const OverlapRequest& request, const AZStd::span<QueryHit> hits) const
    {
        return OverlapImpl(request, hits);
    }

    template<class Hit>
    QueryResult World::OverlapAabbImpl(const AabbOverlapRequest& request, AZStd::span<Hit> hits) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::OverlapAabb");
        AZStd::lock_guard lock(m_mutex);
        if (!request.m_aabb.IsValid())
        {
            return {};
        }
        struct Context final
        {
            const World& m_world;
            const QueryFilter& m_filter;
            AZStd::span<Hit> m_hits;
            size_t m_requiredHitCount = 0;
            size_t m_hitCount = 0;
            bool m_filterSensors = false;
            bool m_filterBodyTypes = false;
        } context{ *this,
                   request.m_filter,
                   hits,
                   0,
                   0,
                   !request.m_filter.m_includeSensors && m_sensorShapeCount > 0,
                   request.m_filter.m_bodyTypes != QueryBodyTypes::All };
        const auto callback = [](const OverlapCandidate& candidate, void* opaqueContext)
        {
            auto& queryContext = *static_cast<Context*>(opaqueContext);
            ResolvedShape resolvedShape;
            if (!queryContext.m_world.ResolveShape(candidate.m_shapeUserData, candidate.m_shapeId, resolvedShape) ||
                (queryContext.m_filterSensors && resolvedShape.m_slot->m_isSensor))
            {
                return true;
            }
            if (queryContext.m_filterBodyTypes)
            {
                const BodySlot* bodySlot = queryContext.m_world.FindBodySlot(resolvedShape.m_slot->m_bodyHandle);
                if (bodySlot == nullptr || !IncludesBodyType(queryContext.m_filter.m_bodyTypes, bodySlot->m_type))
                {
                    return true;
                }
            }
            if (queryContext.m_filter.m_callback != nullptr)
            {
                QueryHit filterHit;
                filterHit.m_bodyHandle = resolvedShape.m_slot->m_bodyHandle;
                filterHit.m_shapeHandle = resolvedShape.m_shapeHandle;
                if (!queryContext.m_filter.m_callback(filterHit, queryContext.m_filter.m_userData))
                {
                    return true;
                }
            }
            ++queryContext.m_requiredHitCount;
            if (queryContext.m_hitCount < queryContext.m_hits.size())
            {
                Hit& hit = queryContext.m_hits[queryContext.m_hitCount++];
                hit.m_bodyHandle = resolvedShape.m_slot->m_bodyHandle;
                hit.m_shapeHandle = resolvedShape.m_shapeHandle;
            }
            return true;
        };
        {
            AZ_PROFILE_SCOPE(Physics, "Box3D::World::OverlapAabb::Native");
            Box3D::OverlapAabb(
                m_nativeId,
                request.m_aabb,
                request.m_filter.m_collisionFilter.m_categoryBits,
                request.m_filter.m_collisionFilter.m_maskBits,
                callback,
                &context);
        }
        {
            AZ_PROFILE_SCOPE(Physics, "Box3D::World::OverlapAabb::Sort");
            AZStd::sort(
                hits.begin(),
                hits.begin() + context.m_hitCount,
                [](const Hit& left, const Hit& right)
                {
                    if (left.m_bodyHandle != right.m_bodyHandle)
                    {
                        return left.m_bodyHandle < right.m_bodyHandle;
                    }
                    return left.m_shapeHandle < right.m_shapeHandle;
                });
        }
        return { context.m_hitCount, context.m_requiredHitCount };
    }

    QueryResult World::OverlapAabb(const AabbOverlapRequest& request, const AZStd::span<OverlapHit> hits) const
    {
        return OverlapAabbImpl(request, hits);
    }

    QueryResult World::OverlapAabb(const AabbOverlapRequest& request, const AZStd::span<QueryHit> hits) const
    {
        return OverlapAabbImpl(request, hits);
    }

    StepEvents World::GetStepEvents() const
    {
        AZStd::lock_guard lock(m_mutex);
        return { m_bodyMoveEvents, m_sensorEvents, m_contactEvents, m_contactPoints, m_contactHitEvents, m_jointEvents };
    }

    bool World::SetContactCallbacks(const CollisionFilterCallback collisionFilterCallback, const PreSolveCallback preSolveCallback, void* userData)
    {
        AZStd::lock_guard lock(m_mutex);
        if (m_recording != nullptr && (collisionFilterCallback != nullptr || preSolveCallback != nullptr))
        {
            return false;
        }
        Box3D::SetContactCallbacks(m_nativeId, this, collisionFilterCallback != nullptr, preSolveCallback != nullptr);
        m_collisionFilterCallback = collisionFilterCallback;
        m_preSolveCallback = preSolveCallback;
        m_contactCallbackUserData = userData;
        return true;
    }

    bool World::GetStatistics(const StatisticsFlags flags, WorldStatistics& statistics) const
    {
        AZStd::lock_guard lock(m_mutex);
        const bool success = Box3D::GetStatistics(m_nativeId, flags, statistics);
        statistics.m_simulationTick = m_lastCompletedTick;
        return success;
    }

    bool World::StartRecording(const size_t initialCapacityBytes)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::StartRecording");
        if (m_recording != nullptr || m_collisionFilterCallback != nullptr || m_preSolveCallback != nullptr ||
            m_systemConfiguration.m_frictionCallback != nullptr || m_systemConfiguration.m_restitutionCallback != nullptr)
        {
            return false;
        }
        m_recording = Box3D::CreateRecording(initialCapacityBytes);
        if (m_recording != nullptr && Box3D::StartRecording(m_nativeId, *m_recording))
        {
            return true;
        }
        m_recording.reset();
        return false;
    }

    bool World::StopRecording(AZStd::vector<AZ::u8>& data)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::StopRecording");
        if (m_recording == nullptr)
        {
            data.clear();
            return false;
        }
        const bool success = Box3D::StopRecording(m_nativeId, *m_recording, data);
        m_recording.reset();
        return success;
    }

    bool World::Draw(const DebugDrawSettings& settings, IDebugRenderer& renderer) const
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::Draw");
        return Box3D::DrawWorld(m_nativeId, settings, renderer);
    }

    bool World::RebuildStaticTree()
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::RebuildStaticTree");
        return Box3D::RebuildStaticTree(m_nativeId);
    }

    bool World::Explode(const ExplosionConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::Explode");
        return Box3D::Explode(
            m_nativeId,
            configuration.m_position,
            configuration.m_maskBits,
            configuration.m_radius,
            configuration.m_falloff,
            configuration.m_impulsePerArea);
    }

    bool World::ApplyWind(const BodyHandle bodyHandle, const WindConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::ApplyWind");
        const BodySlot* bodySlot = FindBodySlot(bodyHandle);
        if (bodySlot == nullptr)
        {
            return false;
        }
        bool applied = false;
        AZ::u32 shapeIndex = bodySlot->m_firstShapeIndex;
        while (shapeIndex != InvalidSlotIndex)
        {
            const ShapeSlot& shapeSlot = m_shapeSlots[shapeIndex];
            applied = Box3D::ApplyWind(
                          shapeSlot.m_nativeId,
                          configuration.m_velocity,
                          configuration.m_drag,
                          configuration.m_lift,
                          configuration.m_maximumSpeed,
                          configuration.m_wake) ||
                applied;
            shapeIndex = shapeSlot.m_nextShapeIndex;
        }
        return applied;
    }

    World::BodySlot* World::FindBodySlot(const BodyHandle bodyHandle)
    {
        return const_cast<BodySlot*>(static_cast<const World&>(*this).FindBodySlot(bodyHandle));
    }

    const World::BodySlot* World::FindBodySlot(const BodyHandle bodyHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(bodyHandle, parts) || parts.m_worldIndex != m_worldIndex ||
            parts.m_index >= m_bodySlots.size())
        {
            return nullptr;
        }
        const BodySlot& slot = m_bodySlots[parts.m_index];
        return slot.m_generation == parts.m_generation && Box3D::IsValid(slot.m_nativeId) ? &slot : nullptr;
    }

    World::ShapeSlot* World::FindShapeSlot(const ShapeHandle shapeHandle)
    {
        return const_cast<ShapeSlot*>(static_cast<const World&>(*this).FindShapeSlot(shapeHandle));
    }

    const World::ShapeSlot* World::FindShapeSlot(const ShapeHandle shapeHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(shapeHandle, parts) || parts.m_worldIndex != m_worldIndex ||
            parts.m_index >= m_shapeSlots.size())
        {
            return nullptr;
        }
        const ShapeSlot& slot = m_shapeSlots[parts.m_index];
        return slot.m_generation == parts.m_generation && Box3D::IsValid(slot.m_nativeId) ? &slot : nullptr;
    }

    bool World::ResolveShape(void* userData, const ShapeId nativeId, ResolvedShape& resolvedShape) const
    {
        AZ::u32 shapeIndex = 0;
        if (!DecodeSlotIndex(userData, shapeIndex) || shapeIndex >= m_shapeSlots.size())
        {
            return false;
        }

        const ShapeSlot& slot = m_shapeSlots[shapeIndex];
        if (slot.m_nativeId != nativeId || slot.m_generation == 0)
        {
            return false;
        }
        resolvedShape = { &slot, Internal::MakeWorldMemberHandle<ShapeHandle>(m_worldIndex, shapeIndex, slot.m_generation), shapeIndex };
        return true;
    }

    bool World::ResolveShape(const ShapeId nativeId, ResolvedShape& resolvedShape) const
    {
        const AZ::u32 nativeIndex = nativeId.GetIndex();
        if (nativeIndex >= m_nativeShapeSlots.size())
        {
            return false;
        }

        const AZ::u32 shapeIndex = m_nativeShapeSlots[nativeIndex];
        if (shapeIndex == InvalidSlotIndex || shapeIndex >= m_shapeSlots.size())
        {
            return false;
        }
        return ResolveShape(EncodeSlotIndex(shapeIndex), nativeId, resolvedShape);
    }

    void World::RegisterShape(const ShapeId nativeId, const AZ::u32 shapeIndex)
    {
        const AZ::u32 nativeIndex = nativeId.GetIndex();
        if (nativeIndex == AZStd::numeric_limits<AZ::u32>::max())
        {
            return;
        }
        if (nativeIndex >= m_nativeShapeSlots.size())
        {
            m_nativeShapeSlots.resize(static_cast<size_t>(nativeIndex) + 1, InvalidSlotIndex);
        }
        m_nativeShapeSlots[nativeIndex] = shapeIndex;
    }

    void World::UnregisterShape(const ShapeId nativeId)
    {
        const AZ::u32 nativeIndex = nativeId.GetIndex();
        if (nativeIndex >= m_nativeShapeSlots.size())
        {
            return;
        }

        const AZ::u32 shapeIndex = m_nativeShapeSlots[nativeIndex];
        if (shapeIndex < m_shapeSlots.size() && m_shapeSlots[shapeIndex].m_nativeId == nativeId)
        {
            m_nativeShapeSlots[nativeIndex] = InvalidSlotIndex;
        }
    }

    World::JointSlot* World::FindJointSlot(const JointHandle jointHandle)
    {
        return const_cast<JointSlot*>(static_cast<const World&>(*this).FindJointSlot(jointHandle));
    }

    const World::JointSlot* World::FindJointSlot(const JointHandle jointHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(jointHandle, parts) || parts.m_worldIndex != m_worldIndex ||
            parts.m_index >= m_jointSlots.size())
        {
            return nullptr;
        }
        const JointSlot& slot = m_jointSlots[parts.m_index];
        return slot.m_generation == parts.m_generation && Box3D::IsValid(slot.m_nativeId) ? &slot : nullptr;
    }

    World::CharacterSlot* World::FindCharacterSlot(const CharacterHandle characterHandle, AZ::u32* characterIndex)
    {
        return const_cast<CharacterSlot*>(static_cast<const World&>(*this).FindCharacterSlot(characterHandle, characterIndex));
    }

    const World::CharacterSlot* World::FindCharacterSlot(const CharacterHandle characterHandle, AZ::u32* characterIndex) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(characterHandle, parts) || parts.m_worldIndex != m_worldIndex ||
            parts.m_index >= m_characterSlots.size())
        {
            return nullptr;
        }
        const CharacterSlot& slot = m_characterSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || slot.m_generation == 0)
        {
            return nullptr;
        }
        if (characterIndex != nullptr)
        {
            *characterIndex = parts.m_index;
        }
        return &slot;
    }

    ShapeId World::CreateNativeShape(
        AZ::u32 shapeIndex,
        BodyId bodyId,
        ShapeSlot& shapeSlot,
        ShapeResources& resources,
        const ShapeProperties& properties,
        const ShapeGeometry* geometry,
        const NativeGeometry* cookedGeometry,
        float uniformScale)
    {
        if (!IsValidTransform(properties.m_localTransform) || !AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f ||
            !IsNonNegative(properties.m_density) || !IsNonNegative(properties.m_explosionScale) ||
            (geometry == nullptr) == (cookedGeometry == nullptr))
        {
            return {};
        }

        AZStd::vector<SurfaceMaterial> nativeMaterials;
        nativeMaterials.reserve(properties.m_materials.size());
        for (MaterialHandle materialHandle : properties.m_materials)
        {
            MaterialConfiguration ignored;
            if (!m_system.GetMaterial(materialHandle, ignored))
            {
                return {};
            }
            nativeMaterials.push_back(m_system.ResolveMaterial(materialHandle));
        }

        NativeShapeConfiguration nativeConfiguration;
        nativeConfiguration.m_materials = nativeMaterials;
        if (!nativeMaterials.empty())
        {
            nativeConfiguration.m_baseMaterial = nativeMaterials.front();
            MaterialConfiguration baseMaterial;
            [[maybe_unused]] const bool materialFound = m_system.GetMaterial(properties.m_materials.front(), baseMaterial);
            nativeConfiguration.m_density = baseMaterial.m_density;
            nativeConfiguration.m_explosionScale = baseMaterial.m_explosionScale;
        }
        else
        {
            nativeConfiguration.m_density = properties.m_density;
            nativeConfiguration.m_explosionScale = properties.m_explosionScale;
        }
        nativeConfiguration.m_categoryBits = properties.m_collisionFilter.m_categoryBits;
        nativeConfiguration.m_maskBits = properties.m_collisionFilter.m_maskBits;
        nativeConfiguration.m_groupIndex = properties.m_collisionFilter.m_groupIndex;
        nativeConfiguration.m_userData = EncodeSlotIndex(shapeIndex);
        nativeConfiguration.m_isSensor = properties.m_isSensor;
        nativeConfiguration.m_enableSensorEvents = properties.m_enableSensorEvents;
        nativeConfiguration.m_enableContactEvents = properties.m_enableContactEvents;
        nativeConfiguration.m_enableHitEvents = properties.m_enableHitEvents;
        nativeConfiguration.m_enableCustomFiltering = properties.m_enableCustomFiltering;
        nativeConfiguration.m_enablePreSolveEvents = properties.m_enablePreSolveEvents;
        nativeConfiguration.m_invokeContactCreation = properties.m_createContactsImmediately;
        nativeConfiguration.m_updateBodyMass = properties.m_updateBodyMass;
        resources.m_materials = properties.m_materials;

        resources.m_geometry = cookedGeometry != nullptr
            ? *cookedGeometry
            : CookGeometry(*geometry, GeometryTransform{ properties.m_localTransform, uniformScale }, nativeMaterials);
        shapeSlot.m_isCompound = AZStd::holds_alternative<AZStd::shared_ptr<const CompoundGeometry>>(resources.m_geometry);
        return Box3D::CreateShape(bodyId, nativeConfiguration, resources.m_geometry);
    }

    bool World::RecreateShapeWithMaterials(
        const ShapeHandle shapeHandle,
        ShapeSlot& shapeSlot,
        ShapeResources& resources,
        const AZStd::span<const SurfaceMaterial> materials,
        const float density,
        const float explosionScale,
        AZStd::span<const MaterialHandle> materialHandles)
    {
        Internal::WorldMemberHandleParts shapeParts;
        const BodySlot* bodySlot = FindBodySlot(shapeSlot.m_bodyHandle);
        NativeShapeState state;
        if (bodySlot == nullptr || materials.empty() || !Internal::DecodeWorldMemberHandle(shapeHandle, shapeParts) ||
            !Box3D::GetShapeState(shapeSlot.m_nativeId, state))
        {
            return false;
        }

        NativeShapeConfiguration configuration;
        configuration.m_materials = materials;
        configuration.m_baseMaterial = materials.front();
        configuration.m_categoryBits = state.m_categoryBits;
        configuration.m_maskBits = state.m_maskBits;
        configuration.m_groupIndex = state.m_groupIndex;
        configuration.m_userData = EncodeSlotIndex(shapeParts.m_index);
        configuration.m_density = density;
        configuration.m_explosionScale = explosionScale;
        configuration.m_isSensor = state.m_isSensor;
        configuration.m_enableCustomFiltering = shapeSlot.m_enableCustomFiltering;
        configuration.m_enableSensorEvents = state.m_enableSensorEvents;
        configuration.m_enableContactEvents = state.m_enableContactEvents;
        configuration.m_enableHitEvents = state.m_enableHitEvents;
        configuration.m_enablePreSolveEvents = state.m_enablePreSolveEvents;
        configuration.m_updateBodyMass = false;

        ShapeSlot replacement = shapeSlot;
        ShapeResources replacementResources = resources;
        if (const auto* compound = AZStd::get_if<AZStd::shared_ptr<const CompoundGeometry>>(&resources.m_geometry))
        {
            if (*compound == nullptr)
            {
                return false;
            }
            AZStd::shared_ptr<const CompoundGeometry> replacementGeometry = (*compound)->CloneWithMaterials(materials);
            if (replacementGeometry == nullptr)
            {
                return false;
            }
            replacementResources.m_geometry = AZStd::move(replacementGeometry);
        }
        replacement.m_nativeId = Box3D::CreateShape(bodySlot->m_nativeId, configuration, replacementResources.m_geometry);
        if (!Box3D::IsValid(replacement.m_nativeId))
        {
            return false;
        }
        replacementResources.m_materials.assign(materialHandles.begin(), materialHandles.end());
        replacement.m_explosionScale = explosionScale;
        replacement.m_isCompound = AZStd::holds_alternative<AZStd::shared_ptr<const CompoundGeometry>>(replacementResources.m_geometry);
        m_retiredShapes.push_back({ shapeSlot.m_nativeId, shapeSlot.m_bodyHandle, shapeHandle });
        UnregisterShape(shapeSlot.m_nativeId);
        Box3D::DestroyShape(shapeSlot.m_nativeId, false);
        shapeSlot = AZStd::move(replacement);
        resources = AZStd::move(replacementResources);
        RegisterShape(shapeSlot.m_nativeId, shapeParts.m_index);
        Box3D::ApplyMassFromShapes(bodySlot->m_nativeId);
        return true;
    }

    bool World::GetCompoundSourceMaterials(
        const ShapeSlot& shapeSlot, const ShapeResources& resources, AZStd::vector<SurfaceMaterial>& materials) const
    {
        const auto* compound = AZStd::get_if<AZStd::shared_ptr<const CompoundGeometry>>(&resources.m_geometry);
        if (compound == nullptr || *compound == nullptr)
        {
            return false;
        }
        const AZStd::span<const AZ::u8> materialSources = (*compound)->GetMaterialSources();
        const size_t nativeMaterialCount = Box3D::GetShapeMaterialCount(shapeSlot.m_nativeId);
        if (materialSources.size() != nativeMaterialCount)
        {
            return false;
        }

        size_t sourceMaterialCount = resources.m_materials.size();
        for (AZ::u8 sourceIndex : materialSources)
        {
            sourceMaterialCount = AZStd::max(sourceMaterialCount, aznumeric_cast<size_t>(sourceIndex) + 1);
        }
        materials.assign(sourceMaterialCount, SurfaceMaterial{});
        for (size_t sourceIndex = 0; sourceIndex < resources.m_materials.size(); ++sourceIndex)
        {
            MaterialConfiguration ignored;
            if (!m_system.GetMaterial(resources.m_materials[sourceIndex], ignored))
            {
                return false;
            }
            materials[sourceIndex] = m_system.ResolveMaterial(resources.m_materials[sourceIndex]);
        }

        AZStd::vector<SurfaceMaterial> nativeMaterials(nativeMaterialCount);
        if (!Box3D::GetShapeMaterials(shapeSlot.m_nativeId, nativeMaterials))
        {
            return false;
        }
        for (size_t materialIndex = 0; materialIndex < nativeMaterials.size(); ++materialIndex)
        {
            materials[materialSources[materialIndex]] = nativeMaterials[materialIndex];
        }
        return !materials.empty();
    }

    BodyHandle World::GetBodyHandle(const BodyMove& bodyMove) const
    {
        AZ::u32 bodyIndex = 0;
        if (!DecodeSlotIndex(bodyMove.m_bodyUserData, bodyIndex) || bodyIndex >= m_bodySlots.size())
        {
            return {};
        }
        const BodySlot& slot = m_bodySlots[bodyIndex];
        return Box3D::IsValid(slot.m_nativeId) && slot.m_nativeId == bodyMove.m_bodyId
            ? Internal::MakeWorldMemberHandle<BodyHandle>(m_worldIndex, bodyIndex, slot.m_generation)
            : BodyHandle{};
    }

    BodyHandle World::GetBodyHandle(const ShapeData& shapeData) const
    {
        AZ::u32 bodyIndex = 0;
        if (!DecodeSlotIndex(shapeData.m_bodyUserData, bodyIndex) || bodyIndex >= m_bodySlots.size())
        {
            return {};
        }
        const BodySlot& slot = m_bodySlots[bodyIndex];
        return Box3D::IsValid(slot.m_nativeId) && slot.m_nativeId == shapeData.m_bodyId
            ? Internal::MakeWorldMemberHandle<BodyHandle>(m_worldIndex, bodyIndex, slot.m_generation)
            : BodyHandle{};
    }

    ShapeHandle World::GetShapeHandle(const ShapeData& shapeData) const
    {
        ResolvedShape resolvedShape;
        return ResolveShape(shapeData.m_shapeUserData, shapeData.m_shapeId, resolvedShape) ? resolvedShape.m_shapeHandle : ShapeHandle{};
    }

    bool World::ResolveShapeHandles(const ShapeId nativeId, BodyHandle& bodyHandle, ShapeHandle& shapeHandle) const
    {
        ShapeData shapeData;
        if (Box3D::GetShapeData(nativeId, shapeData))
        {
            bodyHandle = GetBodyHandle(shapeData);
            shapeHandle = GetShapeHandle(shapeData);
            if (bodyHandle && shapeHandle)
            {
                return true;
            }
        }

        const auto retired = AZStd::lower_bound(
            m_retiredShapes.begin(),
            m_retiredShapes.end(),
            nativeId.m_value,
            [](const RetiredShapeProvenance& provenance, const AZ::u64 value)
            {
                return provenance.m_nativeId.m_value < value;
            });
        if (retired == m_retiredShapes.end() || retired->m_nativeId != nativeId)
        {
            return false;
        }
        bodyHandle = retired->m_bodyHandle;
        shapeHandle = retired->m_shapeHandle;
        return true;
    }

    ContactSnapshotResult World::CopyContactSnapshots(
        const AZStd::span<const ContactData> nativeContacts, AZStd::span<ContactSnapshot> contacts, AZStd::span<ContactPoint> points) const
    {
        ContactSnapshotResult result;
        for (const ContactData& nativeContact : nativeContacts)
        {
            const ShapeData shapeA{
                nativeContact.m_shapeIdA, nativeContact.m_bodyIdA, nativeContact.m_shapeUserDataA, nativeContact.m_bodyUserDataA
            };
            const ShapeData shapeB{
                nativeContact.m_shapeIdB, nativeContact.m_bodyIdB, nativeContact.m_shapeUserDataB, nativeContact.m_bodyUserDataB
            };
            const BodyHandle bodyA = GetBodyHandle(shapeA);
            const BodyHandle bodyB = GetBodyHandle(shapeB);
            const ShapeHandle shapeHandleA = GetShapeHandle(shapeA);
            const ShapeHandle shapeHandleB = GetShapeHandle(shapeB);
            if (!bodyA || !bodyB || !shapeHandleA || !shapeHandleB)
            {
                continue;
            }

            ++result.m_contacts.m_requiredCount;
            result.m_points.m_requiredCount += nativeContact.m_pointCount;
            if (result.m_contacts.m_count >= contacts.size())
            {
                continue;
            }

            ContactSnapshot& contact = contacts[result.m_contacts.m_count++];
            contact = { bodyA,
                        bodyB,
                        shapeHandleA,
                        shapeHandleB,
                        result.m_points.m_count,
                        0,
                        nativeContact.m_pointCount,
                        nativeContact.m_childIndexA,
                        nativeContact.m_childIndexB };
            const size_t pointCount = AZStd::min(nativeContact.m_pointCount, points.size() - result.m_points.m_count);
            for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
            {
                const NativeContactPoint& nativePoint = m_nativeContactPoints[nativeContact.m_firstPoint + pointIndex];
                points[result.m_points.m_count++] = { nativePoint.m_position,
                                                      nativePoint.m_normal,
                                                      nativePoint.m_impulse,
                                                      nativePoint.m_separation,
                                                      nativePoint.m_triangleIndex };
            }
            contact.m_pointCount = pointCount;
        }
        return result;
    }

    BufferResult World::CopySensorOverlaps(const AZStd::span<const SensorOverlapData> nativeOverlaps, AZStd::span<SensorOverlap> overlaps) const
    {
        BufferResult result;
        for (const SensorOverlapData& nativeOverlap : nativeOverlaps)
        {
            const BodyHandle sensorBody = GetBodyHandle(nativeOverlap.m_sensor);
            const BodyHandle visitorBody = GetBodyHandle(nativeOverlap.m_visitor);
            const ShapeHandle sensorShape = GetShapeHandle(nativeOverlap.m_sensor);
            const ShapeHandle visitorShape = GetShapeHandle(nativeOverlap.m_visitor);
            if (!sensorBody || !visitorBody || !sensorShape || !visitorShape)
            {
                continue;
            }

            if (result.m_count < overlaps.size())
            {
                overlaps[result.m_count++] = { sensorBody, visitorBody, sensorShape, visitorShape };
            }
            ++result.m_requiredCount;
        }
        return result;
    }

    void World::GatherStepEvents(bool append)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::World::GatherStepEvents");
        if (!append)
        {
            m_bodyMoveEvents.clear();
            m_sensorEvents.clear();
            m_contactEvents.clear();
            m_contactPoints.clear();
            m_contactHitEvents.clear();
            m_jointEvents.clear();
            m_entityBodyMoves.clear();
            m_entityJointThresholds.clear();
        }
        if (m_configuration.m_collectedEventTypes == StepEventTypes::None && m_entityBodyCount == 0 && m_entityJointCount == 0)
        {
            m_activeContactIds.clear();
            m_retiredShapes.clear();
            return;
        }
        AZStd::sort(
            m_retiredShapes.begin(),
            m_retiredShapes.end(),
            [](const RetiredShapeProvenance& left, const RetiredShapeProvenance& right)
            {
                return left.m_nativeId.m_value < right.m_nativeId.m_value;
            });
        StepEventFlags nativeFlags = StepEventFlags::None;
        nativeFlags |= (m_configuration.m_collectedEventTypes & StepEventTypes::BodyMoves) != StepEventTypes::None || m_entityBodyCount > 0
            ? StepEventFlags::BodyMoves
            : StepEventFlags::None;
        nativeFlags |= (m_configuration.m_collectedEventTypes & StepEventTypes::Sensors) != StepEventTypes::None ? StepEventFlags::Sensors
                                                                                                                 : StepEventFlags::None;
        nativeFlags |= (m_configuration.m_collectedEventTypes & StepEventTypes::Contacts) != StepEventTypes::None ? StepEventFlags::Contacts
                                                                                                                  : StepEventFlags::None;
        nativeFlags |= (m_configuration.m_collectedEventTypes & StepEventTypes::ContactHits) != StepEventTypes::None
            ? StepEventFlags::ContactHits
            : StepEventFlags::None;
        nativeFlags |=
            (m_configuration.m_collectedEventTypes & StepEventTypes::JointThresholds) != StepEventTypes::None || m_entityJointCount > 0
            ? StepEventFlags::Joints
            : StepEventFlags::None;
        Box3D::GetStepEvents(
            m_nativeId,
            nativeFlags,
            m_nativeBodyMoves,
            m_nativeSensorEvents,
            m_nativeContactEvents,
            m_nativeContactHits,
            m_nativeJointEvents);

        const auto contactIdLess = [](const ContactId& left, const ContactId& right)
        {
            if (left.m_world != right.m_world)
            {
                return left.m_world < right.m_world;
            }
            if (left.m_index != right.m_index)
            {
                return left.m_index < right.m_index;
            }
            return left.m_generation < right.m_generation;
        };
        m_endedContactIds.clear();
        m_endedContactIds.reserve(m_nativeContactEvents.size());
        for (const ContactTransition& transition : m_nativeContactEvents)
        {
            if (transition.m_type == ContactTransitionType::End)
            {
                m_endedContactIds.push_back(transition.m_contactId);
            }
        }
        AZStd::sort(m_endedContactIds.begin(), m_endedContactIds.end(), contactIdLess);

        m_bodyMoveEvents.reserve(m_nativeBodyMoves.size());
        for (const BodyMove& nativeEvent : m_nativeBodyMoves)
        {
            const BodyHandle bodyHandle = GetBodyHandle(nativeEvent);
            if (bodyHandle)
            {
                const BodyMoveEvent event{ bodyHandle, nativeEvent.m_transform, nativeEvent.m_fellAsleep };
                if ((m_configuration.m_collectedEventTypes & StepEventTypes::BodyMoves) != StepEventTypes::None)
                {
                    m_bodyMoveEvents.push_back(event);
                }
                const BodySlot* bodySlot = FindBodySlot(bodyHandle);
                if (bodySlot != nullptr && bodySlot->m_entityId.IsValid())
                {
                    m_entityBodyMoves.push_back({ bodySlot->m_entityId, event });
                }
            }
        }
        AZStd::sort(
            m_bodyMoveEvents.begin(),
            m_bodyMoveEvents.end(),
            [](const BodyMoveEvent& left, const BodyMoveEvent& right)
            {
                return left.m_bodyHandle < right.m_bodyHandle;
            });

        m_sensorEvents.reserve(m_nativeSensorEvents.size());
        for (const SensorTransition& nativeEvent : m_nativeSensorEvents)
        {
            BodyHandle sensorBody;
            BodyHandle visitorBody;
            ShapeHandle sensorShape;
            ShapeHandle visitorShape;
            if (!ResolveShapeHandles(nativeEvent.m_sensorShapeId, sensorBody, sensorShape) ||
                !ResolveShapeHandles(nativeEvent.m_visitorShapeId, visitorBody, visitorShape))
            {
                continue;
            }
            m_sensorEvents.push_back(
                { sensorBody,
                  visitorBody,
                  sensorShape,
                  visitorShape,
                  nativeEvent.m_type == SensorTransitionType::Begin ? EventPhase::Begin : EventPhase::End });
        }
        AZStd::sort(
            m_sensorEvents.begin(),
            m_sensorEvents.end(),
            [](const SensorEvent& left, const SensorEvent& right)
            {
                if (left.m_sensorBody != right.m_sensorBody)
                {
                    return left.m_sensorBody < right.m_sensorBody;
                }
                if (left.m_visitorBody != right.m_visitorBody)
                {
                    return left.m_visitorBody < right.m_visitorBody;
                }
                if (left.m_sensorShape != right.m_sensorShape)
                {
                    return left.m_sensorShape < right.m_sensorShape;
                }
                if (left.m_visitorShape != right.m_visitorShape)
                {
                    return left.m_visitorShape < right.m_visitorShape;
                }
                return left.m_phase < right.m_phase;
            });

        m_contactEvents.reserve(m_activeContactIds.size() + m_nativeContactEvents.size());
        const auto appendContactData = [this](const ContactData& contactData, const EventPhase phase)
        {
            BodyHandle bodyA;
            BodyHandle bodyB;
            ShapeHandle shapeA;
            ShapeHandle shapeB;
            if (!ResolveShapeHandles(contactData.m_shapeIdA, bodyA, shapeA) || !ResolveShapeHandles(contactData.m_shapeIdB, bodyB, shapeB))
            {
                return false;
            }

            ContactEvent event;
            event.m_bodyA = bodyA;
            event.m_bodyB = bodyB;
            event.m_shapeA = shapeA;
            event.m_shapeB = shapeB;
            event.m_phase = phase;
            event.m_firstPoint = aznumeric_cast<AZ::u32>(m_contactPoints.size());
            event.m_childIndexA = contactData.m_childIndexA;
            event.m_childIndexB = contactData.m_childIndexB;
            const size_t pointEnd = contactData.m_firstPoint + contactData.m_pointCount;
            for (size_t pointIndex = contactData.m_firstPoint; pointIndex < pointEnd; ++pointIndex)
            {
                const NativeContactPoint& point = m_nativeContactPoints[pointIndex];
                m_contactPoints.push_back({ point.m_position, point.m_normal, point.m_impulse, point.m_separation, point.m_triangleIndex });
            }
            event.m_pointCount = aznumeric_cast<AZ::u32>(m_contactPoints.size()) - event.m_firstPoint;
            m_contactEvents.push_back(event);
            return true;
        };

        for (auto activeContact = m_activeContactIds.begin(); activeContact != m_activeContactIds.end();)
        {
            const bool ended = AZStd::binary_search(m_endedContactIds.begin(), m_endedContactIds.end(), *activeContact, contactIdLess);
            ContactData contactData;
            if (ended || !Box3D::GetContactData(*activeContact, contactData, m_nativeContactPoints))
            {
                activeContact = m_activeContactIds.erase(activeContact);
                continue;
            }
            appendContactData(contactData, EventPhase::Persist);
            ++activeContact;
        }

        for (const ContactTransition& nativeEvent : m_nativeContactEvents)
        {
            if (nativeEvent.m_type == ContactTransitionType::Begin)
            {
                ContactData contactData;
                if (Box3D::GetContactData(nativeEvent.m_contactId, contactData, m_nativeContactPoints) &&
                    appendContactData(contactData, EventPhase::Begin))
                {
                    m_activeContactIds.push_back(nativeEvent.m_contactId);
                }
                continue;
            }

            BodyHandle bodyA;
            BodyHandle bodyB;
            ShapeHandle shapeA;
            ShapeHandle shapeB;
            if (!ResolveShapeHandles(nativeEvent.m_shapeIdA, bodyA, shapeA) || !ResolveShapeHandles(nativeEvent.m_shapeIdB, bodyB, shapeB))
            {
                continue;
            }
            ContactEvent event;
            event.m_bodyA = bodyA;
            event.m_bodyB = bodyB;
            event.m_shapeA = shapeA;
            event.m_shapeB = shapeB;
            event.m_phase = EventPhase::End;
            event.m_firstPoint = aznumeric_cast<AZ::u32>(m_contactPoints.size());
            m_contactEvents.push_back(event);
        }
        AZStd::sort(m_activeContactIds.begin(), m_activeContactIds.end(), contactIdLess);
        AZStd::sort(
            m_contactEvents.begin(),
            m_contactEvents.end(),
            [](const ContactEvent& left, const ContactEvent& right)
            {
                if (left.m_bodyA != right.m_bodyA)
                {
                    return left.m_bodyA < right.m_bodyA;
                }
                if (left.m_bodyB != right.m_bodyB)
                {
                    return left.m_bodyB < right.m_bodyB;
                }
                if (left.m_shapeA != right.m_shapeA)
                {
                    return left.m_shapeA < right.m_shapeA;
                }
                if (left.m_shapeB != right.m_shapeB)
                {
                    return left.m_shapeB < right.m_shapeB;
                }
                return left.m_phase < right.m_phase;
            });

        m_contactHitEvents.reserve(m_nativeContactHits.size());
        for (const ContactHit& nativeEvent : m_nativeContactHits)
        {
            BodyHandle bodyA;
            BodyHandle bodyB;
            ShapeHandle shapeA;
            ShapeHandle shapeB;
            if (!ResolveShapeHandles(nativeEvent.m_shapeIdA, bodyA, shapeA) || !ResolveShapeHandles(nativeEvent.m_shapeIdB, bodyB, shapeB))
            {
                continue;
            }
            m_contactHitEvents.push_back(
                { bodyA,
                  bodyB,
                  shapeA,
                  shapeB,
                  Internal::HandleAccess::Create<MaterialHandle>(nativeEvent.m_userMaterialIdA),
                  Internal::HandleAccess::Create<MaterialHandle>(nativeEvent.m_userMaterialIdB),
                  nativeEvent.m_position,
                  nativeEvent.m_normal,
                  nativeEvent.m_approachSpeed,
                  nativeEvent.m_triangleIndex,
                  nativeEvent.m_childIndexA,
                  nativeEvent.m_childIndexB });
        }
        AZStd::sort(
            m_contactHitEvents.begin(),
            m_contactHitEvents.end(),
            [](const ContactHitEvent& left, const ContactHitEvent& right)
            {
                if (left.m_bodyA != right.m_bodyA)
                {
                    return left.m_bodyA < right.m_bodyA;
                }
                if (left.m_bodyB != right.m_bodyB)
                {
                    return left.m_bodyB < right.m_bodyB;
                }
                if (left.m_shapeA != right.m_shapeA)
                {
                    return left.m_shapeA < right.m_shapeA;
                }
                return left.m_shapeB < right.m_shapeB;
            });

        m_jointEvents.reserve(m_nativeJointEvents.size());
        for (const NativeJointThresholdEvent& nativeEvent : m_nativeJointEvents)
        {
            AZ::u32 jointIndex = 0;
            if (!DecodeSlotIndex(nativeEvent.m_jointUserData, jointIndex) || jointIndex >= m_jointSlots.size())
            {
                continue;
            }
            const JointSlot& slot = m_jointSlots[jointIndex];
            if (Box3D::IsValid(slot.m_nativeId))
            {
                const JointThresholdEvent event{ Internal::MakeWorldMemberHandle<JointHandle>(
                    m_worldIndex, jointIndex, slot.m_generation) };
                if ((m_configuration.m_collectedEventTypes & StepEventTypes::JointThresholds) != StepEventTypes::None)
                {
                    m_jointEvents.push_back(event);
                }
                if (slot.m_entityId.IsValid())
                {
                    m_entityJointThresholds.push_back({ slot.m_entityId, event });
                }
            }
        }
        AZStd::sort(
            m_jointEvents.begin(),
            m_jointEvents.end(),
            [](const JointThresholdEvent& left, const JointThresholdEvent& right)
            {
                return left.m_jointHandle < right.m_jointHandle;
            });
        m_retiredShapes.clear();
    }

    bool World::ShouldCollide(const ShapeId shapeA, const ShapeId shapeB)
    {
        if (m_collisionFilterCallback == nullptr)
        {
            return true;
        }
        ShapeData dataA;
        ShapeData dataB;
        return Box3D::GetShapeData(shapeA, dataA) && Box3D::GetShapeData(shapeB, dataB) &&
            m_collisionFilterCallback(GetShapeHandle(dataA), GetShapeHandle(dataB), m_contactCallbackUserData);
    }

    bool World::BeforeSolve(const ShapeId shapeA, const ShapeId shapeB, const AZ::Vector3& position, const AZ::Vector3& normal)
    {
        if (m_preSolveCallback == nullptr)
        {
            return true;
        }
        ShapeData dataA;
        ShapeData dataB;
        return Box3D::GetShapeData(shapeA, dataA) && Box3D::GetShapeData(shapeB, dataB) &&
            m_preSolveCallback(GetShapeHandle(dataA), GetShapeHandle(dataB), position, normal, m_contactCallbackUserData);
    }
} // namespace Box3D
