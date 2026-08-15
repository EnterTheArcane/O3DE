/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Shape.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ShapeStats::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<ShapeStats>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<ShapeStats>()
                ->Field("LocalBounds", &ShapeStats::m_localBounds)
                ->Field("MemorySize", &ShapeStats::m_memorySize)
                ->Field("TriangleCount", &ShapeStats::m_triangleCount);

            serializeContext
                ->Class<ShapeProperties>()
                ->Field("Inertia", &ShapeProperties::m_inertia)
                ->Field("LocalBounds", &ShapeProperties::m_localBounds)
                ->Field("CenterOfMass", &ShapeProperties::m_centerOfMass)
                ->Field("InnerRadius", &ShapeProperties::m_innerRadius)
                ->Field("Mass", &ShapeProperties::m_mass)
                ->Field("Volume", &ShapeProperties::m_volume)
                ->Field("SubShapeIdBitCount", &ShapeProperties::m_subShapeIdBitCount)
                ->Field("Kind", &ShapeProperties::m_kind)
                ->Field("MustBeStatic", &ShapeProperties::m_mustBeStatic);

            serializeContext
                ->Class<SubmergedVolumeRequest>()
                ->Field("CenterOfMassTransform", &SubmergedVolumeRequest::m_centerOfMassTransform)
                ->Field("SurfacePosition", &SubmergedVolumeRequest::m_surfacePosition)
                ->Field("Scale", &SubmergedVolumeRequest::m_scale)
                ->Field("SurfaceNormal", &SubmergedVolumeRequest::m_surfaceNormal);

            serializeContext
                ->Class<SubmergedVolumeResult>()
                ->Field("CenterOfBuoyancy", &SubmergedVolumeResult::m_centerOfBuoyancy)
                ->Field("SubmergedVolume", &SubmergedVolumeResult::m_submergedVolume)
                ->Field("TotalVolume", &SubmergedVolumeResult::m_totalVolume);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            if (!ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("ShapeStats")))
            {
                return;
            }

            behaviorContext->Class<ShapeStats>("ShapeStats")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("localBounds", BehaviorValueGetter(&ShapeStats::m_localBounds), nullptr)
                ->Property("memorySize", BehaviorValueGetter(&ShapeStats::m_memorySize), nullptr)
                ->Property("triangleCount", BehaviorValueGetter(&ShapeStats::m_triangleCount), nullptr);

            behaviorContext->Class<ShapeProperties>("JoltShapeProperties")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ShapeProperties")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ShapeProperties")
                ->Property("inertia", BehaviorValueGetter(&ShapeProperties::m_inertia), nullptr)
                ->Property("localBounds", BehaviorValueGetter(&ShapeProperties::m_localBounds), nullptr)
                ->Property("centerOfMass", BehaviorValueGetter(&ShapeProperties::m_centerOfMass), nullptr)
                ->Property("innerRadius", BehaviorValueGetter(&ShapeProperties::m_innerRadius), nullptr)
                ->Property("mass", BehaviorValueGetter(&ShapeProperties::m_mass), nullptr)
                ->Property("volume", BehaviorValueGetter(&ShapeProperties::m_volume), nullptr)
                ->Property("subShapeIdBitCount", BehaviorValueGetter(&ShapeProperties::m_subShapeIdBitCount), nullptr)
                ->Property("kind", BehaviorValueGetter(&ShapeProperties::m_kind), nullptr)
                ->Property("mustBeStatic", BehaviorValueGetter(&ShapeProperties::m_mustBeStatic), nullptr);

            behaviorContext->Class<SubmergedVolumeRequest>("SubmergedVolumeRequest")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "centerOfMassTransform",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SubmergedVolumeRequest::m_centerOfMassTransform))
                ->Property(
                    "surfacePosition",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SubmergedVolumeRequest::m_surfacePosition))
                ->Property("scale", JOLT_BEHAVIOR_VALUE_PROPERTY(&SubmergedVolumeRequest::m_scale))
                ->Property(
                    "surfaceNormal",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SubmergedVolumeRequest::m_surfaceNormal));

            behaviorContext->Class<SubmergedVolumeResult>("SubmergedVolumeResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "centerOfBuoyancy",
                    BehaviorValueGetter(&SubmergedVolumeResult::m_centerOfBuoyancy),
                    nullptr)
                ->Property(
                    "submergedVolume",
                    BehaviorValueGetter(&SubmergedVolumeResult::m_submergedVolume),
                    nullptr)
                ->Property(
                    "totalVolume",
                    BehaviorValueGetter(&SubmergedVolumeResult::m_totalVolume),
                    nullptr);
        }
    }
} // namespace Jolt
