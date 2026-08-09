/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/ColliderComponent.h>

#include <Box3D/SystemInternal.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    ColliderComponent::ColliderComponent(ShapeConfiguration shapeConfiguration)
        : m_shapeConfigurations{ AZStd::move(shapeConfiguration) }
    {
    }

    ColliderComponent::ColliderComponent(AZStd::vector<ShapeConfiguration> shapeConfigurations)
        : m_shapeConfigurations(AZStd::move(shapeConfigurations))
    {
    }

    void ColliderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ColliderComponent, AZ::Component>()->Version(1)->Field(
                "Shapes", &ColliderComponent::m_shapeConfigurations);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<ColliderComponent>("Box3D Collider", "Attaches one or more shapes to a Box3D body")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ColliderComponent::m_shapeConfigurations, "Shapes", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<ColliderRequestBus>("Box3DColliderRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("GetShapeCount", &ColliderRequests::GetShapeCount)
                ->Event("GetShapeHandleAt", &ColliderRequests::GetShapeHandleAt)
                ->Event("GetAabb", &ColliderRequests::GetAabb)
                ->Event("IsSensor", &ColliderRequests::IsSensor)
                ->Event("UpdateSphere", &ColliderRequests::UpdateSphere)
                ->Event("UpdateCapsule", &ColliderRequests::UpdateCapsule)
                ->Event("UpdateBox", &ColliderRequests::UpdateBox)
                ->Event("UpdateCylinder", &ColliderRequests::UpdateCylinder)
                ->Event("UpdateConvexHull", &ColliderRequests::UpdateConvexHull)
                ->Event("UpdateTriangleMesh", &ColliderRequests::UpdateTriangleMesh)
                ->Event("UpdateHeightfield", &ColliderRequests::UpdateHeightfield)
                ->Event("UpdateCompound", &ColliderRequests::UpdateCompound)
                ->Event("SetCollisionFilter", &ColliderRequests::SetCollisionFilter)
                ->Event("SetMaterials", &ColliderRequests::SetMaterialsFromCollection);

            behaviorContext->Class<ColliderComponent>("Box3D::ColliderComponent")->RequestBus("Box3DColliderRequestBus");
        }
    }

    void ColliderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DColliderService"));
    }

    void ColliderComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DColliderService"));
    }

    AZStd::span<const ShapeConfiguration> ColliderComponent::GetShapeConfigurations() const
    {
        return m_shapeConfigurations;
    }

    AZStd::span<const ShapeHandle> ColliderComponent::GetShapeHandles() const
    {
        return m_shapeHandles;
    }

    AZ::Aabb ColliderComponent::GetAabb() const
    {
        AZ::Aabb result = AZ::Aabb::CreateNull();
        if (m_system == nullptr)
        {
            return result;
        }
        for (ShapeHandle shapeHandle : m_shapeHandles)
        {
            result.AddAabb(m_system->GetShapeAabb(m_worldHandle, shapeHandle));
        }
        return result;
    }

    bool ColliderComponent::IsSensor() const
    {
        return AZStd::any_of(
            m_shapeConfigurations.begin(),
            m_shapeConfigurations.end(),
            [](const ShapeConfiguration& configuration)
            {
                return configuration.m_properties.m_isSensor;
            });
    }

    bool ColliderComponent::UpdateShape(const size_t index, const ShapeConfiguration& configuration)
    {
        if (index >= m_shapeConfigurations.size())
        {
            return false;
        }
        AZStd::vector<MaterialHandle> replacementMaterials;
        ShapeConfiguration runtimeConfiguration{ configuration.m_geometry, configuration.m_properties };
        if (m_system != nullptr)
        {
            replacementMaterials.reserve(configuration.m_materialConfigurations.size());
            for (const MaterialConfiguration& materialConfiguration : configuration.m_materialConfigurations)
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
            if (!m_system->UpdateShape(m_worldHandle, m_shapeHandles[index], runtimeConfiguration, m_uniformScale))
            {
                for (MaterialHandle createdMaterial : replacementMaterials)
                {
                    m_system->DestroyMaterial(createdMaterial);
                }
                return false;
            }
            for (MaterialHandle oldMaterial : m_ownedMaterials[index])
            {
                m_system->DestroyMaterial(oldMaterial);
            }
            m_ownedMaterials[index] = AZStd::move(replacementMaterials);
        }
        m_shapeConfigurations[index] = configuration;
        return true;
    }

    bool ColliderComponent::SetCollisionFilter(const size_t index, const CollisionFilter& collisionFilter)
    {
        if (index >= m_shapeConfigurations.size())
        {
            return false;
        }
        if (m_system != nullptr && !m_system->SetShapeCollisionFilter(m_worldHandle, m_shapeHandles[index], collisionFilter))
        {
            return false;
        }
        m_shapeConfigurations[index].m_properties.m_collisionFilter = collisionFilter;
        return true;
    }

    bool ColliderComponent::SetMaterials(const size_t index, AZStd::span<const MaterialHandle> materials)
    {
        if (index >= m_shapeConfigurations.size())
        {
            return false;
        }
        if (m_system != nullptr && !m_system->SetShapeMaterials(m_worldHandle, m_shapeHandles[index], materials))
        {
            return false;
        }
        if (m_system != nullptr)
        {
            for (MaterialHandle materialHandle : m_ownedMaterials[index])
            {
                m_system->DestroyMaterial(materialHandle);
            }
            m_ownedMaterials[index].clear();
        }
        m_shapeConfigurations[index].m_materialConfigurations.clear();
        m_shapeConfigurations[index].m_properties.m_materials.assign(materials.begin(), materials.end());
        return true;
    }

    void ColliderComponent::Activate()
    {
        ColliderRequestBus::Handler::BusConnect(GetEntityId());
    }

    void ColliderComponent::Deactivate()
    {
        Detach();
        ColliderRequestBus::Handler::BusDisconnect();
    }

    bool ColliderComponent::Attach(ISystem& system, const WorldHandle worldHandle, const BodyHandle bodyHandle, const float uniformScale)
    {
        if (!AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f)
        {
            return false;
        }
        System* concreteSystem = azrtti_cast<System*>(&system);
        if (concreteSystem == nullptr)
        {
            return false;
        }
        Detach();
        m_system = concreteSystem;
        m_worldHandle = worldHandle;
        m_bodyHandle = bodyHandle;
        m_uniformScale = uniformScale;
        m_shapeHandles.reserve(m_shapeConfigurations.size());
        m_ownedMaterials.resize(m_shapeConfigurations.size());
        for (size_t shapeIndex = 0; shapeIndex < m_shapeConfigurations.size(); ++shapeIndex)
        {
            const ShapeConfiguration& configuration = m_shapeConfigurations[shapeIndex];
            ShapeConfiguration runtimeConfiguration{ configuration.m_geometry, configuration.m_properties };
            AZStd::vector<MaterialHandle>& ownedMaterials = m_ownedMaterials[shapeIndex];
            ownedMaterials.reserve(configuration.m_materialConfigurations.size());
            for (const MaterialConfiguration& materialConfiguration : configuration.m_materialConfigurations)
            {
                const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
                if (!materialHandle.IsValid())
                {
                    Detach();
                    return false;
                }
                ownedMaterials.push_back(materialHandle);
                runtimeConfiguration.m_properties.m_materials.push_back(materialHandle);
            }
            const ShapeHandle shapeHandle = m_system->CreateShape(worldHandle, bodyHandle, runtimeConfiguration, m_uniformScale);
            if (!shapeHandle.IsValid())
            {
                Detach();
                return false;
            }
            m_shapeHandles.push_back(shapeHandle);
        }
        return true;
    }

    bool ColliderComponent::UpdateUniformScale(const float uniformScale)
    {
        if (!AZ::IsFiniteFloat(uniformScale) || uniformScale <= 0.0f)
        {
            return false;
        }
        if (AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance) || m_system == nullptr)
        {
            m_uniformScale = uniformScale;
            return true;
        }

        const float previousScale = m_uniformScale;
        size_t updatedShapeCount = 0;
        for (; updatedShapeCount < m_shapeConfigurations.size(); ++updatedShapeCount)
        {
            const ShapeConfiguration& authoredConfiguration = m_shapeConfigurations[updatedShapeCount];
            ShapeConfiguration runtimeConfiguration{ authoredConfiguration.m_geometry, authoredConfiguration.m_properties };
            runtimeConfiguration.m_properties.m_materials.insert(
                runtimeConfiguration.m_properties.m_materials.end(),
                m_ownedMaterials[updatedShapeCount].begin(),
                m_ownedMaterials[updatedShapeCount].end());
            if (!m_system->UpdateShape(m_worldHandle, m_shapeHandles[updatedShapeCount], runtimeConfiguration, uniformScale))
            {
                break;
            }
        }
        if (updatedShapeCount == m_shapeConfigurations.size())
        {
            m_uniformScale = uniformScale;
            return true;
        }

        while (updatedShapeCount > 0)
        {
            --updatedShapeCount;
            const ShapeConfiguration& authoredConfiguration = m_shapeConfigurations[updatedShapeCount];
            ShapeConfiguration runtimeConfiguration{ authoredConfiguration.m_geometry, authoredConfiguration.m_properties };
            runtimeConfiguration.m_properties.m_materials.insert(
                runtimeConfiguration.m_properties.m_materials.end(),
                m_ownedMaterials[updatedShapeCount].begin(),
                m_ownedMaterials[updatedShapeCount].end());
            [[maybe_unused]] const bool restored =
                m_system->UpdateShape(m_worldHandle, m_shapeHandles[updatedShapeCount], runtimeConfiguration, previousScale);
        }
        return false;
    }

    void ColliderComponent::Detach()
    {
        if (m_system != nullptr)
        {
            for (ShapeHandle shapeHandle : m_shapeHandles)
            {
                m_system->DestroyShape(m_worldHandle, shapeHandle);
            }
            for (const AZStd::vector<MaterialHandle>& materials : m_ownedMaterials)
            {
                for (MaterialHandle materialHandle : materials)
                {
                    m_system->DestroyMaterial(materialHandle);
                }
            }
        }
        m_shapeHandles.clear();
        m_ownedMaterials.clear();
        m_system = nullptr;
        m_worldHandle = {};
        m_bodyHandle = {};
        m_uniformScale = 1.0f;
    }
} // namespace Box3D
