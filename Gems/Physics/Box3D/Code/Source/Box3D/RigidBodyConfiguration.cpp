/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/RigidBodyConfiguration.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    void RigidBodyConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Enum<BodyType>()
                ->Value("Static", BodyType::Static)
                ->Value("Kinematic", BodyType::Kinematic)
                ->Value("Dynamic", BodyType::Dynamic);

            serializeContext
                ->Class<MotionLocks>()
                ->Field("LinearX", &MotionLocks::m_linearX)
                ->Field("LinearY", &MotionLocks::m_linearY)
                ->Field("LinearZ", &MotionLocks::m_linearZ)
                ->Field("AngularX", &MotionLocks::m_angularX)
                ->Field("AngularY", &MotionLocks::m_angularY)
                ->Field("AngularZ", &MotionLocks::m_angularZ);

            serializeContext
                ->Class<RigidBodyConfiguration>()
                ->Field("Transform", &RigidBodyConfiguration::m_transform)
                ->Field("LinearVelocity", &RigidBodyConfiguration::m_linearVelocity)
                ->Field("AngularVelocity", &RigidBodyConfiguration::m_angularVelocity)
                ->Field("EntityId", &RigidBodyConfiguration::m_entityId)
                ->Field("Name", &RigidBodyConfiguration::m_name)
                ->Field("LinearDamping", &RigidBodyConfiguration::m_linearDamping)
                ->Field("AngularDamping", &RigidBodyConfiguration::m_angularDamping)
                ->Field("GravityScale", &RigidBodyConfiguration::m_gravityScale)
                ->Field("SleepThreshold", &RigidBodyConfiguration::m_sleepThreshold)
                ->Field("MotionLocks", &RigidBodyConfiguration::m_motionLocks)
                ->Field("BodyType", &RigidBodyConfiguration::m_bodyType)
                ->Field("EnableSleep", &RigidBodyConfiguration::m_enableSleep)
                ->Field("StartAwake", &RigidBodyConfiguration::m_startAwake)
                ->Field("IsBullet", &RigidBodyConfiguration::m_isBullet)
                ->Field("IsEnabled", &RigidBodyConfiguration::m_isEnabled)
                ->Field("AllowFastRotation", &RigidBodyConfiguration::m_allowFastRotation)
                ->Field("EnableContactRecycling", &RigidBodyConfiguration::m_enableContactRecycling);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<MotionLocks>("Motion locks", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_linearX, "Linear X", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_linearY, "Linear Y", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_linearZ, "Linear Z", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_angularX, "Angular X", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_angularY, "Angular Y", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MotionLocks::m_angularZ, "Angular Z", "");
                editContext->Class<RigidBodyConfiguration>("Rigid body", "Initial state and simulation policy")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &RigidBodyConfiguration::m_bodyType, "Body type", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_linearVelocity, "Initial linear velocity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_angularVelocity, "Initial angular velocity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_linearDamping, "Linear damping", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_angularDamping, "Angular damping", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_gravityScale, "Gravity scale", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_sleepThreshold, "Sleep threshold", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_motionLocks, "Motion locks", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_enableSleep, "Sleeping", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_startAwake, "Start awake", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_isBullet, "Continuous collision", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_isEnabled, "Enabled", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_allowFastRotation, "Fast rotation", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RigidBodyConfiguration::m_enableContactRecycling, "Contact recycling", "");
            }
        }
    }
} // namespace Box3D
