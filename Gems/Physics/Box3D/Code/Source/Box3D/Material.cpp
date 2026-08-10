/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Material.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    void MaterialConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Enum<DebugMaterialPreset>()
                ->Value("Default", DebugMaterialPreset::Default)
                ->Value("Matte", DebugMaterialPreset::Matte)
                ->Value("Soft", DebugMaterialPreset::Soft)
                ->Value("Dead", DebugMaterialPreset::Dead)
                ->Value("Glossy", DebugMaterialPreset::Glossy)
                ->Value("Metallic", DebugMaterialPreset::Metallic);

            serializeContext
                ->Class<MaterialConfiguration>()
                ->Field("Name", &MaterialConfiguration::m_name)
                ->Field("TangentVelocity", &MaterialConfiguration::m_tangentVelocity)
                ->Field("DebugColor", &MaterialConfiguration::m_debugColor)
                ->Field("SurfaceTypeId", &MaterialConfiguration::m_surfaceTypeId)
                ->Field("Friction", &MaterialConfiguration::m_friction)
                ->Field("Restitution", &MaterialConfiguration::m_restitution)
                ->Field("RollingResistance", &MaterialConfiguration::m_rollingResistance)
                ->Field("Density", &MaterialConfiguration::m_density)
                ->Field("ExplosionScale", &MaterialConfiguration::m_explosionScale)
                ->Field("DebugMaterialPreset", &MaterialConfiguration::m_debugMaterialPreset)
                ->Field("DebugAppearanceEnabled", &MaterialConfiguration::m_debugAppearanceEnabled);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<MaterialConfiguration>("Material", "Surface response and diagnostic appearance")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_name, "Name", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_friction, "Friction", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_restitution, "Restitution", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_rollingResistance, "Rolling resistance", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_tangentVelocity, "Tangent velocity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_density, "Density", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_explosionScale, "Explosion scale", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_surfaceTypeId, "Surface type id", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_debugAppearanceEnabled, "Debug appearance", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialConfiguration::m_debugColor, "Debug color", "")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &MaterialConfiguration::m_debugMaterialPreset, "Debug material preset", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Enum<static_cast<int>(DebugMaterialPreset::Default)>("DebugMaterialPreset_Default")
                ->Enum<static_cast<int>(DebugMaterialPreset::Matte)>("DebugMaterialPreset_Matte")
                ->Enum<static_cast<int>(DebugMaterialPreset::Soft)>("DebugMaterialPreset_Soft")
                ->Enum<static_cast<int>(DebugMaterialPreset::Dead)>("DebugMaterialPreset_Dead")
                ->Enum<static_cast<int>(DebugMaterialPreset::Glossy)>("DebugMaterialPreset_Glossy")
                ->Enum<static_cast<int>(DebugMaterialPreset::Metallic)>("DebugMaterialPreset_Metallic");

            behaviorContext->Class<MaterialConfiguration>("MaterialConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("name", BehaviorValueProperty(&MaterialConfiguration::m_name))
                ->Property("tangentVelocity", BehaviorValueProperty(&MaterialConfiguration::m_tangentVelocity))
                ->Property("debugColor", BehaviorValueProperty(&MaterialConfiguration::m_debugColor))
                ->Property("surfaceTypeId", BehaviorValueProperty(&MaterialConfiguration::m_surfaceTypeId))
                ->Property("friction", BehaviorValueProperty(&MaterialConfiguration::m_friction))
                ->Property("restitution", BehaviorValueProperty(&MaterialConfiguration::m_restitution))
                ->Property("rollingResistance", BehaviorValueProperty(&MaterialConfiguration::m_rollingResistance))
                ->Property("density", BehaviorValueProperty(&MaterialConfiguration::m_density))
                ->Property("explosionScale", BehaviorValueProperty(&MaterialConfiguration::m_explosionScale))
                ->Property("debugMaterialPreset", BehaviorValueProperty(&MaterialConfiguration::m_debugMaterialPreset))
                ->Property("debugAppearanceEnabled", BehaviorValueProperty(&MaterialConfiguration::m_debugAppearanceEnabled));

            behaviorContext->Class<MaterialHandleCollection>("MaterialHandleCollection")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Method("Add", &MaterialHandleCollection::Add)
                ->Method("Clear", &MaterialHandleCollection::Clear)
                ->Method("GetCount", &MaterialHandleCollection::GetCount)
                ->Method("GetAt", &MaterialHandleCollection::GetAt);

            behaviorContext->Class<MaterialConfigurationCollection>("MaterialConfigurationCollection")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Method("Add", &MaterialConfigurationCollection::Add)
                ->Method("Clear", &MaterialConfigurationCollection::Clear)
                ->Method("GetCount", &MaterialConfigurationCollection::GetCount)
                ->Method("GetAt", &MaterialConfigurationCollection::GetAt);
        }
    }
} // namespace Box3D
