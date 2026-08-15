/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/HairComponent.h>
#include <Jolt/PathComponent.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/RigidBodyComponent.h>
#include <Jolt/SceneComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/SkeletonComponent.h>
#include <Jolt/StaticRigidBodyComponent.h>
#include <Jolt/SystemComponent.h>
#include <Jolt/TypeIds.h>
#include <Jolt/VehicleComponents.h>
#include <Jolt/VirtualCharacterControllerComponent.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace Jolt
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
            m_descriptors.push_back(ColliderComponent::CreateDescriptor());
            m_descriptors.push_back(ConstraintComponent::CreateDescriptor());
            m_descriptors.push_back(CharacterControllerComponent::CreateDescriptor());
            m_descriptors.push_back(HairComponent::CreateDescriptor());
            m_descriptors.push_back(PathComponent::CreateDescriptor());
            m_descriptors.push_back(RagdollComponent::CreateDescriptor());
            m_descriptors.push_back(RigidBodyComponent::CreateDescriptor());
            m_descriptors.push_back(SceneComponent::CreateDescriptor());
            m_descriptors.push_back(SoftBodyComponent::CreateDescriptor());
            m_descriptors.push_back(SkeletonComponent::CreateDescriptor());
            m_descriptors.push_back(StaticRigidBodyComponent::CreateDescriptor());
            m_descriptors.push_back(WheeledVehicleComponent::CreateDescriptor());
            m_descriptors.push_back(MotorcycleComponent::CreateDescriptor());
            m_descriptors.push_back(TrackedVehicleComponent::CreateDescriptor());
            m_descriptors.push_back(VirtualCharacterControllerComponent::CreateDescriptor());
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return {azrtti_typeid<SystemComponent>()};
        }
    };
} // namespace Jolt

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Jolt::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Jolt, Jolt::Module)
#endif
