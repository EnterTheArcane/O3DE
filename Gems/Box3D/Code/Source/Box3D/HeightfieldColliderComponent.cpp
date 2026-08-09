/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/HeightfieldColliderComponent.h>

#include <Box3D/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    HeightfieldColliderComponent::HeightfieldColliderComponent(ShapeConfiguration configuration, AZ::Name worldName)
        : m_configuration(AZStd::move(configuration))
        , m_worldName(AZStd::move(worldName))
    {
    }

    void HeightfieldColliderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HeightfieldColliderComponent, AZ::Component>()
                ->Version(1)
                ->Field("Configuration", &HeightfieldColliderComponent::m_configuration)
                ->Field("WorldName", &HeightfieldColliderComponent::m_worldName);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<HeightfieldColliderComponent>("Box3D Heightfield Collider", "Static mutable terrain collision")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HeightfieldColliderComponent::m_worldName, "World", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HeightfieldColliderComponent::m_configuration,
                        "Heightfield",
                        "Geometry, materials, filtering, and events");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<HeightfieldRequestBus>("Box3DHeightfieldRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("EnableSimulation", &HeightfieldRequests::EnableSimulation)
                ->Event("DisableSimulation", &HeightfieldRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &HeightfieldRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &HeightfieldRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &HeightfieldRequests::GetBodyHandle)
                ->Event("GetShapeHandle", &HeightfieldRequests::GetShapeHandle)
                ->Event("GetColumnCount", &HeightfieldRequests::GetColumnCount)
                ->Event("GetRowCount", &HeightfieldRequests::GetRowCount)
                ->Event("GetHeights", &HeightfieldRequests::GetHeightsCopy)
                ->Event("GetMaterialIndices", &HeightfieldRequests::GetMaterialIndicesCopy)
                ->Event("ReplaceHeightfield", &HeightfieldRequests::ReplaceHeightfield)
                ->Event("UpdateHeights", &HeightfieldRequests::UpdateHeightsFromList)
                ->Event("UpdateMaterials", &HeightfieldRequests::UpdateMaterialsFromList);

            behaviorContext->Class<HeightfieldColliderComponent>("Box3D::HeightfieldColliderComponent")
                ->RequestBus("Box3DHeightfieldRequestBus");
        }
    }

    void HeightfieldColliderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DBodyService"));
        provided.push_back(AZ_CRC_CE("Box3DColliderService"));
        provided.push_back(AZ_CRC_CE("Box3DHeightfieldService"));
    }

    void HeightfieldColliderComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DBodyService"));
        incompatible.push_back(AZ_CRC_CE("Box3DColliderService"));
        incompatible.push_back(AZ_CRC_CE("Box3DHeightfieldService"));
        incompatible.push_back(AZ_CRC_CE("Box3DCharacterService"));
    }

    void HeightfieldColliderComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    AZStd::span<const ShapeConfiguration> HeightfieldColliderComponent::GetShapeConfigurations() const
    {
        return { &m_configuration, 1 };
    }

    AZStd::span<const ShapeHandle> HeightfieldColliderComponent::GetShapeHandles() const
    {
        return m_shapeHandle.IsValid() ? AZStd::span<const ShapeHandle>(&m_shapeHandle, 1) : AZStd::span<const ShapeHandle>{};
    }

    AZ::Aabb HeightfieldColliderComponent::GetAabb() const
    {
        return m_system != nullptr ? m_system->GetShapeAabb(m_worldHandle, m_shapeHandle) : AZ::Aabb::CreateNull();
    }

    bool HeightfieldColliderComponent::IsSensor() const
    {
        return m_configuration.m_properties.m_isSensor;
    }

    bool HeightfieldColliderComponent::UpdateShape(size_t index, const ShapeConfiguration& configuration)
    {
        if (index != 0 || !AZStd::holds_alternative<HeightfieldShapeConfiguration>(configuration.m_geometry))
        {
            return false;
        }

        ShapeConfiguration runtimeConfiguration = configuration;
        AZStd::vector<MaterialHandle> replacementMaterials;
        if (m_system != nullptr)
        {
            replacementMaterials.reserve(configuration.m_properties.m_materialConfigurations.size());
            for (const MaterialConfiguration& materialConfiguration : configuration.m_properties.m_materialConfigurations)
            {
                const MaterialHandle materialHandle = m_system->CreateMaterial(materialConfiguration);
                if (!materialHandle.IsValid())
                {
                    for (MaterialHandle createdMaterial : replacementMaterials)
                    {
                        m_system->DestroyMaterial(createdMaterial);
                    }
                    return false;
                }
                replacementMaterials.push_back(materialHandle);
                runtimeConfiguration.m_properties.m_materials.push_back(materialHandle);
            }
            if (!m_system->UpdateShape(m_worldHandle, m_shapeHandle, runtimeConfiguration, m_nonUniformScale * m_uniformScale))
            {
                for (MaterialHandle createdMaterial : replacementMaterials)
                {
                    m_system->DestroyMaterial(createdMaterial);
                }
                return false;
            }
            for (MaterialHandle oldMaterial : m_ownedMaterials)
            {
                m_system->DestroyMaterial(oldMaterial);
            }
            m_ownedMaterials = AZStd::move(replacementMaterials);
        }
        m_configuration = configuration;
        return true;
    }

    bool HeightfieldColliderComponent::SetCollisionFilter(size_t index, const CollisionFilter& collisionFilter)
    {
        if (index != 0 || (m_system != nullptr && !m_system->SetShapeCollisionFilter(m_worldHandle, m_shapeHandle, collisionFilter)))
        {
            return false;
        }
        m_configuration.m_properties.m_collisionFilter = collisionFilter;
        return true;
    }

    bool HeightfieldColliderComponent::SetMaterials(size_t index, AZStd::span<const MaterialHandle> materials)
    {
        if (index != 0 || (m_system != nullptr && !m_system->SetShapeMaterials(m_worldHandle, m_shapeHandle, materials)))
        {
            return false;
        }
        if (m_system != nullptr)
        {
            for (MaterialHandle materialHandle : m_ownedMaterials)
            {
                m_system->DestroyMaterial(materialHandle);
            }
            m_ownedMaterials.clear();
        }
        m_configuration.m_properties.m_materialConfigurations.clear();
        m_configuration.m_properties.m_materials.assign(materials.begin(), materials.end());
        return true;
    }

    bool HeightfieldColliderComponent::EnableSimulation()
    {
        if (m_shapeHandle.IsValid())
        {
            return true;
        }
        if (m_system == nullptr || !AZStd::holds_alternative<HeightfieldShapeConfiguration>(m_configuration.m_geometry))
        {
            return false;
        }

        m_worldHandle = m_worldName.IsEmpty() ? m_system->GetDefaultWorldHandle() : m_system->FindWorld(m_worldName);
        if (!m_worldHandle.IsValid())
        {
            return false;
        }
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_entityId = GetEntityId();
        bodyConfiguration.m_name = AZ::Name(GetEntity()->GetName());
        AZ::TransformBus::EventResult(bodyConfiguration.m_transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        m_uniformScale = bodyConfiguration.m_transform.ExtractUniformScale();
        m_bodyHandle = m_system->CreateBody(m_worldHandle, bodyConfiguration);
        if (!m_bodyHandle.IsValid())
        {
            return false;
        }

        ShapeConfiguration runtimeConfiguration = m_configuration;
        m_ownedMaterials.reserve(m_configuration.m_properties.m_materialConfigurations.size());
        for (const MaterialConfiguration& materialConfiguration : m_configuration.m_properties.m_materialConfigurations)
        {
            const MaterialHandle materialHandle = m_system->CreateMaterial(materialConfiguration);
            if (!materialHandle.IsValid())
            {
                DisableSimulation();
                return false;
            }
            m_ownedMaterials.push_back(materialHandle);
            runtimeConfiguration.m_properties.m_materials.push_back(materialHandle);
        }
        m_shapeHandle = m_system->CreateShape(m_worldHandle, m_bodyHandle, runtimeConfiguration, m_nonUniformScale * m_uniformScale);
        if (!m_shapeHandle.IsValid())
        {
            DisableSimulation();
            return false;
        }
        return true;
    }

    bool HeightfieldColliderComponent::DisableSimulation()
    {
        if (m_system != nullptr && m_shapeHandle.IsValid())
        {
            if (!m_system->DestroyShape(m_worldHandle, m_shapeHandle))
            {
                return false;
            }
            m_shapeHandle = {};
            for (MaterialHandle materialHandle : m_ownedMaterials)
            {
                m_system->DestroyMaterial(materialHandle);
            }
            m_ownedMaterials.clear();
        }
        if (m_system != nullptr && m_bodyHandle.IsValid())
        {
            if (!m_system->DestroyBody(m_worldHandle, m_bodyHandle))
            {
                return false;
            }
            m_bodyHandle = {};
            m_uniformScale = 1.0f;
        }
        return true;
    }

    bool HeightfieldColliderComponent::IsSimulationEnabled() const
    {
        return m_system != nullptr && m_shapeHandle.IsValid();
    }

    WorldHandle HeightfieldColliderComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle HeightfieldColliderComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    ShapeHandle HeightfieldColliderComponent::GetShapeHandle() const
    {
        return m_shapeHandle;
    }

    AZ::u32 HeightfieldColliderComponent::GetColumnCount() const
    {
        const auto* heightfield = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        return heightfield != nullptr ? heightfield->m_columnCount : 0;
    }

    AZ::u32 HeightfieldColliderComponent::GetRowCount() const
    {
        const auto* heightfield = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        return heightfield != nullptr ? heightfield->m_rowCount : 0;
    }

    AZStd::span<const float> HeightfieldColliderComponent::GetHeights() const
    {
        const auto* heightfield = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        return heightfield != nullptr ? AZStd::span<const float>(heightfield->m_samples) : AZStd::span<const float>{};
    }

    AZStd::span<const AZ::u8> HeightfieldColliderComponent::GetMaterialIndices() const
    {
        const auto* heightfield = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        return heightfield != nullptr ? AZStd::span<const AZ::u8>(heightfield->m_materialIndices) : AZStd::span<const AZ::u8>{};
    }

    bool HeightfieldColliderComponent::ReplaceHeightfield(const HeightfieldShapeConfiguration& configuration)
    {
        ShapeConfiguration candidate = m_configuration;
        candidate.m_geometry = configuration;
        return UpdateShape(0, candidate);
    }

    bool HeightfieldColliderComponent::UpdateHeights(
        AZ::u32 startColumn, AZ::u32 startRow, AZ::u32 columnCount, AZ::u32 rowCount, AZStd::span<const float> heights)
    {
        const auto* current = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        if (current == nullptr || columnCount == 0 || rowCount == 0 || columnCount > current->m_columnCount ||
            rowCount > current->m_rowCount || startColumn > current->m_columnCount - columnCount ||
            startRow > current->m_rowCount - rowCount || heights.size() != aznumeric_cast<size_t>(columnCount) * rowCount ||
            current->m_samples.size() != aznumeric_cast<size_t>(current->m_columnCount) * current->m_rowCount)
        {
            return false;
        }

        HeightfieldShapeConfiguration candidate = *current;
        for (AZ::u32 row = 0; row < rowCount; ++row)
        {
            const size_t destination = aznumeric_cast<size_t>(startRow + row) * current->m_columnCount + startColumn;
            const size_t source = aznumeric_cast<size_t>(row) * columnCount;
            AZStd::copy(heights.begin() + source, heights.begin() + source + columnCount, candidate.m_samples.begin() + destination);
        }
        return ReplaceHeightfield(candidate);
    }

    bool HeightfieldColliderComponent::UpdateMaterials(
        AZ::u32 startColumn, AZ::u32 startRow, AZ::u32 columnCount, AZ::u32 rowCount, AZStd::span<const AZ::u8> materialIndices)
    {
        const auto* current = AZStd::get_if<HeightfieldShapeConfiguration>(&m_configuration.m_geometry);
        if (current == nullptr || current->m_columnCount < 2 || current->m_rowCount < 2)
        {
            return false;
        }
        const AZ::u32 materialColumnCount = current->m_columnCount - 1;
        const AZ::u32 materialRowCount = current->m_rowCount - 1;
        if (columnCount == 0 || rowCount == 0 || columnCount > materialColumnCount || rowCount > materialRowCount ||
            startColumn > materialColumnCount - columnCount || startRow > materialRowCount - rowCount ||
            materialIndices.size() != aznumeric_cast<size_t>(columnCount) * rowCount)
        {
            return false;
        }

        HeightfieldShapeConfiguration candidate = *current;
        if (!candidate.m_materialIndices.empty() &&
            candidate.m_materialIndices.size() != aznumeric_cast<size_t>(materialColumnCount) * materialRowCount)
        {
            return false;
        }
        if (candidate.m_materialIndices.empty())
        {
            candidate.m_materialIndices.resize(aznumeric_cast<size_t>(materialColumnCount) * materialRowCount);
        }
        for (AZ::u32 row = 0; row < rowCount; ++row)
        {
            const size_t destination = aznumeric_cast<size_t>(startRow + row) * materialColumnCount + startColumn;
            const size_t source = aznumeric_cast<size_t>(row) * columnCount;
            AZStd::copy(
                materialIndices.begin() + source,
                materialIndices.begin() + source + columnCount,
                candidate.m_materialIndices.begin() + destination);
        }
        return ReplaceHeightfield(candidate);
    }

    void HeightfieldColliderComponent::Activate()
    {
        m_system = azrtti_cast<System*>(AZ::Interface<ISystem>::Get());
        m_nonUniformScale = AZ::Vector3::CreateOne();
        AZ::NonUniformScaleRequestBus::EventResult(m_nonUniformScale, GetEntityId(), &AZ::NonUniformScaleRequests::GetScale);
        AZ::NonUniformScaleRequestBus::Event(
            GetEntityId(), &AZ::NonUniformScaleRequests::RegisterScaleChangedEvent, m_nonUniformScaleChangedHandler);
        ColliderRequestBus::Handler::BusConnect(GetEntityId());
        HeightfieldRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        EnableSimulation();
    }

    void HeightfieldColliderComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        HeightfieldRequestBus::Handler::BusDisconnect();
        ColliderRequestBus::Handler::BusDisconnect();
        DisableSimulation();
        m_nonUniformScaleChangedHandler.Disconnect();
        m_system = nullptr;
        m_worldHandle = {};
    }

    void HeightfieldColliderComponent::OnTransformChanged([[maybe_unused]] const AZ::Transform& local, const AZ::Transform& world)
    {
        if (m_system != nullptr && m_bodyHandle.IsValid())
        {
            AZ::Transform pose = world;
            const float uniformScale = pose.ExtractUniformScale();
            if (!AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance))
            {
                if (!UpdateScale(m_nonUniformScale * uniformScale))
                {
                    return;
                }
                m_uniformScale = uniformScale;
            }
            [[maybe_unused]] const bool transformSet = m_system->SetBodyTransform(m_worldHandle, m_bodyHandle, pose);
        }
    }

    bool HeightfieldColliderComponent::UpdateScale(const AZ::Vector3& scale)
    {
        if (!scale.IsFinite() || scale.GetMinElement() < 0.01f)
        {
            return false;
        }
        if (m_system == nullptr || !m_shapeHandle.IsValid())
        {
            return true;
        }

        ShapeConfiguration runtimeConfiguration = m_configuration;
        runtimeConfiguration.m_properties.m_materials.insert(
            runtimeConfiguration.m_properties.m_materials.end(), m_ownedMaterials.begin(), m_ownedMaterials.end());
        return m_system->UpdateShape(m_worldHandle, m_shapeHandle, runtimeConfiguration, scale);
    }
} // namespace Box3D
