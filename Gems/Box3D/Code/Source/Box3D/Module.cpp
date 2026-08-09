/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

#include <Box3D/CharacterControllerComponent.h>
#include <Box3D/ColliderComponent.h>
#include <Box3D/EffectComponents.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/JointComponent.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/StaticRigidBodyComponent.h>
#include <Box3D/SystemComponent.h>
#include <Box3D/TypeIds.h>

namespace Box3D
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, ModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.push_back(SystemComponent::CreateDescriptor());
            m_descriptors.push_back(CharacterControllerComponent::CreateDescriptor());
            m_descriptors.push_back(ColliderComponent::CreateDescriptor());
            m_descriptors.push_back(ExplosionComponent::CreateDescriptor());
            m_descriptors.push_back(WindComponent::CreateDescriptor());
            m_descriptors.push_back(HeightfieldColliderComponent::CreateDescriptor());
            m_descriptors.push_back(JointComponent::CreateDescriptor());
            m_descriptors.push_back(RigidBodyComponent::CreateDescriptor());
            m_descriptors.push_back(StaticRigidBodyComponent::CreateDescriptor());
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<SystemComponent>() };
        }
    };
} // namespace Box3D

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Box3D::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Box3D, Box3D::Module)
#endif
