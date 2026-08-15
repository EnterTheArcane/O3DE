/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Cooking.h>

#include <Jolt/BehaviorReflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void CookedShapeArchive::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CookedCompoundChildConfiguration>()
                ->Field("ShapeHandle", &CookedCompoundChildConfiguration::m_shapeHandle)
                ->Field("Position", &CookedCompoundChildConfiguration::m_position)
                ->Field("Rotation", &CookedCompoundChildConfiguration::m_rotation)
                ->Field("UserData", &CookedCompoundChildConfiguration::m_userData);

            serializeContext
                ->Class<CookedCompoundShapeConfiguration>()
                ->Field("Children", &CookedCompoundShapeConfiguration::m_children)
                ->Field("UserData", &CookedCompoundShapeConfiguration::m_userData);

            serializeContext
                ->Class<CookedOffsetCenterOfMassShapeConfiguration>()
                ->Field("ShapeHandle", &CookedOffsetCenterOfMassShapeConfiguration::m_shapeHandle)
                ->Field("Offset", &CookedOffsetCenterOfMassShapeConfiguration::m_offset);

            serializeContext
                ->Class<CookedRotatedTranslatedShapeConfiguration>()
                ->Field("ShapeHandle", &CookedRotatedTranslatedShapeConfiguration::m_shapeHandle)
                ->Field("Position", &CookedRotatedTranslatedShapeConfiguration::m_position)
                ->Field("Rotation", &CookedRotatedTranslatedShapeConfiguration::m_rotation);

            serializeContext
                ->Class<CookedScaledShapeConfiguration>()
                ->Field("ShapeHandle", &CookedScaledShapeConfiguration::m_shapeHandle)
                ->Field("Scale", &CookedScaledShapeConfiguration::m_scale);

            serializeContext
                ->Class<CookedDecoratedShapeConfiguration>()
                ->Field("Geometry", &CookedDecoratedShapeConfiguration::m_geometry)
                ->Field("UserData", &CookedDecoratedShapeConfiguration::m_userData);

            serializeContext
                ->Class<CookedShapeArchive>()
                ->Field("BinaryState", &CookedShapeArchive::m_binaryState)
                ->Field("BuildFingerprint", &CookedShapeArchive::m_buildFingerprint)
                ->Field("ContentHash", &CookedShapeArchive::m_contentHash)
                ->Field("FormatVersion", &CookedShapeArchive::m_formatVersion)
                ->Field("MaterialCount", &CookedShapeArchive::m_materialCount)
                ->Field("ChildShapeCount", &CookedShapeArchive::m_childShapeCount);

            serializeContext
                ->Class<CookedRaycastHit>()
                ->Field("Position", &CookedRaycastHit::m_position)
                ->Field("Normal", &CookedRaycastHit::m_normal)
                ->Field("MaterialHandle", &CookedRaycastHit::m_materialHandle)
                ->Field("SubShapeId", &CookedRaycastHit::m_subShapeId)
                ->Field("Distance", &CookedRaycastHit::m_distance)
                ->Field("Fraction", &CookedRaycastHit::m_fraction);

            serializeContext
                ->Class<CustomConvexShapeInfo>()
                ->Field("ProviderId", &CustomConvexShapeInfo::m_providerId)
                ->Field("ProviderVersion", &CustomConvexShapeInfo::m_providerVersion)
                ->Field("SourceHash", &CustomConvexShapeInfo::m_sourceHash);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<CookedCompoundChildConfiguration>("CookedCompoundChildConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundChildConfiguration::m_shapeHandle))
                ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundChildConfiguration::m_position))
                ->Property("rotation", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundChildConfiguration::m_rotation))
                ->Property("userData", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundChildConfiguration::m_userData));

            behaviorContext->Class<CookedCompoundShapeConfiguration>("CookedCompoundShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("children", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundShapeConfiguration::m_children))
                ->Property("userData", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedCompoundShapeConfiguration::m_userData));

            behaviorContext->Class<CookedOffsetCenterOfMassShapeConfiguration>(
                "CookedOffsetCenterOfMassShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedOffsetCenterOfMassShapeConfiguration::m_shapeHandle))
                ->Property("offset", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedOffsetCenterOfMassShapeConfiguration::m_offset));

            behaviorContext->Class<CookedRotatedTranslatedShapeConfiguration>(
                "CookedRotatedTranslatedShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedRotatedTranslatedShapeConfiguration::m_shapeHandle))
                ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedRotatedTranslatedShapeConfiguration::m_position))
                ->Property("rotation", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedRotatedTranslatedShapeConfiguration::m_rotation));

            behaviorContext->Class<CookedScaledShapeConfiguration>("CookedScaledShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedScaledShapeConfiguration::m_shapeHandle))
                ->Property("scale", JOLT_BEHAVIOR_VALUE_PROPERTY(&CookedScaledShapeConfiguration::m_scale));

            behaviorContext->Class<CookedRaycastHit>("CookedRaycastHit")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("position", BehaviorValueGetter(&CookedRaycastHit::m_position), nullptr)
                ->Property("normal", BehaviorValueGetter(&CookedRaycastHit::m_normal), nullptr)
                ->Property("materialHandle", BehaviorValueGetter(&CookedRaycastHit::m_materialHandle), nullptr)
                ->Property("subShapeId", BehaviorValueGetter(&CookedRaycastHit::m_subShapeId), nullptr)
                ->Property("distance", BehaviorValueGetter(&CookedRaycastHit::m_distance), nullptr)
                ->Property("fraction", BehaviorValueGetter(&CookedRaycastHit::m_fraction), nullptr);

            behaviorContext->Class<CustomConvexShapeInfo>("CustomConvexShapeInfo")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("providerId", BehaviorValueGetter(&CustomConvexShapeInfo::m_providerId), nullptr)
                ->Property("providerVersion", BehaviorValueGetter(&CustomConvexShapeInfo::m_providerVersion), nullptr)
                ->Property("sourceHash", BehaviorValueGetter(&CustomConvexShapeInfo::m_sourceHash), nullptr);
        }
    }
} // namespace Jolt
