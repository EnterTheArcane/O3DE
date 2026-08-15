/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemConfiguration.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    AZ_CLASS_ALLOCATOR_IMPL(SystemConfiguration, AZ::SystemAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(WorldConfiguration, AZ::SystemAllocator);

    void CollisionGroupId::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<CollisionGroupId>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<CollisionGroupId>()
                ->Field("Value", &CollisionGroupId::m_value);
        }
    }

    void CollisionSubGroupId::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<CollisionSubGroupId>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<CollisionSubGroupId>()
                ->Field("Value", &CollisionSubGroupId::m_value);
        }
    }

    void SubGroupPair::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionSubGroupId::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SubGroupPair>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SubGroupPair>()
                ->Field("First", &SubGroupPair::m_first)
                ->Field("Second", &SubGroupPair::m_second);
        }
    }

    void GroupFilterTableConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        SubGroupPair::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<GroupFilterTableConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<GroupFilterTableConfiguration>()
                ->Field("SubGroupCount", &GroupFilterTableConfiguration::m_subGroupCount)
                ->Field("DisabledPairs", &GroupFilterTableConfiguration::m_disabledPairs);
        }
    }

    void CollisionGroupConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupId::Reflect(context);
        CollisionSubGroupId::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<CollisionGroupConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<CollisionGroupConfiguration>()
                ->Field("GroupId", &CollisionGroupConfiguration::m_groupId)
                ->Field("SubGroupId", &CollisionGroupConfiguration::m_subGroupId);
        }
    }

    void BroadPhaseLayer::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<BroadPhaseLayer>()
                ->Field("Value", &BroadPhaseLayer::m_value);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<BroadPhaseLayer>("JoltBroadPhaseLayer")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "BroadPhaseLayer")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BroadPhaseLayer")
                ->Constructor<>()
                ->Constructor<ValueType>()
                ->Method("GetValue", &BroadPhaseLayer::GetValue)
                ->Method("IsValid", &BroadPhaseLayer::IsValid);
        }
    }

    void ObjectLayer::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ObjectLayer>()
                ->Field("Value", &ObjectLayer::m_value);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ObjectLayer>("JoltObjectLayer")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ObjectLayer")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ObjectLayer")
                ->Constructor<>()
                ->Constructor<ValueType>()
                ->Method("GetValue", &ObjectLayer::GetValue)
                ->Method("IsValid", &ObjectLayer::IsValid);
        }
    }

    void BroadPhaseLayerConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<BroadPhaseLayerConfiguration>()
                ->Field("Name", &BroadPhaseLayerConfiguration::m_name);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<BroadPhaseLayerConfiguration>("BroadPhaseLayerConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("name", JOLT_BEHAVIOR_VALUE_PROPERTY(&BroadPhaseLayerConfiguration::m_name));
        }
    }

    void ObjectLayerConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        BroadPhaseLayer::Reflect(context);
        ObjectLayer::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ObjectLayerConfiguration>()
                ->Field("Name", &ObjectLayerConfiguration::m_name)
                ->Field("CollidesWith", &ObjectLayerConfiguration::m_collidesWith)
                ->Field("BroadPhaseLayer", &ObjectLayerConfiguration::m_broadPhaseLayer);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ObjectLayerConfiguration>("ObjectLayerConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("name", JOLT_BEHAVIOR_VALUE_PROPERTY(&ObjectLayerConfiguration::m_name))
                ->Property(
                    "collidesWith",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&ObjectLayerConfiguration::m_collidesWith))
                ->Property(
                    "broadPhaseLayer",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&ObjectLayerConfiguration::m_broadPhaseLayer));
        }
    }

    void WorldCapacity::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<WorldCapacity>()
                ->Field("MaxBodies", &WorldCapacity::m_maxBodies)
                ->Field("BodyMutexCount", &WorldCapacity::m_bodyMutexCount)
                ->Field("MaxBodyPairs", &WorldCapacity::m_maxBodyPairs)
                ->Field("MaxContactConstraints", &WorldCapacity::m_maxContactConstraints)
                ->Field("MaxJobs", &WorldCapacity::m_maxJobs)
                ->Field("MaxBarriers", &WorldCapacity::m_maxBarriers)
                ->Field("TempAllocatorBytes", &WorldCapacity::m_tempAllocatorBytes);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<WorldCapacity>("WorldCapacity")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("maxBodies", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_maxBodies))
                ->Property("bodyMutexCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_bodyMutexCount))
                ->Property("maxBodyPairs", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_maxBodyPairs))
                ->Property(
                    "maxContactConstraints",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_maxContactConstraints))
                ->Property("maxJobs", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_maxJobs))
                ->Property("maxBarriers", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_maxBarriers))
                ->Property(
                    "tempAllocatorBytes",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldCapacity::m_tempAllocatorBytes));
        }
    }

    void SimulationConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<SimulationConfiguration>()
                ->Field("Baumgarte", &SimulationConfiguration::m_baumgarte)
                ->Field(
                    "BodyPairCacheMaximumDeltaPosition",
                    &SimulationConfiguration::m_bodyPairCacheMaximumDeltaPosition)
                ->Field(
                    "BodyPairCacheMaximumDeltaRotation",
                    &SimulationConfiguration::m_bodyPairCacheMaximumDeltaRotation)
                ->Field(
                    "ContactNormalMaximumDeltaRotation",
                    &SimulationConfiguration::m_contactNormalMaximumDeltaRotation)
                ->Field(
                    "ContactPointPreserveLambdaMaximumDistance",
                    &SimulationConfiguration::m_contactPointPreserveLambdaMaximumDistance)
                ->Field(
                    "InternalEdgeRemovalVertexTolerance",
                    &SimulationConfiguration::m_internalEdgeRemovalVertexTolerance)
                ->Field("LinearCastMaximumPenetration", &SimulationConfiguration::m_linearCastMaximumPenetration)
                ->Field("LinearCastThreshold", &SimulationConfiguration::m_linearCastThreshold)
                ->Field("ManifoldTolerance", &SimulationConfiguration::m_manifoldTolerance)
                ->Field("MaximumPenetrationDistance", &SimulationConfiguration::m_maximumPenetrationDistance)
                ->Field("MinimumVelocityForRestitution", &SimulationConfiguration::m_minimumVelocityForRestitution)
                ->Field("PenetrationSlop", &SimulationConfiguration::m_penetrationSlop)
                ->Field("PointVelocitySleepThreshold", &SimulationConfiguration::m_pointVelocitySleepThreshold)
                ->Field("SpeculativeContactDistance", &SimulationConfiguration::m_speculativeContactDistance)
                ->Field("TimeBeforeSleep", &SimulationConfiguration::m_timeBeforeSleep)
                ->Field("MaximumInFlightBodyPairs", &SimulationConfiguration::m_maximumInFlightBodyPairs)
                ->Field("PositionStepCount", &SimulationConfiguration::m_positionStepCount)
                ->Field("StepListenerBatchSize", &SimulationConfiguration::m_stepListenerBatchSize)
                ->Field("StepListenerBatchesPerJob", &SimulationConfiguration::m_stepListenerBatchesPerJob)
                ->Field("VelocityStepCount", &SimulationConfiguration::m_velocityStepCount)
                ->Field("AllowSleeping", &SimulationConfiguration::m_allowSleeping)
                ->Field("CheckActiveEdges", &SimulationConfiguration::m_checkActiveEdges)
                ->Field("ConstraintWarmStart", &SimulationConfiguration::m_constraintWarmStart)
                ->Field("UseBodyPairContactCache", &SimulationConfiguration::m_useBodyPairContactCache)
                ->Field("UseLargeIslandSplitter", &SimulationConfiguration::m_useLargeIslandSplitter)
                ->Field("UseManifoldReduction", &SimulationConfiguration::m_useManifoldReduction);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SimulationConfiguration>("SimulationConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("baumgarte", JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_baumgarte))
                ->Property(
                    "bodyPairCacheMaximumDeltaPosition",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_bodyPairCacheMaximumDeltaPosition))
                ->Property(
                    "bodyPairCacheMaximumDeltaRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_bodyPairCacheMaximumDeltaRotation))
                ->Property(
                    "contactNormalMaximumDeltaRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_contactNormalMaximumDeltaRotation))
                ->Property(
                    "contactPointPreserveLambdaMaximumDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_contactPointPreserveLambdaMaximumDistance))
                ->Property(
                    "internalEdgeRemovalVertexTolerance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_internalEdgeRemovalVertexTolerance))
                ->Property(
                    "linearCastMaximumPenetration",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_linearCastMaximumPenetration))
                ->Property(
                    "linearCastThreshold",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_linearCastThreshold))
                ->Property(
                    "manifoldTolerance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_manifoldTolerance))
                ->Property(
                    "maximumPenetrationDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_maximumPenetrationDistance))
                ->Property(
                    "minimumVelocityForRestitution",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_minimumVelocityForRestitution))
                ->Property(
                    "penetrationSlop",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_penetrationSlop))
                ->Property(
                    "pointVelocitySleepThreshold",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_pointVelocitySleepThreshold))
                ->Property(
                    "speculativeContactDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_speculativeContactDistance))
                ->Property(
                    "timeBeforeSleep",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_timeBeforeSleep))
                ->Property(
                    "maximumInFlightBodyPairs",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_maximumInFlightBodyPairs))
                ->Property(
                    "positionStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_positionStepCount))
                ->Property(
                    "stepListenerBatchSize",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_stepListenerBatchSize))
                ->Property(
                    "stepListenerBatchesPerJob",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_stepListenerBatchesPerJob))
                ->Property(
                    "velocityStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_velocityStepCount))
                ->Property(
                    "allowSleeping",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_allowSleeping))
                ->Property(
                    "checkActiveEdges",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_checkActiveEdges))
                ->Property(
                    "constraintWarmStart",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_constraintWarmStart))
                ->Property(
                    "useBodyPairContactCache",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_useBodyPairContactCache))
                ->Property(
                    "useLargeIslandSplitter",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_useLargeIslandSplitter))
                ->Property(
                    "useManifoldReduction",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SimulationConfiguration::m_useManifoldReduction));
        }
    }

    void WorldRuntimeConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<WorldRuntimeConfiguration>()
                ->Field("FrictionCombineMode", &WorldRuntimeConfiguration::m_frictionCombineMode)
                ->Field("RestitutionCombineMode", &WorldRuntimeConfiguration::m_restitutionCombineMode)
                ->Field("FixedTimeStep", &WorldRuntimeConfiguration::m_fixedTimeStep)
                ->Field("CollisionStepCount", &WorldRuntimeConfiguration::m_collisionStepCount)
                ->Field("MaximumCatchUpSteps", &WorldRuntimeConfiguration::m_maximumCatchUpSteps)
                ->Field("AutoSimulate", &WorldRuntimeConfiguration::m_autoSimulate)
                ->Field("CollectActivationEvents", &WorldRuntimeConfiguration::m_collectActivationEvents)
                ->Field("CollectContactEvents", &WorldRuntimeConfiguration::m_collectContactEvents)
                ->Field("Enabled", &WorldRuntimeConfiguration::m_enabled);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, Average);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, GeometricMean);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, Maximum);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, Minimum);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MaterialCombineMode, Multiply);

            behaviorContext->Class<WorldRuntimeConfiguration>("WorldRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "frictionCombineMode",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_frictionCombineMode))
                ->Property(
                    "restitutionCombineMode",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_restitutionCombineMode))
                ->Property(
                    "fixedTimeStep",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_fixedTimeStep))
                ->Property(
                    "collisionStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_collisionStepCount))
                ->Property(
                    "maximumCatchUpSteps",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_maximumCatchUpSteps))
                ->Property(
                    "autoSimulate",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_autoSimulate))
                ->Property(
                    "collectActivationEvents",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_collectActivationEvents))
                ->Property(
                    "collectContactEvents",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_collectContactEvents))
                ->Property("enabled", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldRuntimeConfiguration::m_enabled));
        }
    }

    void WorldConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        BroadPhaseLayerConfiguration::Reflect(context);
        ObjectLayerConfiguration::Reflect(context);
        WorldCapacity::Reflect(context);
        WorldRuntimeConfiguration::Reflect(context);
        SimulationConfiguration::Reflect(context);
        WorldPosition::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<WorldConfiguration>()
                ->Field("Name", &WorldConfiguration::m_name)
                ->Field("Origin", &WorldConfiguration::m_origin)
                ->Field("Gravity", &WorldConfiguration::m_gravity)
                ->Field("Capacity", &WorldConfiguration::m_capacity)
                ->Field("Simulation", &WorldConfiguration::m_simulation)
                ->Field("BroadPhaseLayers", &WorldConfiguration::m_broadPhaseLayers)
                ->Field("ObjectLayers", &WorldConfiguration::m_objectLayers)
                ->Field("FrictionCombineMode", &WorldConfiguration::m_frictionCombineMode)
                ->Field("RestitutionCombineMode", &WorldConfiguration::m_restitutionCombineMode)
                ->Field("FixedTimeStep", &WorldConfiguration::m_fixedTimeStep)
                ->Field("CollisionStepCount", &WorldConfiguration::m_collisionStepCount)
                ->Field("MaximumCatchUpSteps", &WorldConfiguration::m_maximumCatchUpSteps)
                ->Field("WorkerCount", &WorldConfiguration::m_workerCount)
                ->Field("AutoSimulate", &WorldConfiguration::m_autoSimulate)
                ->Field("CollectActivationEvents", &WorldConfiguration::m_collectActivationEvents)
                ->Field("CollectContactEvents", &WorldConfiguration::m_collectContactEvents)
                ->Field("Enabled", &WorldConfiguration::m_enabled);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<WorldConfiguration>("JoltWorldConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "WorldConfiguration")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "WorldConfiguration")
                ->Constructor<>()
                ->Property("name", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_name))
                ->Property("origin", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_origin))
                ->Property("gravity", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_gravity))
                ->Property("capacity", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_capacity))
                ->Property("simulation", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_simulation))
                ->Property(
                    "broadPhaseLayers",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_broadPhaseLayers))
                ->Property(
                    "objectLayers",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_objectLayers))
                ->Property(
                    "frictionCombineMode",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_frictionCombineMode))
                ->Property(
                    "restitutionCombineMode",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_restitutionCombineMode))
                ->Property("fixedTimeStep", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_fixedTimeStep))
                ->Property(
                    "collisionStepCount",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_collisionStepCount))
                ->Property(
                    "maximumCatchUpSteps",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_maximumCatchUpSteps))
                ->Property("workerCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_workerCount))
                ->Property("autoSimulate", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_autoSimulate))
                ->Property(
                    "collectActivationEvents",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_collectActivationEvents))
                ->Property(
                    "collectContactEvents",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_collectContactEvents))
                ->Property("enabled", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldConfiguration::m_enabled));
        }
    }

    void SystemConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupConfiguration::Reflect(context);
        GroupFilterTableConfiguration::Reflect(context);
        WorldConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<SystemConfiguration>()
                ->Field("DefaultWorld", &SystemConfiguration::m_defaultWorld)
                ->Field("SoftBodyTriangleThickness", &SystemConfiguration::m_softBodyTriangleThickness)
                ->Field("CreateDefaultWorld", &SystemConfiguration::m_createDefaultWorld);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SystemConfiguration>("JoltSystemConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "SystemConfiguration")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "SystemConfiguration")
                ->Constructor<>()
                ->Property("defaultWorld", JOLT_BEHAVIOR_VALUE_PROPERTY(&SystemConfiguration::m_defaultWorld))
                ->Property(
                    "softBodyTriangleThickness",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SystemConfiguration::m_softBodyTriangleThickness))
                ->Property(
                    "createDefaultWorld",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SystemConfiguration::m_createDefaultWorld));
        }
    }
} // namespace Jolt
