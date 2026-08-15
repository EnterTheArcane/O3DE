/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ColliderComponent.h>

#include <Jolt/Reflection.h>
#include <Jolt/System.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        constexpr AZ::u32 MaximumScriptMaterialCount = 256;

        template<class Value>
        [[nodiscard]]
        AZ::u32 GetCollectionSize(
            const AZStd::vector<Value>& values)
        {
            return aznumeric_cast<AZ::u32>(values.size());
        }

        template<class Value>
        [[nodiscard]]
        Value GetCollectionItem(
            const AZStd::vector<Value>& values,
            const AZ::u32 index)
        {
            if (index < values.size())
            {
                return values[index];
            }

            return {};
        }

        template<class Value>
        [[nodiscard]]
        bool HasCollectionOverflow(
            const AZStd::vector<Value>& values,
            const AZ::u32 requiredCount)
        {
            return values.size() < requiredCount;
        }

        template<class Configuration>
        [[nodiscard]]
        bool GetDecoratedConfiguration(
            ISystem* system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            Configuration& configuration)
        {
            DecoratedShapeConfiguration decoratedConfiguration;
            if (!system
                || !system->GetDecoratedShapeConfiguration(
                    worldHandle,
                    shapeHandle,
                    decoratedConfiguration))
            {
                return false;
            }

            const auto* typedConfiguration = AZStd::get_if<Configuration>(
                &decoratedConfiguration.m_geometry);
            if (!typedConfiguration)
            {
                return false;
            }

            configuration = *typedConfiguration;
            return true;
        }

        template<class Configuration>
        [[nodiscard]]
        bool GetPrimitiveConfiguration(
            ISystem* system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            Configuration& configuration)
        {
            PrimitiveShapeState state;
            if (!system
                || !system->GetPrimitiveShapeState(
                    worldHandle,
                    shapeHandle,
                    state))
            {
                return false;
            }

            const auto* typedConfiguration = AZStd::get_if<Configuration>(&state.m_geometry);
            if (!typedConfiguration)
            {
                return false;
            }

            configuration = *typedConfiguration;
            return true;
        }
    } // namespace

    bool HeightfieldSampleCollection::AddSample(
        const float sample)
    {
        if (m_samples.size() >= MaximumScriptHeightfieldElements
            || !AZ::IsFiniteFloat(sample))
        {
            return false;
        }

        m_samples.push_back(sample);
        m_requiredSampleCount = GetCollectionSize(m_samples);
        return true;
    }

    void HeightfieldSampleCollection::Clear()
    {
        m_samples.clear();
        m_requiredSampleCount = 0;
    }

    AZ::u32 HeightfieldSampleCollection::GetSampleCount() const
    {
        return GetCollectionSize(m_samples);
    }

    float HeightfieldSampleCollection::GetSample(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_samples, index);
    }

    AZ::u32 HeightfieldSampleCollection::GetRequiredSampleCount() const
    {
        return m_requiredSampleCount;
    }

    bool HeightfieldSampleCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_samples, m_requiredSampleCount);
    }

    AZStd::span<const float> HeightfieldSampleCollection::GetSamples() const
    {
        return m_samples;
    }

    bool HeightfieldMaterialIndexCollection::AddIndex(
        const AZ::u8 index)
    {
        if (m_indices.size() >= MaximumScriptHeightfieldElements)
        {
            return false;
        }

        m_indices.push_back(index);
        m_requiredIndexCount = GetCollectionSize(m_indices);
        return true;
    }

    void HeightfieldMaterialIndexCollection::Clear()
    {
        m_indices.clear();
        m_requiredIndexCount = 0;
    }

    AZ::u32 HeightfieldMaterialIndexCollection::GetIndexCount() const
    {
        return GetCollectionSize(m_indices);
    }

    AZ::u8 HeightfieldMaterialIndexCollection::GetIndex(
        const AZ::u32 position) const
    {
        return GetCollectionItem(m_indices, position);
    }

    AZ::u32 HeightfieldMaterialIndexCollection::GetRequiredIndexCount() const
    {
        return m_requiredIndexCount;
    }

    bool HeightfieldMaterialIndexCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_indices, m_requiredIndexCount);
    }

    AZStd::span<const AZ::u8> HeightfieldMaterialIndexCollection::GetIndices() const
    {
        return m_indices;
    }

    bool MaterialCollection::AddMaterial(
        const MaterialHandle materialHandle)
    {
        if (!materialHandle || m_materials.size() >= MaximumScriptMaterialCount)
        {
            return false;
        }

        m_materials.push_back(materialHandle);
        m_requiredMaterialCount = GetCollectionSize(m_materials);
        return true;
    }

    void MaterialCollection::Clear()
    {
        m_materials.clear();
        m_requiredMaterialCount = 0;
    }

    AZ::u32 MaterialCollection::GetMaterialCount() const
    {
        return GetCollectionSize(m_materials);
    }

    MaterialHandle MaterialCollection::GetMaterial(
        const AZ::u32 index) const
    {
        return GetCollectionItem(m_materials, index);
    }

    AZ::u32 MaterialCollection::GetRequiredMaterialCount() const
    {
        return m_requiredMaterialCount;
    }

    bool MaterialCollection::HasOverflow() const
    {
        return HasCollectionOverflow(m_materials, m_requiredMaterialCount);
    }

    AZStd::span<const MaterialHandle> MaterialCollection::GetMaterials() const
    {
        return m_materials;
    }

    ConvexHullState ConvexHullTopology::GetState() const
    {
        return m_state;
    }

    AZ::u32 ConvexHullTopology::GetPointCount() const
    {
        return GetCollectionSize(m_pointsRelativeToCenterOfMass);
    }

    AZ::Vector3 ConvexHullTopology::GetPointRelativeToCenterOfMass(
        const AZ::u32 pointIndex) const
    {
        return GetCollectionItem(m_pointsRelativeToCenterOfMass, pointIndex);
    }

    AZ::u32 ConvexHullTopology::GetPlaneCount() const
    {
        return GetCollectionSize(m_planesRelativeToCenterOfMass);
    }

    AZ::Plane ConvexHullTopology::GetPlaneRelativeToCenterOfMass(
        const AZ::u32 planeIndex) const
    {
        if (planeIndex < m_planesRelativeToCenterOfMass.size())
        {
            return m_planesRelativeToCenterOfMass[planeIndex];
        }

        return AZ::Plane::CreateFromNormalAndDistance(AZ::Vector3::CreateAxisZ(), 0.0f);
    }

    AZ::u32 ConvexHullTopology::GetFaceCount() const
    {
        if (m_faceVertexOffsetsAndIndices.size() < m_state.m_faceCount + 1)
        {
            return 0;
        }

        return m_state.m_faceCount;
    }

    AZ::u32 ConvexHullTopology::GetFaceVertexCount(
        const AZ::u32 faceIndex) const
    {
        if (faceIndex >= GetFaceCount())
        {
            return 0;
        }

        return m_faceVertexOffsetsAndIndices[faceIndex + 1]
            - m_faceVertexOffsetsAndIndices[faceIndex];
    }

    AZ::u32 ConvexHullTopology::GetFaceVertexIndex(
        const AZ::u32 faceIndex,
        const AZ::u32 faceVertexIndex) const
    {
        if (faceIndex >= GetFaceCount())
        {
            return 0;
        }

        const AZ::u32 firstVertex = m_faceVertexOffsetsAndIndices[faceIndex];
        const AZ::u32 vertexCount = m_faceVertexOffsetsAndIndices[faceIndex + 1] - firstVertex;
        if (faceVertexIndex >= vertexCount)
        {
            return 0;
        }

        const size_t dataIndex = m_state.m_faceCount + 1 + firstVertex + faceVertexIndex;
        if (dataIndex >= m_faceVertexOffsetsAndIndices.size())
        {
            return 0;
        }

        return m_faceVertexOffsetsAndIndices[dataIndex];
    }

    bool ConvexHullTopology::HasOverflow() const
    {
        return m_overflow;
    }

    bool ConvexHullTopology::IsComplete() const
    {
        return m_complete;
    }

    void ColliderShapeConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        ShapeConfiguration::Reflect(context);
        MaterialConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<ColliderShapeConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<ColliderShapeConfiguration>()
                ->Field("Shape", &ColliderShapeConfiguration::m_shape)
                ->Field("Materials", &ColliderShapeConfiguration::m_materials)
                ->Field("LocalTransform", &ColliderShapeConfiguration::m_localTransform)
                ->Field("CompoundUserData", &ColliderShapeConfiguration::m_compoundUserData);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ColliderShapeConfiguration>("Shape", "A shape and its body-local transform.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderShapeConfiguration::m_shape,
                        "Shape",
                        "Shape geometry and density.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderShapeConfiguration::m_materials,
                        "Materials",
                        "Materials referenced by the shape.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderShapeConfiguration::m_localTransform,
                        "Local transform",
                        "Body-local translation, rotation, and uniform scale.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderShapeConfiguration::m_compoundUserData,
                        "Compound user data",
                        "Application data stored on this child when multiple shapes are combined.");
            }
        }
    }

    ColliderComponent::ColliderComponent(
        AZStd::vector<ColliderShapeConfiguration> configurations)
        : m_configurations(AZStd::move(configurations))
    {
    }

    void ColliderComponent::Reflect(
        AZ::ReflectContext* context)
    {
        ColliderShapeConfiguration::Reflect(context);
        ShapeStats::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ConvexHullTopology>();
            serializeContext->Class<HeightfieldSampleCollection>();
            serializeContext->Class<HeightfieldMaterialIndexCollection>();
            serializeContext->Class<MaterialCollection>();

            serializeContext
                ->Class<ColliderComponent, AZ::Component>()
                ->Field("Shapes", &ColliderComponent::m_configurations);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<ColliderComponent>("Jolt Collider", "Creates the collision shape for a Jolt body.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ColliderComponent::m_configurations,
                        "Shapes",
                        "One or more shapes combined into the body shape.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ConvexHullTopology>("ConvexHullTopology")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("GetState", &ConvexHullTopology::GetState)
                ->Method("GetPointCount", &ConvexHullTopology::GetPointCount)
                ->Method("GetPointRelativeToCenterOfMass", &ConvexHullTopology::GetPointRelativeToCenterOfMass)
                ->Method("GetPlaneCount", &ConvexHullTopology::GetPlaneCount)
                ->Method("GetPlaneRelativeToCenterOfMass", &ConvexHullTopology::GetPlaneRelativeToCenterOfMass)
                ->Method("GetFaceCount", &ConvexHullTopology::GetFaceCount)
                ->Method("GetFaceVertexCount", &ConvexHullTopology::GetFaceVertexCount)
                ->Method("GetFaceVertexIndex", &ConvexHullTopology::GetFaceVertexIndex)
                ->Method("HasOverflow", &ConvexHullTopology::HasOverflow)
                ->Method("IsComplete", &ConvexHullTopology::IsComplete);

            behaviorContext->Class<HeightfieldSampleCollection>("HeightfieldSampleCollection")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("AddSample", &HeightfieldSampleCollection::AddSample)
                ->Method("Clear", &HeightfieldSampleCollection::Clear)
                ->Method("GetSampleCount", &HeightfieldSampleCollection::GetSampleCount)
                ->Method("GetSample", &HeightfieldSampleCollection::GetSample)
                ->Method("GetRequiredSampleCount", &HeightfieldSampleCollection::GetRequiredSampleCount)
                ->Method("HasOverflow", &HeightfieldSampleCollection::HasOverflow);

            behaviorContext->Class<HeightfieldMaterialIndexCollection>("HeightfieldMaterialIndexCollection")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("AddIndex", &HeightfieldMaterialIndexCollection::AddIndex)
                ->Method("Clear", &HeightfieldMaterialIndexCollection::Clear)
                ->Method("GetIndexCount", &HeightfieldMaterialIndexCollection::GetIndexCount)
                ->Method("GetIndex", &HeightfieldMaterialIndexCollection::GetIndex)
                ->Method("GetRequiredIndexCount", &HeightfieldMaterialIndexCollection::GetRequiredIndexCount)
                ->Method("HasOverflow", &HeightfieldMaterialIndexCollection::HasOverflow);

            behaviorContext->Class<MaterialCollection>("MaterialCollection")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("AddMaterial", &MaterialCollection::AddMaterial)
                ->Method("Clear", &MaterialCollection::Clear)
                ->Method("GetMaterialCount", &MaterialCollection::GetMaterialCount)
                ->Method("GetMaterial", &MaterialCollection::GetMaterial)
                ->Method("GetRequiredMaterialCount", &MaterialCollection::GetRequiredMaterialCount)
                ->Method("HasOverflow", &MaterialCollection::HasOverflow);

            behaviorContext->EBus<ColliderRequestBus>("JoltColliderRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("GetShapeCount", &IColliderRequests::GetShapeCount)
                ->Event("GetShapeHandleAt", &IColliderRequests::GetShapeHandleAt)
                ->Event("GetRootShapeHandle", &IColliderRequests::GetRootShapeHandle)
                ->Event("GetRootShapeStats", &IColliderRequests::GetRootShapeStats)
                ->Event("GetRootShapeStatsRecursive", &IColliderRequests::GetRootShapeStatsRecursive)
                ->Event("GetRootShapeProperties", &IColliderRequests::GetRootShapeProperties)
                ->Event("GetRootShapeSubmergedVolume", &IColliderRequests::GetRootShapeSubmergedVolume)
                ->Event("GetRootBoxConfiguration", &IColliderRequests::GetRootBoxConfiguration)
                ->Event("GetRootCapsuleConfiguration", &IColliderRequests::GetRootCapsuleConfiguration)
                ->Event("GetRootConvexHullState", &IColliderRequests::GetRootConvexHullState)
                ->Event("GetRootConvexHullTopology", &IColliderRequests::GetRootConvexHullTopology)
                ->Event("GetRootCylinderConfiguration", &IColliderRequests::GetRootCylinderConfiguration)
                ->Event("GetRootEmptyConfiguration", &IColliderRequests::GetRootEmptyConfiguration)
                ->Event("GetRootPlaneConfiguration", &IColliderRequests::GetRootPlaneConfiguration)
                ->Event("GetRootSphereConfiguration", &IColliderRequests::GetRootSphereConfiguration)
                ->Event(
                    "GetRootTaperedCapsuleConfiguration",
                    &IColliderRequests::GetRootTaperedCapsuleConfiguration)
                ->Event(
                    "GetRootTaperedCylinderConfiguration",
                    &IColliderRequests::GetRootTaperedCylinderConfiguration)
                ->Event("GetRootTriangleConfiguration", &IColliderRequests::GetRootTriangleConfiguration)
                ->Event("GetRootShapeMaterial", &IColliderRequests::GetRootShapeMaterial)
                ->Event("GetRootShapeSurfaceNormal", &IColliderRequests::GetRootShapeSurfaceNormal)
                ->Event("GetRootShapeUserData", &IColliderRequests::GetRootShapeUserData)
                ->Event("GetRootShapeSubShapeUserData", &IColliderRequests::GetRootShapeSubShapeUserData)
                ->Event("GetRootDirectChildShape", &IColliderRequests::GetRootDirectChildShape)
                ->Event(
                    "GetRootOffsetCenterOfMassConfiguration",
                    &IColliderRequests::GetRootOffsetCenterOfMassConfiguration)
                ->Event(
                    "GetRootRotatedTranslatedConfiguration",
                    &IColliderRequests::GetRootRotatedTranslatedConfiguration)
                ->Event("GetRootScaledConfiguration", &IColliderRequests::GetRootScaledConfiguration)
                ->Event("GetRootMeshMaterials", &IColliderRequests::GetRootMeshMaterials)
                ->Event(
                    "GetRootMeshTriangleMaterialIndex",
                    &IColliderRequests::GetRootMeshTriangleMaterialIndex)
                ->Event("GetRootMeshTriangleUserData", &IColliderRequests::GetRootMeshTriangleUserData)
                ->Event("GetRootHeightfieldState", &IColliderRequests::GetRootHeightfieldState)
                ->Event("GetRootHeightfieldPosition", &IColliderRequests::GetRootHeightfieldPosition)
                ->Event("IsRootHeightfieldNoCollision", &IColliderRequests::IsRootHeightfieldNoCollision)
                ->Event("GetRootHeightfieldHeights", &IColliderRequests::GetRootHeightfieldHeights)
                ->Event(
                    "GetRootHeightfieldMaterialIndices",
                    &IColliderRequests::GetRootHeightfieldMaterialIndices)
                ->Event("GetRootHeightfieldMaterials", &IColliderRequests::GetRootHeightfieldMaterials)
                ->Event(
                    "GetRootHeightfieldSubShapeCoordinates",
                    &IColliderRequests::GetRootHeightfieldSubShapeCoordinates)
                ->Event("UpdateRootHeightfieldHeights", &IColliderRequests::UpdateRootHeightfieldHeights)
                ->Event("UpdateRootHeightfieldMaterials", &IColliderRequests::UpdateRootHeightfieldMaterials)
                ->Event("GetRootCompoundChildCount", &IColliderRequests::GetRootCompoundChildCount)
                ->Event("GetRootCompoundChild", &IColliderRequests::GetRootCompoundChild)
                ->Event("GetRootCompoundChildIndex", &IColliderRequests::GetRootCompoundChildIndex)
                ->Event("IsRootShapeScaleValid", &IColliderRequests::IsRootShapeScaleValid)
                ->Event("MakeRootShapeScaleValid", &IColliderRequests::MakeRootShapeScaleValid);

            behaviorContext->Class<ColliderComponent>("Jolt::ColliderComponent")
                ->RequestBus("JoltColliderRequestBus");
        }
    }

    void ColliderComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    void ColliderComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltColliderService"));
    }

    AZStd::span<const ColliderShapeConfiguration> ColliderComponent::GetShapeConfigurations() const
    {
        return m_configurations;
    }

    AZStd::span<const ShapeHandle> ColliderComponent::GetShapeHandles() const
    {
        return m_shapeSet.m_shapeHandles;
    }

    ShapeHandle ColliderComponent::GetRootShapeHandle() const
    {
        return m_shapeSet.m_rootShapeHandle;
    }

    bool ColliderComponent::GetRootShapeStats(
        ShapeStats& stats) const
    {
        return m_system
            && m_system->GetShapeStats(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                stats);
    }

    bool ColliderComponent::GetRootShapeStatsRecursive(
        ShapeStats& stats) const
    {
        return m_system
            && m_system->GetShapeStatsRecursive(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                stats);
    }

    bool ColliderComponent::GetRootShapeProperties(
        ShapeProperties& properties) const
    {
        return m_system
            && m_system->GetShapeProperties(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                properties);
    }

    bool ColliderComponent::GetRootShapeSubmergedVolume(
        const SubmergedVolumeRequest& request,
        SubmergedVolumeResult& result) const
    {
        return m_system
            && m_system->GetShapeSubmergedVolume(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                request,
                result);
    }

    bool ColliderComponent::GetRootBoxConfiguration(
        BoxShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootCapsuleConfiguration(
        CapsuleShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootConvexHullState(
        ConvexHullState& state) const
    {
        return m_system
            && m_system->GetConvexHullState(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                state);
    }

    ConvexHullTopology ColliderComponent::GetRootConvexHullTopology() const
    {
        ConvexHullTopology topology;
        if (!GetRootConvexHullState(topology.m_state))
        {
            return topology;
        }
        if (topology.m_state.m_faceCount >= MaximumScriptConvexHullElements
            || topology.m_state.m_pointCount > MaximumScriptConvexHullElements)
        {
            topology.m_overflow = true;
            return topology;
        }

        topology.m_pointsRelativeToCenterOfMass.resize(topology.m_state.m_pointCount);
        BufferResult bufferResult = m_system->GetConvexHullPointsRelativeToCenterOfMass(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            topology.m_pointsRelativeToCenterOfMass);
        if (!bufferResult.IsComplete()
            || bufferResult.m_count != topology.m_state.m_pointCount)
        {
            topology.m_pointsRelativeToCenterOfMass.clear();
            return topology;
        }

        topology.m_planesRelativeToCenterOfMass.resize(topology.m_state.m_faceCount);
        bufferResult = m_system->GetConvexHullPlanesRelativeToCenterOfMass(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            topology.m_planesRelativeToCenterOfMass);
        if (!bufferResult.IsComplete()
            || bufferResult.m_count != topology.m_state.m_faceCount)
        {
            topology.m_planesRelativeToCenterOfMass.clear();
            return topology;
        }

        const size_t faceOffsetCount = topology.m_state.m_faceCount + 1;
        topology.m_faceVertexOffsetsAndIndices.resize(faceOffsetCount);
        for (AZ::u32 faceIndex = 0; faceIndex < topology.m_state.m_faceCount; ++faceIndex)
        {
            bufferResult = m_system->GetConvexHullFaceVertexIndices(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                faceIndex,
                {});
            const size_t remainingCapacity =
                MaximumScriptConvexHullElements - topology.m_faceVertexOffsetsAndIndices.size();
            if (bufferResult.m_requiredCount == 0
                || bufferResult.m_requiredCount > remainingCapacity)
            {
                topology.m_faceVertexOffsetsAndIndices.clear();
                topology.m_overflow = true;
                return topology;
            }

            const size_t firstVertex = topology.m_faceVertexOffsetsAndIndices.size();
            topology.m_faceVertexOffsetsAndIndices.resize(firstVertex + bufferResult.m_requiredCount);
            bufferResult = m_system->GetConvexHullFaceVertexIndices(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                faceIndex,
                AZStd::span<AZ::u32>(topology.m_faceVertexOffsetsAndIndices).subspan(firstVertex));
            if (!bufferResult.IsComplete()
                || bufferResult.m_count != topology.m_faceVertexOffsetsAndIndices.size() - firstVertex)
            {
                topology.m_faceVertexOffsetsAndIndices.clear();
                return topology;
            }
            topology.m_faceVertexOffsetsAndIndices[faceIndex + 1] =
                aznumeric_cast<AZ::u32>(topology.m_faceVertexOffsetsAndIndices.size() - faceOffsetCount);
        }

        topology.m_complete = true;
        return topology;
    }

    bool ColliderComponent::GetRootCylinderConfiguration(
        CylinderShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootEmptyConfiguration(
        EmptyShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootPlaneConfiguration(
        PlaneShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootSphereConfiguration(
        SphereShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootTaperedCapsuleConfiguration(
        TaperedCapsuleShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootTaperedCylinderConfiguration(
        TaperedCylinderShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootTriangleConfiguration(
        TriangleShapeConfiguration& configuration) const
    {
        return GetPrimitiveConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootShapeMaterial(
        const SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        return m_system
            && m_system->GetShapeMaterial(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                materialHandle);
    }

    bool ColliderComponent::GetRootShapeSurfaceNormal(
        const SubShapeId subShapeId,
        const AZ::Vector3& localSurfacePosition,
        AZ::Vector3& normal) const
    {
        return m_system
            && m_system->GetShapeSurfaceNormal(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                localSurfacePosition,
                normal);
    }

    bool ColliderComponent::GetRootShapeUserData(
        AZ::u64& userData) const
    {
        return m_system
            && m_system->GetShapeUserData(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                userData);
    }

    bool ColliderComponent::GetRootShapeSubShapeUserData(
        const SubShapeId subShapeId,
        AZ::u64& userData) const
    {
        return m_system
            && m_system->GetShapeSubShapeUserData(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                userData);
    }

    bool ColliderComponent::GetRootDirectChildShape(
        const SubShapeId subShapeId,
        ShapeHandle& childShapeHandle,
        SubShapeTransform& transform) const
    {
        return m_system
            && m_system->GetDirectChildShape(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                childShapeHandle,
                transform);
    }

    bool ColliderComponent::GetRootOffsetCenterOfMassConfiguration(
        OffsetCenterOfMassShapeConfiguration& configuration) const
    {
        return GetDecoratedConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootRotatedTranslatedConfiguration(
        RotatedTranslatedShapeConfiguration& configuration) const
    {
        return GetDecoratedConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    bool ColliderComponent::GetRootScaledConfiguration(
        ScaledShapeConfiguration& configuration) const
    {
        return GetDecoratedConfiguration(
            m_system,
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            configuration);
    }

    MaterialCollection ColliderComponent::GetRootMeshMaterials() const
    {
        MaterialCollection result;
        if (!m_system)
        {
            return result;
        }

        BufferResult bufferResult = m_system->GetMeshMaterials(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            {});
        result.m_requiredMaterialCount = bufferResult.m_requiredCount;
        if (bufferResult.m_requiredCount == 0
            || bufferResult.m_requiredCount > MaximumScriptMaterialCount)
        {
            return result;
        }

        result.m_materials.resize(bufferResult.m_requiredCount);
        bufferResult = m_system->GetMeshMaterials(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            result.m_materials);
        if (!bufferResult.IsComplete())
        {
            result.m_materials.clear();
        }
        result.m_requiredMaterialCount = bufferResult.m_requiredCount;
        return result;
    }

    bool ColliderComponent::GetRootMeshTriangleMaterialIndex(
        const SubShapeId subShapeId,
        AZ::u32& materialIndex) const
    {
        return m_system
            && m_system->GetMeshTriangleMaterialIndex(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                materialIndex);
    }

    bool ColliderComponent::GetRootMeshTriangleUserData(
        const SubShapeId subShapeId,
        AZ::u32& userData) const
    {
        return m_system
            && m_system->GetMeshTriangleUserData(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                userData);
    }

    bool ColliderComponent::GetRootHeightfieldState(
        HeightfieldState& state) const
    {
        return m_system
            && m_system->GetHeightfieldState(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                state);
    }

    bool ColliderComponent::GetRootHeightfieldPosition(
        const AZ::u32 column,
        const AZ::u32 row,
        AZ::Vector3& position) const
    {
        return m_system
            && m_system->GetHeightfieldPosition(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                column,
                row,
                position);
    }

    bool ColliderComponent::IsRootHeightfieldNoCollision(
        const AZ::u32 column,
        const AZ::u32 row,
        bool& noCollision) const
    {
        return m_system
            && m_system->IsHeightfieldNoCollision(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                column,
                row,
                noCollision);
    }

    HeightfieldSampleCollection ColliderComponent::GetRootHeightfieldHeights(
        const HeightfieldRegion& region) const
    {
        HeightfieldSampleCollection result;
        if (!m_system)
        {
            return result;
        }

        QueryResult queryResult = m_system->GetHeightfieldHeights(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            region,
            {});
        result.m_requiredSampleCount = queryResult.m_requiredHitCount;
        if (queryResult.m_requiredHitCount == 0
            || queryResult.m_requiredHitCount > MaximumScriptHeightfieldElements)
        {
            return result;
        }

        result.m_samples.resize(queryResult.m_requiredHitCount);
        queryResult = m_system->GetHeightfieldHeights(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            region,
            result.m_samples);
        if (!queryResult.IsComplete())
        {
            result.m_samples.clear();
        }
        result.m_requiredSampleCount = queryResult.m_requiredHitCount;
        return result;
    }

    HeightfieldMaterialIndexCollection ColliderComponent::GetRootHeightfieldMaterialIndices(
        const HeightfieldRegion& region) const
    {
        HeightfieldMaterialIndexCollection result;
        if (!m_system)
        {
            return result;
        }

        QueryResult queryResult = m_system->GetHeightfieldMaterialIndices(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            region,
            {});
        result.m_requiredIndexCount = queryResult.m_requiredHitCount;
        if (queryResult.m_requiredHitCount == 0
            || queryResult.m_requiredHitCount > MaximumScriptHeightfieldElements)
        {
            return result;
        }

        result.m_indices.resize(queryResult.m_requiredHitCount);
        queryResult = m_system->GetHeightfieldMaterialIndices(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            region,
            result.m_indices);
        if (!queryResult.IsComplete())
        {
            result.m_indices.clear();
        }
        result.m_requiredIndexCount = queryResult.m_requiredHitCount;
        return result;
    }

    MaterialCollection ColliderComponent::GetRootHeightfieldMaterials() const
    {
        MaterialCollection result;
        if (!m_system)
        {
            return result;
        }

        QueryResult queryResult = m_system->GetHeightfieldMaterials(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            {});
        result.m_requiredMaterialCount = queryResult.m_requiredHitCount;
        if (queryResult.m_requiredHitCount == 0
            || queryResult.m_requiredHitCount > MaximumScriptMaterialCount)
        {
            return result;
        }

        result.m_materials.resize(queryResult.m_requiredHitCount);
        queryResult = m_system->GetHeightfieldMaterials(
            m_worldHandle,
            m_shapeSet.m_rootShapeHandle,
            result.m_materials);
        if (!queryResult.IsComplete())
        {
            result.m_materials.clear();
        }
        result.m_requiredMaterialCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool ColliderComponent::GetRootHeightfieldSubShapeCoordinates(
        const SubShapeId subShapeId,
        HeightfieldSubShapeCoordinates& coordinates) const
    {
        return m_system
            && m_system->GetHeightfieldSubShapeCoordinates(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                coordinates);
    }

    bool ColliderComponent::UpdateRootHeightfieldHeights(
        const HeightfieldRegion& region,
        const HeightfieldSampleCollection& samples,
        const HeightfieldUpdateConfiguration& configuration)
    {
        return m_system
            && m_system->UpdateHeightfieldHeights(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                region,
                samples.GetSamples(),
                configuration);
    }

    bool ColliderComponent::UpdateRootHeightfieldMaterials(
        const HeightfieldRegion& region,
        const HeightfieldMaterialIndexCollection& materialIndices,
        const MaterialCollection& materials,
        const bool activateBodies)
    {
        return m_system
            && m_system->UpdateHeightfieldMaterials(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                region,
                materialIndices.GetIndices(),
                materials.GetMaterials(),
                activateBodies);
    }

    bool ColliderComponent::GetRootCompoundChildCount(
        AZ::u32& childCount) const
    {
        return m_system
            && m_system->GetCompoundChildCount(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                childCount);
    }

    bool ColliderComponent::GetRootCompoundChild(
        const AZ::u32 childIndex,
        CompoundChildConfiguration& child) const
    {
        return m_system
            && m_system->GetCompoundChild(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                childIndex,
                child);
    }

    bool ColliderComponent::GetRootCompoundChildIndex(
        const SubShapeId subShapeId,
        AZ::u32& childIndex) const
    {
        return m_system
            && m_system->GetCompoundChildIndex(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                subShapeId,
                childIndex);
    }

    bool ColliderComponent::IsRootShapeScaleValid(
        const AZ::Vector3& scale) const
    {
        return m_system
            && m_system->IsShapeScaleValid(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                scale);
    }

    bool ColliderComponent::MakeRootShapeScaleValid(
        const AZ::Vector3& scale,
        AZ::Vector3& validScale) const
    {
        return m_system
            && m_system->MakeShapeScaleValid(
                m_worldHandle,
                m_shapeSet.m_rootShapeHandle,
                scale,
                validScale);
    }

    void ColliderComponent::Activate()
    {
        ColliderRequestBus::Handler::BusConnect(GetEntityId());
    }

    void ColliderComponent::Deactivate()
    {
        DestroyShapes();
        ColliderRequestBus::Handler::BusDisconnect();
    }

    bool ColliderComponent::CreateShapes(
        ISystem& system,
        const WorldHandle worldHandle,
        const float uniformScale)
    {
        if (m_system || !AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f)
        {
            return false;
        }

        Internal::ShapeSet shapeSet;
        if (!Internal::CreateShapeSet(system, worldHandle, m_configurations, uniformScale, shapeSet))
        {
            return false;
        }

        m_system = &system;
        m_worldHandle = worldHandle;
        m_uniformScale = uniformScale;
        m_shapeSet = AZStd::move(shapeSet);
        return true;
    }

    bool ColliderComponent::UpdateUniformScale(
        const BodyHandle bodyHandle,
        const float uniformScale)
    {
        if (!m_system || !bodyHandle || !AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f)
        {
            return false;
        }
        if (AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance))
        {
            return true;
        }

        Internal::ShapeSet replacement;
        if (!Internal::CreateShapeSet(*m_system, m_worldHandle, m_configurations, uniformScale, replacement))
        {
            return false;
        }
        if (!m_system->SetBodyShape(
            m_worldHandle,
            bodyHandle,
            replacement.m_rootShapeHandle,
            true,
            true))
        {
            Internal::DestroyShapeSet(*m_system, m_worldHandle, replacement);
            return false;
        }

        Internal::ShapeSet previous = AZStd::move(m_shapeSet);
        m_shapeSet = AZStd::move(replacement);
        m_uniformScale = uniformScale;
        Internal::DestroyShapeSet(*m_system, m_worldHandle, previous);
        return true;
    }

    bool ColliderComponent::UpdateCharacterUniformScale(
        const CharacterHandle characterHandle,
        const float maximumPenetrationDepth,
        const float uniformScale)
    {
        if (!m_system
            || !characterHandle
            || !AZ::IsFiniteFloat(maximumPenetrationDepth)
            || maximumPenetrationDepth < 0.0f
            || !AZ::IsFiniteFloat(uniformScale)
            || uniformScale <= 0.0f)
        {
            return false;
        }
        if (AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance))
        {
            return true;
        }

        Internal::ShapeSet replacement;
        if (!Internal::CreateShapeSet(*m_system, m_worldHandle, m_configurations, uniformScale, replacement))
        {
            return false;
        }
        if (!m_system->SetCharacterShape(
            m_worldHandle,
            characterHandle,
            replacement.m_rootShapeHandle,
            maximumPenetrationDepth))
        {
            Internal::DestroyShapeSet(*m_system, m_worldHandle, replacement);
            return false;
        }

        Internal::ShapeSet previous = AZStd::move(m_shapeSet);
        m_shapeSet = AZStd::move(replacement);
        m_uniformScale = uniformScale;
        Internal::DestroyShapeSet(*m_system, m_worldHandle, previous);
        return true;
    }

    bool ColliderComponent::UpdateVirtualCharacterUniformScale(
        const VirtualCharacterHandle characterHandle,
        const float maximumPenetrationDepth,
        const bool updateInnerBody,
        const float uniformScale)
    {
        if (!m_system
            || !characterHandle
            || !AZ::IsFiniteFloat(maximumPenetrationDepth)
            || maximumPenetrationDepth < 0.0f
            || !AZ::IsFiniteFloat(uniformScale)
            || uniformScale <= 0.0f)
        {
            return false;
        }
        if (AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance))
        {
            return true;
        }

        Internal::ShapeSet replacement;
        if (!Internal::CreateShapeSet(*m_system, m_worldHandle, m_configurations, uniformScale, replacement))
        {
            return false;
        }
        if (!m_system->SetVirtualCharacterShape(
            m_worldHandle,
            characterHandle,
            replacement.m_rootShapeHandle,
            maximumPenetrationDepth))
        {
            Internal::DestroyShapeSet(*m_system, m_worldHandle, replacement);
            return false;
        }
        if (updateInnerBody
            && !m_system->SetVirtualCharacterInnerBodyShape(
                m_worldHandle,
                characterHandle,
                replacement.m_rootShapeHandle))
        {
            [[maybe_unused]] const bool restored = m_system->SetVirtualCharacterShape(
                m_worldHandle,
                characterHandle,
                m_shapeSet.m_rootShapeHandle,
                maximumPenetrationDepth);
            AZ_Assert(restored, "Failed to restore the virtual-character shape after an inner-body update failure.");
            Internal::DestroyShapeSet(*m_system, m_worldHandle, replacement);
            return false;
        }

        Internal::ShapeSet previous = AZStd::move(m_shapeSet);
        m_shapeSet = AZStd::move(replacement);
        m_uniformScale = uniformScale;
        Internal::DestroyShapeSet(*m_system, m_worldHandle, previous);
        return true;
    }

    void ColliderComponent::DestroyShapes()
    {
        if (!m_system)
        {
            return;
        }

        Internal::DestroyShapeSet(*m_system, m_worldHandle, m_shapeSet);
        m_system = nullptr;
        m_worldHandle = {};
        m_uniformScale = 1.0f;
    }

} // namespace Jolt
