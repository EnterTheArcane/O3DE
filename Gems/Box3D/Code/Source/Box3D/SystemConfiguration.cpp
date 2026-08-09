/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/SystemConfiguration.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    AZ_CLASS_ALLOCATOR_IMPL(SystemConfiguration, AZ::SystemAllocator);

    void SystemConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Enum<StepEventTypes>()
                ->Value("None", StepEventTypes::None)
                ->Value("BodyMoves", StepEventTypes::BodyMoves)
                ->Value("Sensors", StepEventTypes::Sensors)
                ->Value("Contacts", StepEventTypes::Contacts)
                ->Value("ContactHits", StepEventTypes::ContactHits)
                ->Value("JointThresholds", StepEventTypes::JointThresholds)
                ->Value("All", StepEventTypes::All);
            serializeContext->Class<WorldConfiguration>()
                ->Version(0)
                ->Field("Name", &WorldConfiguration::m_name)
                ->Field("Gravity", &WorldConfiguration::m_gravity)
                ->Field("FixedTimeStep", &WorldConfiguration::m_fixedTimeStep)
                ->Field("MaximumCatchUpSteps", &WorldConfiguration::m_maximumCatchUpSteps)
                ->Field("CollectedEventTypes", &WorldConfiguration::m_collectedEventTypes)
                ->Field("AutoSimulate", &WorldConfiguration::m_autoSimulate)
                ->Field("Enabled", &WorldConfiguration::m_enabled);
            serializeContext->Class<SystemConfiguration>()
                ->Version(3)
                ->Field("SubStepCount", &SystemConfiguration::m_subStepCount)
                ->Field("WorkerCount", &SystemConfiguration::m_workerCount)
                ->Field("ContactHertz", &SystemConfiguration::m_contactHertz)
                ->Field("ContactDampingRatio", &SystemConfiguration::m_contactDampingRatio)
                ->Field("ContactSpeed", &SystemConfiguration::m_contactSpeed)
                ->Field("ContactRecycleDistance", &SystemConfiguration::m_contactRecycleDistance)
                ->Field("RestitutionThreshold", &SystemConfiguration::m_restitutionThreshold)
                ->Field("HitEventThreshold", &SystemConfiguration::m_hitEventThreshold)
                ->Field("MaximumLinearSpeed", &SystemConfiguration::m_maximumLinearSpeed)
                ->Field("LengthUnitsPerMeter", &SystemConfiguration::m_lengthUnitsPerMeter)
                ->Field("StallWarningThresholdSeconds", &SystemConfiguration::m_stallWarningThresholdSeconds)
                ->Field("StaticShapeCapacity", &SystemConfiguration::m_staticShapeCapacity)
                ->Field("DynamicShapeCapacity", &SystemConfiguration::m_dynamicShapeCapacity)
                ->Field("StaticBodyCapacity", &SystemConfiguration::m_staticBodyCapacity)
                ->Field("DynamicBodyCapacity", &SystemConfiguration::m_dynamicBodyCapacity)
                ->Field("ContactCapacity", &SystemConfiguration::m_contactCapacity)
                ->Field("EnableSleep", &SystemConfiguration::m_enableSleep)
                ->Field("EnableContinuous", &SystemConfiguration::m_enableContinuous)
                ->Field("EnableWarmStarting", &SystemConfiguration::m_enableWarmStarting)
                ->Field("EnableSpeculative", &SystemConfiguration::m_enableSpeculative);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SystemConfiguration>("Box3D settings", "Solver and capacity settings")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_subStepCount, "Sub-steps", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_workerCount, "Workers", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_contactHertz, "Contact hertz", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_contactDampingRatio, "Contact damping ratio", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_contactSpeed, "Contact speed", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_contactRecycleDistance, "Contact recycle distance", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_restitutionThreshold,
                        "Restitution threshold",
                        "Relative speed below which restitution is disabled.")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_hitEventThreshold, "Hit event threshold", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_maximumLinearSpeed, "Maximum linear speed", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_lengthUnitsPerMeter,
                        "Length units per meter",
                        "Simulation length units represented by one meter. The default uses meters directly.")
                    ->Attribute(AZ::Edit::Attributes::Min, AZ::Constants::FloatEpsilon)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_stallWarningThresholdSeconds,
                        "Stall warning threshold",
                        "Seconds before continuous-collision stalls are reported. The maximum float disables reporting.")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_staticShapeCapacity,
                        "Static shape capacity",
                        "Initial static broad-phase capacity. Zero selects the native default.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_dynamicShapeCapacity,
                        "Dynamic shape capacity",
                        "Initial dynamic broad-phase capacity. Zero selects the native default.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_staticBodyCapacity,
                        "Static body capacity",
                        "Initial static body capacity. Zero selects the native default.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_dynamicBodyCapacity,
                        "Dynamic body capacity",
                        "Initial dynamic and kinematic body capacity. Zero selects the native default.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemConfiguration::m_contactCapacity,
                        "Contact capacity",
                        "Initial contact capacity. Zero selects the native default.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_enableSleep, "Sleeping", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_enableContinuous, "Continuous collision", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_enableWarmStarting, "Warm starting", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemConfiguration::m_enableSpeculative, "Speculative collision", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Enum<static_cast<AZ::u8>(StepEventTypes::None)>("StepEventTypes_None")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::BodyMoves)>("StepEventTypes_BodyMoves")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::Sensors)>("StepEventTypes_Sensors")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::Contacts)>("StepEventTypes_Contacts")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::ContactHits)>("StepEventTypes_ContactHits")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::JointThresholds)>("StepEventTypes_JointThresholds")
                ->Enum<static_cast<AZ::u8>(StepEventTypes::All)>("StepEventTypes_All");
            behaviorContext->Class<WorldConfiguration>("WorldConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("name", BehaviorValueProperty(&WorldConfiguration::m_name))
                ->Property("gravity", BehaviorValueProperty(&WorldConfiguration::m_gravity))
                ->Property("fixedTimeStep", BehaviorValueProperty(&WorldConfiguration::m_fixedTimeStep))
                ->Property("maximumCatchUpSteps", BehaviorValueProperty(&WorldConfiguration::m_maximumCatchUpSteps))
                ->Property("collectedEventTypes", BehaviorValueProperty(&WorldConfiguration::m_collectedEventTypes))
                ->Property("autoSimulate", BehaviorValueProperty(&WorldConfiguration::m_autoSimulate))
                ->Property("enabled", BehaviorValueProperty(&WorldConfiguration::m_enabled));
            behaviorContext->Class<SystemConfiguration>("SystemConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("subStepCount", BehaviorValueProperty(&SystemConfiguration::m_subStepCount))
                ->Property("workerCount", BehaviorValueProperty(&SystemConfiguration::m_workerCount))
                ->Property("contactHertz", BehaviorValueProperty(&SystemConfiguration::m_contactHertz))
                ->Property("contactDampingRatio", BehaviorValueProperty(&SystemConfiguration::m_contactDampingRatio))
                ->Property("contactSpeed", BehaviorValueProperty(&SystemConfiguration::m_contactSpeed))
                ->Property("contactRecycleDistance", BehaviorValueProperty(&SystemConfiguration::m_contactRecycleDistance))
                ->Property("restitutionThreshold", BehaviorValueProperty(&SystemConfiguration::m_restitutionThreshold))
                ->Property("hitEventThreshold", BehaviorValueProperty(&SystemConfiguration::m_hitEventThreshold))
                ->Property("maximumLinearSpeed", BehaviorValueProperty(&SystemConfiguration::m_maximumLinearSpeed))
                ->Property("lengthUnitsPerMeter", BehaviorValueProperty(&SystemConfiguration::m_lengthUnitsPerMeter))
                ->Property("stallWarningThresholdSeconds", BehaviorValueProperty(&SystemConfiguration::m_stallWarningThresholdSeconds))
                ->Property("staticShapeCapacity", BehaviorValueProperty(&SystemConfiguration::m_staticShapeCapacity))
                ->Property("dynamicShapeCapacity", BehaviorValueProperty(&SystemConfiguration::m_dynamicShapeCapacity))
                ->Property("staticBodyCapacity", BehaviorValueProperty(&SystemConfiguration::m_staticBodyCapacity))
                ->Property("dynamicBodyCapacity", BehaviorValueProperty(&SystemConfiguration::m_dynamicBodyCapacity))
                ->Property("contactCapacity", BehaviorValueProperty(&SystemConfiguration::m_contactCapacity))
                ->Property("enableSleep", BehaviorValueProperty(&SystemConfiguration::m_enableSleep))
                ->Property("enableContinuous", BehaviorValueProperty(&SystemConfiguration::m_enableContinuous))
                ->Property("enableWarmStarting", BehaviorValueProperty(&SystemConfiguration::m_enableWarmStarting))
                ->Property("enableSpeculative", BehaviorValueProperty(&SystemConfiguration::m_enableSpeculative));
        }
    }

    bool SystemConfiguration::operator==(const SystemConfiguration& other) const
    {
        return m_subStepCount == other.m_subStepCount && m_workerCount == other.m_workerCount && m_contactHertz == other.m_contactHertz &&
            m_contactDampingRatio == other.m_contactDampingRatio && m_contactSpeed == other.m_contactSpeed &&
            m_contactRecycleDistance == other.m_contactRecycleDistance && m_restitutionThreshold == other.m_restitutionThreshold &&
            m_hitEventThreshold == other.m_hitEventThreshold && m_maximumLinearSpeed == other.m_maximumLinearSpeed &&
            m_lengthUnitsPerMeter == other.m_lengthUnitsPerMeter &&
            m_stallWarningThresholdSeconds == other.m_stallWarningThresholdSeconds &&
            m_staticShapeCapacity == other.m_staticShapeCapacity && m_dynamicShapeCapacity == other.m_dynamicShapeCapacity &&
            m_staticBodyCapacity == other.m_staticBodyCapacity && m_dynamicBodyCapacity == other.m_dynamicBodyCapacity &&
            m_contactCapacity == other.m_contactCapacity && m_frictionCallback == other.m_frictionCallback &&
            m_restitutionCallback == other.m_restitutionCallback && m_enableSleep == other.m_enableSleep &&
            m_enableContinuous == other.m_enableContinuous && m_enableWarmStarting == other.m_enableWarmStarting &&
            m_enableSpeculative == other.m_enableSpeculative;
    }

    bool SystemConfiguration::operator!=(const SystemConfiguration& other) const
    {
        return !(*this == other);
    }
} // namespace Box3D
