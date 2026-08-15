/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/Editor/CharacterControllerComponent.h>
#include <Jolt/Editor/ConstraintComponent.h>
#include <Jolt/Editor/ColliderComponent.h>
#include <Jolt/Editor/HairComponent.h>
#include <Jolt/Editor/PathComponent.h>
#include <Jolt/Editor/RagdollComponent.h>
#include <Jolt/Editor/RigidBodyComponent.h>
#include <Jolt/Editor/SceneComponent.h>
#include <Jolt/Editor/AssetBuilder.h>
#include <Jolt/Editor/SkeletonComponent.h>
#include <Jolt/Editor/SoftBodyComponent.h>
#include <Jolt/Editor/StaticRigidBodyComponent.h>
#include <Jolt/Editor/VehicleComponents.h>
#include <Jolt/Editor/VirtualCharacterControllerComponent.h>
#include <Jolt/RigidBodyComponent.h>
#include <Jolt/SceneComponent.h>
#include <Jolt/SkeletonComponent.h>
#include <Jolt/HairComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/PathComponent.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/StaticRigidBodyComponent.h>
#include <Jolt/SystemComponent.h>
#include <Jolt/TypeIds.h>
#include <Jolt/VehicleComponents.h>
#include <Jolt/VirtualCharacterControllerComponent.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace Jolt::Editor
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, EditorModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    Jolt::SystemComponent::CreateDescriptor(),
                    Jolt::CharacterControllerComponent::CreateDescriptor(),
                    Jolt::ConstraintComponent::CreateDescriptor(),
                    Jolt::ColliderComponent::CreateDescriptor(),
                    Jolt::HairComponent::CreateDescriptor(),
                    Jolt::PathComponent::CreateDescriptor(),
                    Jolt::RagdollComponent::CreateDescriptor(),
                    Jolt::RigidBodyComponent::CreateDescriptor(),
                    Jolt::SceneComponent::CreateDescriptor(),
                    Jolt::SoftBodyComponent::CreateDescriptor(),
                    Jolt::SkeletonComponent::CreateDescriptor(),
                    Jolt::StaticRigidBodyComponent::CreateDescriptor(),
                    Jolt::WheeledVehicleComponent::CreateDescriptor(),
                    Jolt::MotorcycleComponent::CreateDescriptor(),
                    Jolt::TrackedVehicleComponent::CreateDescriptor(),
                    Jolt::VirtualCharacterControllerComponent::CreateDescriptor(),
                    ColliderComponent::CreateDescriptor(),
                    HairComponent::CreateDescriptor(),
                    PathComponent::CreateDescriptor(),
                    RagdollComponent::CreateDescriptor(),
                    CharacterControllerComponent::CreateDescriptor(),
                    ConstraintComponent::CreateDescriptor(),
                    RigidBodyComponent::CreateDescriptor(),
                    SceneComponent::CreateDescriptor(),
                    SoftBodyComponent::CreateDescriptor(),
                    SkeletonComponent::CreateDescriptor(),
                    StaticRigidBodyComponent::CreateDescriptor(),
                    WheeledVehicleComponent::CreateDescriptor(),
                    MotorcycleComponent::CreateDescriptor(),
                    TrackedVehicleComponent::CreateDescriptor(),
                    VirtualCharacterControllerComponent::CreateDescriptor(),
                    BuilderComponent::CreateDescriptor(),
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return {
                azrtti_typeid<Jolt::SystemComponent>(),
                azrtti_typeid<BuilderComponent>(),
            };
        }
    };
} // namespace Jolt::Editor

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Jolt::Editor::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Jolt, Jolt::Editor::Module)
#endif
