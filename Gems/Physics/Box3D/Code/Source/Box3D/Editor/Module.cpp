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
#include <Box3D/Editor/CharacterControllerComponent.h>
#include <Box3D/Editor/ColliderComponent.h>
#include <Box3D/Editor/EffectComponents.h>
#include <Box3D/Editor/HeightfieldColliderComponent.h>
#include <Box3D/Editor/JointComponents.h>
#include <Box3D/Editor/RigidBodyComponent.h>
#include <Box3D/Editor/StaticRigidBodyComponent.h>
#include <Box3D/EffectComponents.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/JointComponent.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/StaticRigidBodyComponent.h>
#include <Box3D/SystemComponent.h>
#include <Box3D/TypeIds.h>

namespace Box3D::Editor
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, ModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    Box3D::SystemComponent::CreateDescriptor(),
                    Box3D::ColliderComponent::CreateDescriptor(),
                    Box3D::HeightfieldColliderComponent::CreateDescriptor(),
                    Box3D::CharacterControllerComponent::CreateDescriptor(),
                    Box3D::ExplosionComponent::CreateDescriptor(),
                    Box3D::WindComponent::CreateDescriptor(),
                    Box3D::JointComponent::CreateDescriptor(),
                    Box3D::RigidBodyComponent::CreateDescriptor(),
                    Box3D::StaticRigidBodyComponent::CreateDescriptor(),
                    ColliderComponent::CreateDescriptor(),
                    HeightfieldColliderComponent::CreateDescriptor(),
                    CharacterControllerComponent::CreateDescriptor(),
                    ExplosionComponent::CreateDescriptor(),
                    WindComponent::CreateDescriptor(),
                    ParallelJointComponent::CreateDescriptor(),
                    DistanceJointComponent::CreateDescriptor(),
                    FilterJointComponent::CreateDescriptor(),
                    MotorJointComponent::CreateDescriptor(),
                    PrismaticJointComponent::CreateDescriptor(),
                    RevoluteJointComponent::CreateDescriptor(),
                    SphericalJointComponent::CreateDescriptor(),
                    WeldJointComponent::CreateDescriptor(),
                    WheelJointComponent::CreateDescriptor(),
                    RigidBodyComponent::CreateDescriptor(),
                    StaticRigidBodyComponent::CreateDescriptor(),
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return {azrtti_typeid<Box3D::SystemComponent>()};
        }
    };
} // namespace Box3D::Editor

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(
    AZ_JOIN(Gem_, O3DE_GEM_NAME),
    Box3D::Editor::Module)
#else
AZ_DECLARE_MODULE_CLASS(
    Gem_Box3D,
    Box3D::Editor::Module)
#endif
