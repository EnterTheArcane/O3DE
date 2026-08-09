/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Effects.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    void ExplosionConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ExplosionConfiguration>()
                ->Version(2)
                ->Field("Position", &ExplosionConfiguration::m_position)
                ->Field("MaskBits", &ExplosionConfiguration::m_maskBits)
                ->Field("Radius", &ExplosionConfiguration::m_radius)
                ->Field("Falloff", &ExplosionConfiguration::m_falloff)
                ->Field("ImpulsePerArea", &ExplosionConfiguration::m_impulsePerArea);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<ExplosionConfiguration>("Explosion", "Impulse field parameters")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionConfiguration::m_position, "Position", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionConfiguration::m_maskBits, "Collision mask", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionConfiguration::m_radius, "Radius", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionConfiguration::m_falloff, "Falloff", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionConfiguration::m_impulsePerArea, "Impulse per area", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ExplosionConfiguration>("ExplosionConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("position", BehaviorValueProperty(&ExplosionConfiguration::m_position))
                ->Property("maskBits", BehaviorValueProperty(&ExplosionConfiguration::m_maskBits))
                ->Property("radius", BehaviorValueProperty(&ExplosionConfiguration::m_radius))
                ->Property("falloff", BehaviorValueProperty(&ExplosionConfiguration::m_falloff))
                ->Property("impulsePerArea", BehaviorValueProperty(&ExplosionConfiguration::m_impulsePerArea));
        }
    }

    void WindConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<WindConfiguration>()
                ->Version(0)
                ->Field("Velocity", &WindConfiguration::m_velocity)
                ->Field("Drag", &WindConfiguration::m_drag)
                ->Field("Lift", &WindConfiguration::m_lift)
                ->Field("MaximumSpeed", &WindConfiguration::m_maximumSpeed)
                ->Field("Wake", &WindConfiguration::m_wake);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<WindConfiguration>("Wind", "Aerodynamic force parameters")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_velocity, "Velocity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_drag, "Drag", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_lift, "Lift", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_maximumSpeed, "Maximum speed", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_wake, "Wake", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<WindConfiguration>("WindConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("velocity", BehaviorValueProperty(&WindConfiguration::m_velocity))
                ->Property("drag", BehaviorValueProperty(&WindConfiguration::m_drag))
                ->Property("lift", BehaviorValueProperty(&WindConfiguration::m_lift))
                ->Property("maximumSpeed", BehaviorValueProperty(&WindConfiguration::m_maximumSpeed))
                ->Property("wake", BehaviorValueProperty(&WindConfiguration::m_wake));
        }
    }
} // namespace Box3D
