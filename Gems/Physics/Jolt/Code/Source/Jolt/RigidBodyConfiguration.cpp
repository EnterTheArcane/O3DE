/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/RigidBodyConfiguration.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void RigidBodyConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<RigidBodyConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<MassPropertiesConfiguration>()
                ->Field("Inertia", &MassPropertiesConfiguration::m_inertia)
                ->Field("InertiaMultiplier", &MassPropertiesConfiguration::m_inertiaMultiplier)
                ->Field("Mass", &MassPropertiesConfiguration::m_mass)
                ->Field("Mode", &MassPropertiesConfiguration::m_mode);

            serializeContext
                ->Class<BodyRuntimeConfiguration>()
                ->Field("MassProperties", &BodyRuntimeConfiguration::m_massProperties)
                ->Field("AngularDamping", &BodyRuntimeConfiguration::m_angularDamping)
                ->Field("Friction", &BodyRuntimeConfiguration::m_friction)
                ->Field("GravityFactor", &BodyRuntimeConfiguration::m_gravityFactor)
                ->Field("LinearDamping", &BodyRuntimeConfiguration::m_linearDamping)
                ->Field("MaximumAngularVelocity", &BodyRuntimeConfiguration::m_maximumAngularVelocity)
                ->Field("MaximumLinearVelocity", &BodyRuntimeConfiguration::m_maximumLinearVelocity)
                ->Field("Restitution", &BodyRuntimeConfiguration::m_restitution)
                ->Field("AllowedDofs", &BodyRuntimeConfiguration::m_allowedDofs)
                ->Field("MotionQuality", &BodyRuntimeConfiguration::m_motionQuality)
                ->Field("PositionStepCount", &BodyRuntimeConfiguration::m_positionStepCount)
                ->Field("VelocityStepCount", &BodyRuntimeConfiguration::m_velocityStepCount)
                ->Field("AllowSleeping", &BodyRuntimeConfiguration::m_allowSleeping)
                ->Field("ApplyGyroscopicForce", &BodyRuntimeConfiguration::m_applyGyroscopicForce)
                ->Field("CollideKinematicVsNonDynamic", &BodyRuntimeConfiguration::m_collideKinematicVsNonDynamic)
                ->Field("EnhancedInternalEdgeRemoval", &BodyRuntimeConfiguration::m_enhancedInternalEdgeRemoval)
                ->Field("IsSensor", &BodyRuntimeConfiguration::m_isSensor)
                ->Field("UseManifoldReduction", &BodyRuntimeConfiguration::m_useManifoldReduction);

            serializeContext
                ->Class<BuoyancyConfiguration>()
                ->Field("SurfacePosition", &BuoyancyConfiguration::m_surfacePosition)
                ->Field("FluidVelocity", &BuoyancyConfiguration::m_fluidVelocity)
                ->Field("Gravity", &BuoyancyConfiguration::m_gravity)
                ->Field("SurfaceNormal", &BuoyancyConfiguration::m_surfaceNormal)
                ->Field("AngularDrag", &BuoyancyConfiguration::m_angularDrag)
                ->Field("Buoyancy", &BuoyancyConfiguration::m_buoyancy)
                ->Field("DeltaTime", &BuoyancyConfiguration::m_deltaTime)
                ->Field("LinearDrag", &BuoyancyConfiguration::m_linearDrag);

            serializeContext
                ->Class<RigidBodyConfiguration>()
                ->Field("Runtime", &RigidBodyConfiguration::m_runtime)
                ->Field("InitialLinearVelocity", &RigidBodyConfiguration::m_initialLinearVelocity)
                ->Field("InitialAngularVelocity", &RigidBodyConfiguration::m_initialAngularVelocity)
                ->Field("CollisionGroup", &RigidBodyConfiguration::m_collisionGroup)
                ->Field("UserData", &RigidBodyConfiguration::m_userData)
                ->Field("ObjectLayer", &RigidBodyConfiguration::m_objectLayer)
                ->Field("MotionType", &RigidBodyConfiguration::m_motionType)
                ->Field("Activate", &RigidBodyConfiguration::m_activate)
                ->Field("AllowDynamicOrKinematic", &RigidBodyConfiguration::m_allowDynamicOrKinematic);

        }
    }

    void StaticRigidBodyConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<StaticRigidBodyConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<StaticRigidBodyConfiguration>()
                ->Field("CollisionGroup", &StaticRigidBodyConfiguration::m_collisionGroup)
                ->Field("UserData", &StaticRigidBodyConfiguration::m_userData)
                ->Field("ObjectLayer", &StaticRigidBodyConfiguration::m_objectLayer)
                ->Field("Friction", &StaticRigidBodyConfiguration::m_friction)
                ->Field("Restitution", &StaticRigidBodyConfiguration::m_restitution)
                ->Field("EnhancedInternalEdgeRemoval", &StaticRigidBodyConfiguration::m_enhancedInternalEdgeRemoval)
                ->Field("IsSensor", &StaticRigidBodyConfiguration::m_isSensor)
                ->Field("UseManifoldReduction", &StaticRigidBodyConfiguration::m_useManifoldReduction);
        }
    }
} // namespace Jolt
