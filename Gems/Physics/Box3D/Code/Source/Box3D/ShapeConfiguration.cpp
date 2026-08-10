/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/ShapeConfiguration.h>

#include <Box3D/ConfigurationSerializer.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    namespace
    {
        template<typename Geometry>
        Geometry GetCompoundChildGeometry(
            const CompoundChildGeometry& geometry)
        {
            const Geometry* value = AZStd::get_if<Geometry>(&geometry);
            if (value)
            {
                return *value;
            }

            return {};
        }

    } // namespace

    CompoundChildType CompoundChildShapeConfiguration::GetType() const
    {
        return static_cast<CompoundChildType>(m_geometry.index());
    }

    SphereShapeConfiguration CompoundChildShapeConfiguration::GetSphere() const
    {
        return GetCompoundChildGeometry<SphereShapeConfiguration>(m_geometry);
    }

    CapsuleShapeConfiguration CompoundChildShapeConfiguration::GetCapsule() const
    {
        return GetCompoundChildGeometry<CapsuleShapeConfiguration>(m_geometry);
    }

    BoxShapeConfiguration CompoundChildShapeConfiguration::GetBox() const
    {
        return GetCompoundChildGeometry<BoxShapeConfiguration>(m_geometry);
    }

    CylinderShapeConfiguration CompoundChildShapeConfiguration::GetCylinder() const
    {
        return GetCompoundChildGeometry<CylinderShapeConfiguration>(m_geometry);
    }

    ConvexHullShapeConfiguration CompoundChildShapeConfiguration::GetConvexHull() const
    {
        return GetCompoundChildGeometry<ConvexHullShapeConfiguration>(m_geometry);
    }

    TriangleMeshShapeConfiguration CompoundChildShapeConfiguration::GetTriangleMesh() const
    {
        return GetCompoundChildGeometry<TriangleMeshShapeConfiguration>(m_geometry);
    }

    void CompoundChildShapeConfiguration::SetSphere(
        const SphereShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void CompoundChildShapeConfiguration::SetCapsule(
        const CapsuleShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void CompoundChildShapeConfiguration::SetBox(
        const BoxShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void CompoundChildShapeConfiguration::SetCylinder(
        const CylinderShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void CompoundChildShapeConfiguration::SetConvexHull(
        const ConvexHullShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void CompoundChildShapeConfiguration::SetTriangleMesh(
        const TriangleMeshShapeConfiguration& geometry)
    {
        m_geometry = geometry;
    }

    void ShapeConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* jsonContext = azrtti_cast<AZ::JsonRegistrationContext*>(context))
        {
            jsonContext->Serializer<JsonShapeGeometrySerializer>()->HandlesType<ShapeGeometry>();
            jsonContext->Serializer<JsonCompoundChildGeometrySerializer>()->HandlesType<CompoundChildGeometry>();
        }

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SphereShapeConfiguration>()->Field("Radius", &SphereShapeConfiguration::m_radius);

            serializeContext
                ->Class<CapsuleShapeConfiguration>()
                ->Field("Height", &CapsuleShapeConfiguration::m_height)
                ->Field("Radius", &CapsuleShapeConfiguration::m_radius);

            serializeContext->Class<BoxShapeConfiguration>()->Field("HalfExtents", &BoxShapeConfiguration::m_halfExtents);

            serializeContext
                ->Class<CylinderShapeConfiguration>()
                ->Field("Height", &CylinderShapeConfiguration::m_height)
                ->Field("Radius", &CylinderShapeConfiguration::m_radius)
                ->Field("SideCount", &CylinderShapeConfiguration::m_sideCount);

            serializeContext->Class<ConvexHullShapeConfiguration>()->Field("Vertices", &ConvexHullShapeConfiguration::m_vertices);

            serializeContext
                ->Class<TriangleMeshShapeConfiguration>()
                ->Field("Vertices", &TriangleMeshShapeConfiguration::m_vertices)
                ->Field("Indices", &TriangleMeshShapeConfiguration::m_indices)
                ->Field("MaterialIndices", &TriangleMeshShapeConfiguration::m_materialIndices)
                ->Field("WeldTolerance", &TriangleMeshShapeConfiguration::m_weldTolerance)
                ->Field("WeldVertices", &TriangleMeshShapeConfiguration::m_weldVertices)
                ->Field("UseMedianSplit", &TriangleMeshShapeConfiguration::m_useMedianSplit)
                ->Field("IdentifyEdges", &TriangleMeshShapeConfiguration::m_identifyEdges);

            serializeContext
                ->Class<HeightfieldShapeConfiguration>()
                ->Field("Samples", &HeightfieldShapeConfiguration::m_samples)
                ->Field("MaterialIndices", &HeightfieldShapeConfiguration::m_materialIndices)
                ->Field("ColumnCount", &HeightfieldShapeConfiguration::m_columnCount)
                ->Field("RowCount", &HeightfieldShapeConfiguration::m_rowCount)
                ->Field("SampleSpacing", &HeightfieldShapeConfiguration::m_sampleSpacing)
                ->Field("HeightScale", &HeightfieldShapeConfiguration::m_heightScale)
                ->Field("MinimumHeight", &HeightfieldShapeConfiguration::m_minimumHeight)
                ->Field("MaximumHeight", &HeightfieldShapeConfiguration::m_maximumHeight)
                ->Field("Clockwise", &HeightfieldShapeConfiguration::m_clockwise)
                ->Field("UseSharedHeightRange", &HeightfieldShapeConfiguration::m_useSharedHeightRange);

            serializeContext
                ->Class<CompoundChildShapeConfiguration>()
                ->Field("Geometry", &CompoundChildShapeConfiguration::m_geometry)
                ->Field("LocalTransform", &CompoundChildShapeConfiguration::m_localTransform)
                ->Field("MaterialIndex", &CompoundChildShapeConfiguration::m_materialIndex);

            serializeContext->Class<CompoundShapeConfiguration>()->Field("Children", &CompoundShapeConfiguration::m_children);

            serializeContext
                ->Class<ShapeProperties>()
                ->Field("LocalTransform", &ShapeProperties::m_localTransform)
                ->Field("CollisionFilter", &ShapeProperties::m_collisionFilter)
                ->Field("Density", &ShapeProperties::m_density)
                ->Field("ExplosionScale", &ShapeProperties::m_explosionScale)
                ->Field("IsSensor", &ShapeProperties::m_isSensor)
                ->Field("EnableSensorEvents", &ShapeProperties::m_enableSensorEvents)
                ->Field("EnableContactEvents", &ShapeProperties::m_enableContactEvents)
                ->Field("EnableHitEvents", &ShapeProperties::m_enableHitEvents)
                ->Field("EnableCustomFiltering", &ShapeProperties::m_enableCustomFiltering)
                ->Field("EnablePreSolveEvents", &ShapeProperties::m_enablePreSolveEvents)
                ->Field("CreateContactsImmediately", &ShapeProperties::m_createContactsImmediately)
                ->Field("UpdateBodyMass", &ShapeProperties::m_updateBodyMass);

            serializeContext
                ->Class<ShapeConfiguration>()
                ->Field("Geometry", &ShapeConfiguration::m_geometry)
                ->Field("Properties", &ShapeConfiguration::m_properties)
                ->Field("MaterialConfigurations", &ShapeConfiguration::m_materialConfigurations);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SphereShapeConfiguration>("Sphere", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SphereShapeConfiguration::m_radius, "Radius", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f);
                editContext->Class<CapsuleShapeConfiguration>("Capsule", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CapsuleShapeConfiguration::m_height, "Height", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CapsuleShapeConfiguration::m_radius, "Radius", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f);
                editContext->Class<BoxShapeConfiguration>("Box", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BoxShapeConfiguration::m_halfExtents, "Half extents", "");
                editContext->Class<CylinderShapeConfiguration>("Cylinder", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CylinderShapeConfiguration::m_height, "Height", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CylinderShapeConfiguration::m_radius, "Radius", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CylinderShapeConfiguration::m_sideCount, "Sides", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 3)
                    ->Attribute(AZ::Edit::Attributes::Max, 32);
                editContext->Class<ConvexHullShapeConfiguration>("Convex hull", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ConvexHullShapeConfiguration::m_vertices, "Vertices", "");
                editContext->Class<TriangleMeshShapeConfiguration>("Triangle mesh", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_vertices, "Vertices", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_indices, "Indices", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_materialIndices, "Material indices", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_weldTolerance, "Weld tolerance", "Maximum distance for optional vertex welding.")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_weldVertices, "Weld vertices", "Merge vertices within the weld tolerance while cooking.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_useMedianSplit, "Median split", "Prefer faster median partitioning for structured meshes such as grids.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TriangleMeshShapeConfiguration::m_identifyEdges, "Identify edges", "Compute triangle adjacency from shared edges.");
                editContext->Class<HeightfieldShapeConfiguration>("Heightfield", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_samples, "Samples", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_materialIndices, "Material indices", "One material index per heightfield cell.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_columnCount, "Columns", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_rowCount, "Rows", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_sampleSpacing, "Sample spacing", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_heightScale, "Height scale", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_minimumHeight, "Shared minimum height", "Quantization minimum shared by adjacent heightfields when enabled.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_maximumHeight, "Shared maximum height", "Quantization maximum shared by adjacent heightfields when enabled.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_clockwise, "Clockwise", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldShapeConfiguration::m_useSharedHeightRange, "Use shared height range", "Use the explicit range so adjacent fields quantize shared heights identically.");
                editContext->Class<CompoundChildShapeConfiguration>("Child shape", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CompoundChildShapeConfiguration::m_geometry, "Geometry", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CompoundChildShapeConfiguration::m_localTransform, "Local transform", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CompoundChildShapeConfiguration::m_materialIndex, "Material index", "");
                editContext->Class<CompoundShapeConfiguration>("Compound", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CompoundShapeConfiguration::m_children, "Children", "");
                editContext->Class<ShapeProperties>("Shape properties", "Collision and instance behavior")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_localTransform, "Local transform", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_collisionFilter, "Collision filter", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_density, "Density", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_explosionScale, "Explosion scale", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_isSensor, "Sensor", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_enableSensorEvents, "Sensor events", "Report when this dynamic or kinematic shape enters and leaves sensors")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_enableContactEvents, "Contact events", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_enableHitEvents, "Hit events", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_enableCustomFiltering, "Custom filtering", "Invoke the world collision filter for this shape")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_enablePreSolveEvents, "Pre-solve events", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_createContactsImmediately, "Create contacts immediately", "Disable while attaching shapes in bulk; contacts are created by a later simulation step.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeProperties::m_updateBodyMass, "Update body mass", "Disable while attaching shapes in bulk, then recompute mass once after the batch.");
                editContext->Class<ShapeConfiguration>("Shape", "Geometry and collision behavior")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeConfiguration::m_geometry, "Geometry", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeConfiguration::m_properties, "Properties", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ShapeConfiguration::m_materialConfigurations, "Materials", "Physical response and diagnostic appearance for this shape");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {

            behaviorContext->Class<SphereShapeConfiguration>("SphereShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("radius", BehaviorValueProperty(&SphereShapeConfiguration::m_radius));

            behaviorContext->Class<CapsuleShapeConfiguration>("CapsuleShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("height", BehaviorValueProperty(&CapsuleShapeConfiguration::m_height))
                ->Property("radius", BehaviorValueProperty(&CapsuleShapeConfiguration::m_radius));

            behaviorContext->Class<BoxShapeConfiguration>("BoxShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("halfExtents", BehaviorValueProperty(&BoxShapeConfiguration::m_halfExtents));

            behaviorContext->Class<CylinderShapeConfiguration>("CylinderShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("height", BehaviorValueProperty(&CylinderShapeConfiguration::m_height))
                ->Property("radius", BehaviorValueProperty(&CylinderShapeConfiguration::m_radius))
                ->Property("sideCount", BehaviorValueProperty(&CylinderShapeConfiguration::m_sideCount));

            behaviorContext->Class<ConvexHullShapeConfiguration>("ConvexHullShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("vertices", BehaviorValueProperty(&ConvexHullShapeConfiguration::m_vertices));

            behaviorContext->Class<TriangleMeshShapeConfiguration>("TriangleMeshShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("vertices", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_vertices))
                ->Property("indices", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_indices))
                ->Property("materialIndices", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_materialIndices))
                ->Property("weldTolerance", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_weldTolerance))
                ->Property("weldVertices", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_weldVertices))
                ->Property("useMedianSplit", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_useMedianSplit))
                ->Property("identifyEdges", BehaviorValueProperty(&TriangleMeshShapeConfiguration::m_identifyEdges));

            behaviorContext->Class<ShapeProperties>("ShapeProperties")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Method("GetMaterials", &ShapeProperties::GetMaterials)
                ->Method("SetMaterials", &ShapeProperties::SetMaterials)
                ->Property("localTransform", BehaviorValueProperty(&ShapeProperties::m_localTransform))
                ->Property("collisionFilter", BehaviorValueProperty(&ShapeProperties::m_collisionFilter))
                ->Property("density", BehaviorValueProperty(&ShapeProperties::m_density))
                ->Property("explosionScale", BehaviorValueProperty(&ShapeProperties::m_explosionScale))
                ->Property("isSensor", BehaviorValueProperty(&ShapeProperties::m_isSensor))
                ->Property("enableSensorEvents", BehaviorValueProperty(&ShapeProperties::m_enableSensorEvents))
                ->Property("enableContactEvents", BehaviorValueProperty(&ShapeProperties::m_enableContactEvents))
                ->Property("enableHitEvents", BehaviorValueProperty(&ShapeProperties::m_enableHitEvents))
                ->Property("enableCustomFiltering", BehaviorValueProperty(&ShapeProperties::m_enableCustomFiltering))
                ->Property("enablePreSolveEvents", BehaviorValueProperty(&ShapeProperties::m_enablePreSolveEvents))
                ->Property("createContactsImmediately", BehaviorValueProperty(&ShapeProperties::m_createContactsImmediately))
                ->Property("updateBodyMass", BehaviorValueProperty(&ShapeProperties::m_updateBodyMass));

            behaviorContext->Class<ShapeConfiguration>("ShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Method("GetMaterialConfigurations", &ShapeConfiguration::GetMaterialConfigurations)
                ->Method("SetMaterialConfigurations", &ShapeConfiguration::SetMaterialConfigurations)
                ->Property("properties", BehaviorValueProperty(&ShapeConfiguration::m_properties));

            behaviorContext->Class<HeightfieldShapeConfiguration>("HeightfieldShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("samples", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_samples))
                ->Property("materialIndices", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_materialIndices))
                ->Property("columnCount", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_columnCount))
                ->Property("rowCount", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_rowCount))
                ->Property("sampleSpacing", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_sampleSpacing))
                ->Property("heightScale", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_heightScale))
                ->Property("minimumHeight", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_minimumHeight))
                ->Property("maximumHeight", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_maximumHeight))
                ->Property("clockwise", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_clockwise))
                ->Property("useSharedHeightRange", BehaviorValueProperty(&HeightfieldShapeConfiguration::m_useSharedHeightRange));

            behaviorContext->Enum<static_cast<int>(CompoundChildType::Sphere)>("CompoundChildType_Sphere")
                ->Enum<static_cast<int>(CompoundChildType::Capsule)>("CompoundChildType_Capsule")
                ->Enum<static_cast<int>(CompoundChildType::Box)>("CompoundChildType_Box")
                ->Enum<static_cast<int>(CompoundChildType::Cylinder)>("CompoundChildType_Cylinder")
                ->Enum<static_cast<int>(CompoundChildType::ConvexHull)>("CompoundChildType_ConvexHull")
                ->Enum<static_cast<int>(CompoundChildType::TriangleMesh)>("CompoundChildType_TriangleMesh");

            behaviorContext->Class<CompoundChildShapeConfiguration>("CompoundChildShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Method("GetType", &CompoundChildShapeConfiguration::GetType)
                ->Method("GetSphere", &CompoundChildShapeConfiguration::GetSphere)
                ->Method("GetCapsule", &CompoundChildShapeConfiguration::GetCapsule)
                ->Method("GetBox", &CompoundChildShapeConfiguration::GetBox)
                ->Method("GetCylinder", &CompoundChildShapeConfiguration::GetCylinder)
                ->Method("GetConvexHull", &CompoundChildShapeConfiguration::GetConvexHull)
                ->Method("GetTriangleMesh", &CompoundChildShapeConfiguration::GetTriangleMesh)
                ->Method("SetSphere", &CompoundChildShapeConfiguration::SetSphere)
                ->Method("SetCapsule", &CompoundChildShapeConfiguration::SetCapsule)
                ->Method("SetBox", &CompoundChildShapeConfiguration::SetBox)
                ->Method("SetCylinder", &CompoundChildShapeConfiguration::SetCylinder)
                ->Method("SetConvexHull", &CompoundChildShapeConfiguration::SetConvexHull)
                ->Method("SetTriangleMesh", &CompoundChildShapeConfiguration::SetTriangleMesh)
                ->Property("localTransform", BehaviorValueProperty(&CompoundChildShapeConfiguration::m_localTransform))
                ->Property("materialIndex", BehaviorValueProperty(&CompoundChildShapeConfiguration::m_materialIndex));

            behaviorContext->Class<CompoundShapeConfiguration>("CompoundShapeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("children", BehaviorValueProperty(&CompoundShapeConfiguration::m_children));
        }
    }
} // namespace Box3D
