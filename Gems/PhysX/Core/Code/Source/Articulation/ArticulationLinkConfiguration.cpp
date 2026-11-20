/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Source/Articulation/ArticulationLinkConfiguration.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzFramework/Physics/PhysicsSystem.h>
#include <AzFramework/Physics/Shape.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <cmath>

namespace PhysX
{
    namespace
    {
        bool MotorVersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& element)
        {
            float oldLimit = 1.0f;
            const int index = element.FindElement(AZ_CRC_CE("ForceLimit"));
            if (index >= 0 && !element.GetSubElement(index).GetData(oldLimit))
            {
                return false;
            }
            return element.AddElementWithData(context, "LegacyImpulseLimit", AZStd::optional<float>{ oldLimit }) >= 0;
        }

        class JsonArticulationMotorSerializer final : public AZ::BaseJsonSerializer
        {
        public:
            AZ_RTTI(JsonArticulationMotorSerializer, "{60DA5114-5381-49B6-9B6E-86F67A98B0FC}", AZ::BaseJsonSerializer);
            AZ_CLASS_ALLOCATOR(JsonArticulationMotorSerializer, AZ::SystemAllocator);

            AZ::JsonSerializationResult::Result Load(
                void* output,
                const AZ::Uuid& typeId,
                const rapidjson::Value& input,
                AZ::JsonDeserializerContext& context) override
            {
                AZ_Assert(typeId == azrtti_typeid<ArticulationJointMotorProperties>(), "Unexpected motor configuration type.");
                if (!input.IsObject())
                {
                    return context.Report(
                        AZ::JsonSerializationResult::Tasks::ReadField,
                        AZ::JsonSerializationResult::Outcomes::Unsupported,
                        "Motor settings must be a JSON object.");
                }
                auto* motor = static_cast<ArticulationJointMotorProperties*>(output);
                AZ::JsonSerializationResult::ResultCode result(AZ::JsonSerializationResult::Tasks::ReadField);
                result.Combine(ContinueLoadingFromJsonObjectField(&motor->m_useMotor, azrtti_typeid<bool>(), input, "UseMotor", context));
                result.Combine(ContinueLoadingFromJsonObjectField(&motor->m_driveForceLimit, azrtti_typeid<float>(), input, "ForceLimit", context));
                result.Combine(ContinueLoadingFromJsonObjectField(&motor->m_driveStiffness, azrtti_typeid<float>(), input, "Stiffness", context));
                result.Combine(ContinueLoadingFromJsonObjectField(&motor->m_driveDamping, azrtti_typeid<float>(), input, "Damping", context));
                if (result.GetProcessing() != AZ::JsonSerializationResult::Processing::Completed)
                {
                    return context.Report(result, "Failed to load articulation motor settings.");
                }
                const auto legacy = input.FindMember("LegacyImpulseLimit");
                if (legacy == input.MemberEnd())
                {
                    motor->m_legacyDriveImpulseLimit = motor->m_driveForceLimit;
                }
                else if (legacy->value.IsNull())
                {
                    motor->m_legacyDriveImpulseLimit.reset();
                }
                else
                {
                    float oldLimit = 0.0f;
                    const auto loaded = ContinueLoadingFromJsonObjectField(&oldLimit, azrtti_typeid<float>(), input, "LegacyImpulseLimit", context);
                    result.Combine(loaded);
                    if (loaded.GetProcessing() == AZ::JsonSerializationResult::Processing::Completed)
                    {
                        motor->m_legacyDriveImpulseLimit = oldLimit;
                    }
                }
                result.Combine(AZ::JsonSerializationResult::ResultCode(AZ::JsonSerializationResult::Tasks::ReadField, AZ::JsonSerializationResult::Outcomes::Success));
                return context.Report(result, "Loaded articulation motor settings.");
            }

            AZ::JsonSerializationResult::Result Store(
                rapidjson::Value& output,
                const void* input,
                const void* defaults,
                const AZ::Uuid& typeId,
                AZ::JsonSerializerContext& context) override
            {
                AZ_Assert(typeId == azrtti_typeid<ArticulationJointMotorProperties>(), "Unexpected motor configuration type.");
                const auto* motor = static_cast<const ArticulationJointMotorProperties*>(input);
                const ArticulationJointMotorProperties defaultMotor;
                const auto* defaultValue = &defaultMotor;
                if (defaults)
                {
                    defaultValue = static_cast<const ArticulationJointMotorProperties*>(defaults);
                }
                output.SetObject();
                AZ::JsonSerializationResult::ResultCode result(AZ::JsonSerializationResult::Tasks::WriteValue);
                result.Combine(ContinueStoringToJsonObjectField(output, "UseMotor", &motor->m_useMotor, &defaultValue->m_useMotor, azrtti_typeid<bool>(), context));
                result.Combine(ContinueStoringToJsonObjectField(output, "ForceLimit", &motor->m_driveForceLimit, &defaultValue->m_driveForceLimit, azrtti_typeid<float>(), context));
                result.Combine(ContinueStoringToJsonObjectField(output, "Stiffness", &motor->m_driveStiffness, &defaultValue->m_driveStiffness, azrtti_typeid<float>(), context));
                result.Combine(ContinueStoringToJsonObjectField(output, "Damping", &motor->m_driveDamping, &defaultValue->m_driveDamping, azrtti_typeid<float>(), context));
                rapidjson::Value legacy(rapidjson::kNullType);
                if (motor->m_legacyDriveImpulseLimit)
                {
                    result.Combine(ContinueStoring(legacy, &*motor->m_legacyDriveImpulseLimit, nullptr, azrtti_typeid<float>(), context));
                }
                // Always emit the slot so new defaults can be distinguished from old JSON, and null clears reused objects.
                output.AddMember("LegacyImpulseLimit", AZStd::move(legacy), context.GetJsonAllocator());
                result.Combine(AZ::JsonSerializationResult::ResultCode(AZ::JsonSerializationResult::Tasks::WriteValue, AZ::JsonSerializationResult::Outcomes::Success));
                return context.Report(result, "Stored articulation motor settings.");
            }
        };
    }

    AZ_CLASS_ALLOCATOR_IMPL(ArticulationSensorConfiguration, AZ::SystemAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(ArticulationLinkConfiguration, AZ::SystemAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(ArticulationJointMotorProperties, AZ::SystemAllocator);

    void ArticulationSensorConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ArticulationSensorConfiguration>()
                ->Version(1)
                ->Field("Local Position", &ArticulationSensorConfiguration::m_localPosition)
                ->Field("Local Rotation", &ArticulationSensorConfiguration::m_localRotation)
                ->Field("Include Forward Dynamics Forces", &ArticulationSensorConfiguration::m_includeForwardDynamicsForces)
                ->Field("Include Constraint Solver Forces", &ArticulationSensorConfiguration::m_includeConstraintSolverForces)
                ->Field("Use World Frame", &ArticulationSensorConfiguration::m_useWorldFrame);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<ArticulationSensorConfiguration>(QT_TRANSLATE_NOOP("PhysX", "PhysX Articulation Sensor Configuration"), "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ArticulationSensorConfiguration::m_localPosition,
                        QT_TRANSLATE_NOOP("PhysX", "Local Position"),
                        QT_TRANSLATE_NOOP("PhysX", "The local position of the sensor relative to the articulation link"))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ArticulationSensorConfiguration::m_localRotation,
                        QT_TRANSLATE_NOOP("PhysX", "Local Rotation"),
                        QT_TRANSLATE_NOOP("PhysX", "The local rotation of the sensor relative to the articulation link"))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ArticulationSensorConfiguration::m_includeForwardDynamicsForces,
                        QT_TRANSLATE_NOOP("PhysX", "Include Forward Dynamics Forces"),
                        QT_TRANSLATE_NOOP("PhysX", "Whether the output reported by the sensor should include forward dynamics forces"))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ArticulationSensorConfiguration::m_includeConstraintSolverForces,
                        QT_TRANSLATE_NOOP("PhysX", "Include Constraint Solver Forces"),
                        QT_TRANSLATE_NOOP("PhysX", "Whether the output reported by the sensor should include constraint solver forces"))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ArticulationSensorConfiguration::m_useWorldFrame,
                        QT_TRANSLATE_NOOP("PhysX", "Use World Frame"),
                        QT_TRANSLATE_NOOP("PhysX", "If true, the output will be reported in world space, otherwise in the local space of the sensor"));
            }
        }
    }

    void ArticulationJointMotorProperties::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ArticulationJointMotorProperties>()
                ->Version(2, &MotorVersionConverter)
                ->Field("UseMotor", &ArticulationJointMotorProperties::m_useMotor)
                ->Field("ForceLimit", &ArticulationJointMotorProperties::m_driveForceLimit)
                ->Field("Stiffness", &ArticulationJointMotorProperties::m_driveStiffness)
                ->Field("Damping", &ArticulationJointMotorProperties::m_driveDamping)
                ->Field("LegacyImpulseLimit", &ArticulationJointMotorProperties::m_legacyDriveImpulseLimit);


            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<PhysX::ArticulationJointMotorProperties>(QT_TRANSLATE_NOOP("PhysX", "PhysX Joint Motor Configuration"), "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "PhysX")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(nullptr, &PhysX::ArticulationJointMotorProperties::m_useMotor, QT_TRANSLATE_NOOP("PhysX", "Use Motor"), QT_TRANSLATE_NOOP("PhysX", "Enable motor in the joint."))
                    ->DataElement(nullptr, &PhysX::ArticulationJointMotorProperties::m_driveForceLimit, QT_TRANSLATE_NOOP("PhysX", "Force/Torque Limit"), QT_TRANSLATE_NOOP("PhysX", "Maximum motor force in newtons for prismatic joints, or torque in newton-meters for hinge joints. Legacy impulse limits require retuning."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(nullptr, &PhysX::ArticulationJointMotorProperties::m_driveStiffness, QT_TRANSLATE_NOOP("PhysX", "Stiffness Value"), QT_TRANSLATE_NOOP("PhysX", "Stiffness of motor servo."))
                    ->DataElement(nullptr, &PhysX::ArticulationJointMotorProperties::m_driveDamping, QT_TRANSLATE_NOOP("PhysX", "Damping Value"), QT_TRANSLATE_NOOP("PhysX", "Damping of motor servo."))
                    ->DataElement(
                        nullptr,
                        &ArticulationJointMotorProperties::m_legacyDriveImpulseLimit,
                        QT_TRANSLATE_NOOP("PhysX", "Legacy Impulse Limit"),
                        QT_TRANSLATE_NOOP(
                            "PhysX", "Original pre-5.12 limit, retained only as a retuning reference. Clear Legacy PhysX Settings after retuning."))
                    ->Attribute(AZ::Edit::Attributes::ReadOnly, true)
                    ->Attribute(AZ::Edit::Attributes::Visibility, &ArticulationJointMotorProperties::HasLegacyDriveImpulseLimit);
            }
        }
        else if (auto* jsonContext = azrtti_cast<AZ::JsonRegistrationContext*>(context))
        {
            jsonContext->Serializer<JsonArticulationMotorSerializer>()->HandlesType<ArticulationJointMotorProperties>();
        }
    }

    bool ArticulationJointMotorProperties::HasLegacyDriveImpulseLimit() const
    {
        return m_legacyDriveImpulseLimit.has_value();
    }

    bool ArticulationJointFrictionProperties::IsValid() const
    {
        return std::isfinite(m_staticFrictionEffort) && std::isfinite(m_dynamicFrictionEffort)
            && std::isfinite(m_viscousFrictionCoefficient) && m_dynamicFrictionEffort >= 0.0f
            && m_staticFrictionEffort >= m_dynamicFrictionEffort && m_viscousFrictionCoefficient >= 0.0f;
    }

    void ArticulationJointFrictionProperties::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ArticulationJointFrictionProperties>()
                ->Version(1)
                ->Field("StaticFrictionEffort", &ArticulationJointFrictionProperties::m_staticFrictionEffort)
                ->Field("DynamicFrictionEffort", &ArticulationJointFrictionProperties::m_dynamicFrictionEffort)
                ->Field("ViscousFrictionCoefficient", &ArticulationJointFrictionProperties::m_viscousFrictionCoefficient);
            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ArticulationJointFrictionProperties>(
                        QT_TRANSLATE_NOOP("PhysX", "Joint Friction"),
                        QT_TRANSLATE_NOOP("PhysX", "Friction efforts use force or torque units for each joint axis."))
                    ->DataElement(
                        nullptr,
                        &ArticulationJointFrictionProperties::m_staticFrictionEffort,
                        QT_TRANSLATE_NOOP("PhysX", "Static Effort"),
                        QT_TRANSLATE_NOOP("PhysX", "Maximum effort at rest."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        nullptr,
                        &ArticulationJointFrictionProperties::m_dynamicFrictionEffort,
                        QT_TRANSLATE_NOOP("PhysX", "Dynamic Effort"),
                        QT_TRANSLATE_NOOP("PhysX", "Effort opposing motion, no greater than static effort."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        nullptr,
                        &ArticulationJointFrictionProperties::m_viscousFrictionCoefficient,
                        QT_TRANSLATE_NOOP("PhysX", "Viscous Coefficient"),
                        QT_TRANSLATE_NOOP("PhysX", "Friction proportional to joint velocity."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
            }
        }
    }

    void ArticulationLinkConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ArticulationLinkConfiguration, AzPhysics::SimulatedBodyConfiguration>()
                ->Version(2)
                ->Field("Fixed Base", &ArticulationLinkConfiguration::m_isFixedBase)
                ->Field("Self Collide", &ArticulationLinkConfiguration::m_selfCollide)
                ->Field("Mass", &ArticulationLinkConfiguration::m_mass)
                ->Field("Gravity Enabled", &ArticulationLinkConfiguration::m_gravityEnabled)
                ->Field("Linear damping", &ArticulationLinkConfiguration::m_linearDamping)
                ->Field("Angular damping", &ArticulationLinkConfiguration::m_angularDamping)
                ->Field("Sleep threshold", &ArticulationLinkConfiguration::m_sleepMinEnergy)
                ->Field("Start Asleep", &ArticulationLinkConfiguration::m_startAsleep)
                ->Field("Centre of mass offset", &ArticulationLinkConfiguration::m_centerOfMassOffset)
                ->Field("Compute inertia", &ArticulationLinkConfiguration::m_computeInertiaTensor)
                ->Field("Inertia tensor", &ArticulationLinkConfiguration::m_inertiaTensor)
                ->Field("Maximum Angular Velocity", &ArticulationLinkConfiguration::m_maxAngularVelocity)
                ->Field("SolverPositionIterations", &ArticulationLinkConfiguration::m_solverPositionIterations)
                ->Field("SolverVelocityIterations", &ArticulationLinkConfiguration::m_solverVelocityIterations)
                ->Field("Articulation Joint Type", &ArticulationLinkConfiguration::m_articulationJointType)
                ->Field("Local Position", &ArticulationLinkConfiguration::m_localPosition)
                ->Field("Local Rotation", &ArticulationLinkConfiguration::m_localRotation)
                ->Field("Fix Joint Location", &ArticulationLinkConfiguration::m_fixJointLocation)
                ->Field("Auto-calculate Lead frame", &ArticulationLinkConfiguration::m_autoCalculateLeadFrame)
                ->Field("Lead Local Position", &ArticulationLinkConfiguration::m_leadLocalPosition)
                ->Field("Lead Local Rotation", &ArticulationLinkConfiguration::m_LeadLocalRotation)
                ->Field("Display Joint Setup", &ArticulationLinkConfiguration::m_displayJointSetup)
                ->Field("Is Limited", &ArticulationLinkConfiguration::m_isLimited)
                ->Field("Linear Limit Lower", &ArticulationLinkConfiguration::m_linearLimitLower)
                ->Field("Linear Limit Upper", &ArticulationLinkConfiguration::m_linearLimitUpper)
                ->Field("Angular Limit Negative", &ArticulationLinkConfiguration::m_angularLimitNegative)
                ->Field("Angular Limit Positive", &ArticulationLinkConfiguration::m_angularLimitPositive)
                ->Field("Motor configuration", &ArticulationLinkConfiguration::m_motorConfiguration)
                ->Field("Armature", &ArticulationLinkConfiguration::m_armature)
                ->Field("Friction", &ArticulationLinkConfiguration::m_jointFriction)
                ->Field("FrictionParameters", &ArticulationLinkConfiguration::m_jointFrictionParameters)
                ->Field("Sensor Configuration", &ArticulationLinkConfiguration::m_sensorConfigs)
                ->Field("Offset", &ArticulationLinkConfiguration::m_offset);
        }
    }

    bool ArticulationLinkConfiguration::IsNotRootArticulation() const
    {
        return !m_isRootArticulation;
    }

    bool ArticulationLinkConfiguration::HasLegacySettings() const
    {
        return m_jointFriction != 0.0f || !m_sensorConfigs.empty() || m_motorConfiguration.HasLegacyDriveImpulseLimit();
    }

    AZ::u32 ArticulationLinkConfiguration::ClearLegacySettings()
    {
        m_jointFriction = 0.0f;
        m_sensorConfigs.clear();
        m_motorConfiguration.m_legacyDriveImpulseLimit.reset();
        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    void ArticulationLinkConfiguration::WarnLegacySettings(const AZStd::string& entityName, AZ::EntityId entityId) const
    {
        const auto id = entityId.ToString();
        AZ_Warning("PhysX", m_sensorConfigs.empty(),
            "Articulation link '%s' (%s) retains obsolete sensor settings. Update callers to incoming joint force/torque, then clear Legacy PhysX Settings.",
            entityName.c_str(), id.c_str());
        AZ_Warning("PhysX", m_jointFriction == 0.0f,
            "Articulation link '%s' (%s) retains friction coefficient %g. Retune the friction effort parameters, then clear Legacy PhysX Settings.",
            entityName.c_str(), id.c_str(), m_jointFriction);
        if (m_motorConfiguration.m_useMotor && m_motorConfiguration.m_legacyDriveImpulseLimit)
        {
            AZ_Warning("PhysX", false,
                "Articulation link '%s' (%s) retains motor impulse limit %g. Motor limits now use force/torque units. Retune the motor, then clear Legacy PhysX Settings.",
                entityName.c_str(), id.c_str(), *m_motorConfiguration.m_legacyDriveImpulseLimit);
        }
    }

    bool ArticulationLinkConfiguration::HingePropertiesVisible() const
    {
        return m_articulationJointType == ArticulationJointType::Hinge && IsNotRootArticulation();
    }

    bool ArticulationLinkConfiguration::PrismaticPropertiesVisible() const
    {
        return m_articulationJointType == ArticulationJointType::Prismatic && IsNotRootArticulation();
    }

    bool ArticulationLinkConfiguration::IsSingleDofJointType() const
    {
        return HingePropertiesVisible() || PrismaticPropertiesVisible();
    }

    AZ::Crc32 ArticulationLinkConfiguration::GetInertiaVisibility() const
    {
        return !m_computeInertiaTensor ? AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }
} // namespace PhysX
