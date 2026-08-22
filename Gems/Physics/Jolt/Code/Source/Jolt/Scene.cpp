/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

#include <Jolt/HandleEncoding.h>
#include <Jolt/Profiler.h>
#include <Jolt/World.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        bool IsDependencyIndexValid(
            const AZ::u32 dependencyIndex,
            const AZ::u32 constraintIndex)
        {
            return dependencyIndex < constraintIndex;
        }

        [[nodiscard]]
        bool ValidateConstraintDependencies(
            const SceneConstraintConfiguration& configuration,
            const AZStd::span<const SceneConstraintConfiguration> constraints,
            const AZ::u32 constraintIndex)
        {
            return AZStd::visit(
                [&](const auto& geometry)
                {
                    using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                    if constexpr (AZStd::is_same_v<Geometry, GearConstraintConfiguration>)
                    {
                        if (geometry.m_firstHingeConstraintHandle
                            || geometry.m_secondHingeConstraintHandle
                            || !IsDependencyIndexValid(configuration.m_firstDependencyIndex, constraintIndex)
                            || !IsDependencyIndexValid(configuration.m_secondDependencyIndex, constraintIndex))
                        {
                            return false;
                        }

                        return AZStd::holds_alternative<HingeConstraintConfiguration>(
                                   constraints[configuration.m_firstDependencyIndex].m_constraint.m_geometry)
                            && AZStd::holds_alternative<HingeConstraintConfiguration>(
                                constraints[configuration.m_secondDependencyIndex].m_constraint.m_geometry);
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintConfiguration>)
                    {
                        if (geometry.m_pinionConstraintHandle
                            || geometry.m_rackConstraintHandle
                            || !IsDependencyIndexValid(configuration.m_firstDependencyIndex, constraintIndex)
                            || !IsDependencyIndexValid(configuration.m_secondDependencyIndex, constraintIndex))
                        {
                            return false;
                        }

                        return AZStd::holds_alternative<HingeConstraintConfiguration>(
                                   constraints[configuration.m_firstDependencyIndex].m_constraint.m_geometry)
                            && AZStd::holds_alternative<SliderConstraintConfiguration>(
                                constraints[configuration.m_secondDependencyIndex].m_constraint.m_geometry);
                    }
                    else
                    {
                        return configuration.m_firstDependencyIndex == InvalidSceneIndex
                            && configuration.m_secondDependencyIndex == InvalidSceneIndex;
                    }
                },
                configuration.m_constraint.m_geometry);
        }

        void ResolveConstraintDependencies(
            ConstraintConfiguration& configuration,
            const SceneConstraintConfiguration& sceneConfiguration,
            const AZStd::span<const ConstraintHandle> constraintHandles)
        {
            AZStd::visit(
                [&](auto& geometry)
                {
                    using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                    if constexpr (AZStd::is_same_v<Geometry, GearConstraintConfiguration>)
                    {
                        geometry.m_firstHingeConstraintHandle =
                            constraintHandles[sceneConfiguration.m_firstDependencyIndex];
                        geometry.m_secondHingeConstraintHandle =
                            constraintHandles[sceneConfiguration.m_secondDependencyIndex];
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, RackAndPinionConstraintConfiguration>)
                    {
                        geometry.m_pinionConstraintHandle =
                            constraintHandles[sceneConfiguration.m_firstDependencyIndex];
                        geometry.m_rackConstraintHandle =
                            constraintHandles[sceneConfiguration.m_secondDependencyIndex];
                    }
                },
                configuration.m_geometry);
        }
    } // namespace

    void SceneConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<SceneRigidBodyConfiguration>()
                ->Field("CookedShapeHandle", &SceneRigidBodyConfiguration::m_cookedShapeHandle)
                ->Field("Body", &SceneRigidBodyConfiguration::m_body);

            serializeContext
                ->Class<SceneConstraintConfiguration>()
                ->Field("Constraint", &SceneConstraintConfiguration::m_constraint)
                ->Field("FirstBodyIndex", &SceneConstraintConfiguration::m_firstBodyIndex)
                ->Field("SecondBodyIndex", &SceneConstraintConfiguration::m_secondBodyIndex)
                ->Field("FirstDependencyIndex", &SceneConstraintConfiguration::m_firstDependencyIndex)
                ->Field("SecondDependencyIndex", &SceneConstraintConfiguration::m_secondDependencyIndex);

            serializeContext
                ->Class<SceneConfiguration>()
                ->Field("Bodies", &SceneConfiguration::m_bodies)
                ->Field("Constraints", &SceneConfiguration::m_constraints)
                ->Field("Name", &SceneConfiguration::m_name);

            serializeContext
                ->Class<SceneDefinitionState>()
                ->Field("Name", &SceneDefinitionState::m_name)
                ->Field("BodyCount", &SceneDefinitionState::m_bodyCount)
                ->Field("RigidBodyCount", &SceneDefinitionState::m_rigidBodyCount)
                ->Field("SoftBodyCount", &SceneDefinitionState::m_softBodyCount)
                ->Field("ConstraintCount", &SceneDefinitionState::m_constraintCount)
                ->Field("InstanceCount", &SceneDefinitionState::m_instanceCount);

            serializeContext
                ->Class<SceneInstanceState>()
                ->Field("DefinitionHandle", &SceneInstanceState::m_definitionHandle)
                ->Field("BodyCount", &SceneInstanceState::m_bodyCount)
                ->Field("RigidBodyCount", &SceneInstanceState::m_rigidBodyCount)
                ->Field("SoftBodyCount", &SceneInstanceState::m_softBodyCount)
                ->Field("ConstraintCount", &SceneInstanceState::m_constraintCount);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SceneDefinitionState>("SceneDefinitionState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("name", BehaviorValueGetter(&SceneDefinitionState::m_name), nullptr)
                ->Property("bodyCount", BehaviorValueGetter(&SceneDefinitionState::m_bodyCount), nullptr)
                ->Property("rigidBodyCount", BehaviorValueGetter(&SceneDefinitionState::m_rigidBodyCount), nullptr)
                ->Property("softBodyCount", BehaviorValueGetter(&SceneDefinitionState::m_softBodyCount), nullptr)
                ->Property("constraintCount", BehaviorValueGetter(&SceneDefinitionState::m_constraintCount), nullptr)
                ->Property("instanceCount", BehaviorValueGetter(&SceneDefinitionState::m_instanceCount), nullptr);

            behaviorContext->Class<SceneInstanceState>("SceneInstanceState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("definitionHandle", BehaviorValueGetter(&SceneInstanceState::m_definitionHandle), nullptr)
                ->Property("bodyCount", BehaviorValueGetter(&SceneInstanceState::m_bodyCount), nullptr)
                ->Property("rigidBodyCount", BehaviorValueGetter(&SceneInstanceState::m_rigidBodyCount), nullptr)
                ->Property("softBodyCount", BehaviorValueGetter(&SceneInstanceState::m_softBodyCount), nullptr)
                ->Property("constraintCount", BehaviorValueGetter(&SceneInstanceState::m_constraintCount), nullptr);
        }
    }

    SceneDefinitionHandle RuntimeImplementation::CreateSceneDefinition(
        const SceneConfiguration& configuration)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::CreateSceneDefinition");
        if (configuration.m_bodies.size() > Internal::MaximumWorldMemberIndex
            || configuration.m_constraints.size() > Internal::MaximumWorldMemberIndex)
        {
            return {};
        }

        AZStd::vector<CookedShapeHandle> cookedShapeHandles;
        AZStd::vector<GroupFilterHandle> groupFilterHandles;
        AZStd::vector<PathHandle> pathHandles;
        AZStd::vector<SoftBodyDefinitionHandle> softBodyDefinitionHandles;
        AZStd::unordered_set<CookedShapeHandle> retainedCookedShapes;
        AZStd::unordered_set<GroupFilterHandle> retainedGroupFilters;
        AZStd::unordered_set<PathHandle> retainedPaths;
        AZStd::unordered_set<SoftBodyDefinitionHandle> retainedSoftBodyDefinitions;
        cookedShapeHandles.reserve(configuration.m_bodies.size());
        groupFilterHandles.reserve(configuration.m_bodies.size());
        softBodyDefinitionHandles.reserve(configuration.m_bodies.size());

        const auto releaseDependencies = [&]()
        {
            for (const PathHandle pathHandle : pathHandles)
            {
                ReleasePath(pathHandle);
            }
            for (const SoftBodyDefinitionHandle definitionHandle : softBodyDefinitionHandles)
            {
                ReleaseSoftBodyDefinition(definitionHandle);
            }
            for (const GroupFilterHandle filterHandle : groupFilterHandles)
            {
                ReleaseGroupFilter(filterHandle);
            }
            for (const CookedShapeHandle shapeHandle : cookedShapeHandles)
            {
                ReleaseCookedShape(shapeHandle);
            }
        };

        for (const SceneBodyConfiguration& body : configuration.m_bodies)
        {
            bool acquired = AZStd::visit(
                [&](const auto& bodyConfiguration)
                {
                    using Configuration = AZStd::remove_cvref_t<decltype(bodyConfiguration)>;
                    if constexpr (AZStd::is_same_v<Configuration, SceneRigidBodyConfiguration>)
                    {
                        if (bodyConfiguration.m_body.m_shapeHandle)
                        {
                            return false;
                        }
                        if (retainedCookedShapes.emplace(bodyConfiguration.m_cookedShapeHandle).second)
                        {
                            JPH::RefConst<JPH::Shape> shape;
                            if (!AcquireCookedShape(bodyConfiguration.m_cookedShapeHandle, shape))
                            {
                                retainedCookedShapes.erase(bodyConfiguration.m_cookedShapeHandle);
                                return false;
                            }
                            cookedShapeHandles.push_back(bodyConfiguration.m_cookedShapeHandle);
                        }

                        const GroupFilterHandle filterHandle = bodyConfiguration.m_body.m_collisionGroup.m_filterHandle;
                        if (filterHandle && retainedGroupFilters.emplace(filterHandle).second)
                        {
                            JPH::CollisionGroup collisionGroup;
                            if (!AcquireCollisionGroup(bodyConfiguration.m_body.m_collisionGroup, collisionGroup))
                            {
                                retainedGroupFilters.erase(filterHandle);
                                return false;
                            }
                            groupFilterHandles.push_back(filterHandle);
                        }
                        return true;
                    }
                    else
                    {
                        const SoftBodyDefinitionHandle definitionHandle = bodyConfiguration.m_definitionHandle;
                        if (retainedSoftBodyDefinitions.emplace(definitionHandle).second)
                        {
                            JPH::RefConst<JPH::SoftBodySharedSettings> settings;
                            if (!AcquireSoftBodyDefinition(definitionHandle, settings))
                            {
                                retainedSoftBodyDefinitions.erase(definitionHandle);
                                return false;
                            }
                            softBodyDefinitionHandles.push_back(definitionHandle);
                        }

                        const GroupFilterHandle filterHandle = bodyConfiguration.m_collisionGroup.m_filterHandle;
                        if (filterHandle && retainedGroupFilters.emplace(filterHandle).second)
                        {
                            JPH::CollisionGroup collisionGroup;
                            if (!AcquireCollisionGroup(bodyConfiguration.m_collisionGroup, collisionGroup))
                            {
                                retainedGroupFilters.erase(filterHandle);
                                return false;
                            }
                            groupFilterHandles.push_back(filterHandle);
                        }
                        return true;
                    }
                },
                body);
            if (!acquired)
            {
                releaseDependencies();
                return {};
            }
        }

        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(configuration.m_bodies.size());
        for (AZ::u32 constraintIndex = 0; constraintIndex < configuration.m_constraints.size(); ++constraintIndex)
        {
            const SceneConstraintConfiguration& constraint = configuration.m_constraints[constraintIndex];
            if (constraint.m_constraint.m_firstBodyHandle
                || constraint.m_constraint.m_secondBodyHandle
                || constraint.m_firstBodyIndex >= bodyCount
                || constraint.m_secondBodyIndex >= bodyCount
                || constraint.m_firstBodyIndex == constraint.m_secondBodyIndex
                || !ValidateConstraintDependencies(constraint, configuration.m_constraints, constraintIndex))
            {
                releaseDependencies();
                return {};
            }

            const auto* path = AZStd::get_if<PathConstraintConfiguration>(&constraint.m_constraint.m_geometry);
            if (path && retainedPaths.emplace(path->m_pathHandle).second)
            {
                JPH::RefConst<JPH::PathConstraintPath> nativePath;
                if (!AcquirePath(path->m_pathHandle, nativePath))
                {
                    retainedPaths.erase(path->m_pathHandle);
                    releaseDependencies();
                    return {};
                }
                pathHandles.push_back(path->m_pathHandle);
            }
        }

        AZStd::shared_ptr<const SceneConfiguration> storedConfiguration =
            AZStd::make_shared<SceneConfiguration>(configuration);
        AZStd::unique_lock lock(m_sceneDefinitionMutex);
        AZ::u32 definitionIndex = 0;
        if (!m_freeSceneDefinitionSlots.empty())
        {
            definitionIndex = m_freeSceneDefinitionSlots.back();
            m_freeSceneDefinitionSlots.pop_back();
        }
        else
        {
            if (m_sceneDefinitionSlots.size() >= Internal::HandlePayloadMask)
            {
                lock.unlock();
                releaseDependencies();
                return {};
            }
            definitionIndex = aznumeric_cast<AZ::u32>(m_sceneDefinitionSlots.size());
            m_sceneDefinitionSlots.emplace_back();
        }

        SceneDefinitionSlot& slot = m_sceneDefinitionSlots[definitionIndex];
        slot.m_configuration = AZStd::move(storedConfiguration);
        slot.m_cookedShapeHandles = AZStd::move(cookedShapeHandles);
        slot.m_groupFilterHandles = AZStd::move(groupFilterHandles);
        slot.m_pathHandles = AZStd::move(pathHandles);
        slot.m_softBodyDefinitionHandles = AZStd::move(softBodyDefinitionHandles);
        slot.m_instanceCount = 0;
        return Internal::MakeResourceHandle<SceneDefinitionHandle>(definitionIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroySceneDefinition(
        const SceneDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard resourceLock(m_sceneResourceMutex);
        AZStd::vector<CookedShapeHandle> cookedShapeHandles;
        AZStd::vector<GroupFilterHandle> groupFilterHandles;
        AZStd::vector<PathHandle> pathHandles;
        AZStd::vector<SoftBodyDefinitionHandle> softBodyDefinitionHandles;
        AZStd::vector<MaterialHandle> ownedMaterialHandles;
        AZStd::vector<CookedShapeHandle> ownedCookedShapeHandles;
        AZStd::vector<GroupFilterHandle> ownedGroupFilterHandles;
        AZStd::vector<PathHandle> ownedPathHandles;
        AZStd::vector<SoftBodyDefinitionHandle> ownedSoftBodyDefinitionHandles;
        {
            AZStd::lock_guard lock(m_sceneDefinitionMutex);
            Internal::ResourceHandleParts parts;
            if (!Internal::DecodeResourceHandle(definitionHandle, parts)
                || parts.m_index >= m_sceneDefinitionSlots.size())
            {
                return false;
            }

            SceneDefinitionSlot& slot = m_sceneDefinitionSlots[parts.m_index];
            if (!slot.m_configuration
                || slot.m_generation != parts.m_generation
                || slot.m_instanceCount > 0)
            {
                return false;
            }

            slot.m_configuration.reset();
            cookedShapeHandles = AZStd::move(slot.m_cookedShapeHandles);
            groupFilterHandles = AZStd::move(slot.m_groupFilterHandles);
            pathHandles = AZStd::move(slot.m_pathHandles);
            softBodyDefinitionHandles = AZStd::move(slot.m_softBodyDefinitionHandles);
            ownedMaterialHandles = AZStd::move(slot.m_ownedMaterialHandles);
            ownedCookedShapeHandles = AZStd::move(slot.m_ownedCookedShapeHandles);
            ownedGroupFilterHandles = AZStd::move(slot.m_ownedGroupFilterHandles);
            ownedPathHandles = AZStd::move(slot.m_ownedPathHandles);
            ownedSoftBodyDefinitionHandles = AZStd::move(slot.m_ownedSoftBodyDefinitionHandles);
            if (Internal::AdvanceGeneration(slot.m_generation))
            {
                m_freeSceneDefinitionSlots.push_back(parts.m_index);
            }
        }

        for (const PathHandle pathHandle : pathHandles)
        {
            ReleasePath(pathHandle);
        }
        for (const SoftBodyDefinitionHandle softBodyDefinitionHandle : softBodyDefinitionHandles)
        {
            ReleaseSoftBodyDefinition(softBodyDefinitionHandle);
        }
        for (const GroupFilterHandle groupFilterHandle : groupFilterHandles)
        {
            ReleaseGroupFilter(groupFilterHandle);
        }
        for (const CookedShapeHandle cookedShapeHandle : cookedShapeHandles)
        {
            ReleaseCookedShape(cookedShapeHandle);
        }

        for (const SoftBodyDefinitionHandle definition : ownedSoftBodyDefinitionHandles)
        {
            [[maybe_unused]] const bool destroyed = DestroySoftBodyDefinition(definition);
            AZ_Assert(destroyed, "Owned soft body definition destruction failed.");
        }
        for (auto shape = ownedCookedShapeHandles.rbegin(); shape != ownedCookedShapeHandles.rend(); ++shape)
        {
            [[maybe_unused]] const bool destroyed = DestroyCookedShape(*shape);
            AZ_Assert(destroyed, "Owned cooked shape destruction failed.");
        }
        for (const PathHandle path : ownedPathHandles)
        {
            [[maybe_unused]] const bool destroyed = DestroyPath(path);
            AZ_Assert(destroyed, "Owned path destruction failed.");
        }
        for (const GroupFilterHandle filter : ownedGroupFilterHandles)
        {
            [[maybe_unused]] const bool destroyed = DestroyGroupFilter(filter);
            AZ_Assert(destroyed, "Owned group filter destruction failed.");
        }
        for (const MaterialHandle material : ownedMaterialHandles)
        {
            [[maybe_unused]] const bool destroyed = DestroyMaterial(material);
            AZ_Assert(destroyed, "Owned material destruction failed.");
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SceneDefinitionHandle definitionHandle) const
    {
        AZStd::shared_lock lock(m_sceneDefinitionMutex);
        return FindSceneDefinitionUnlocked(definitionHandle);
    }

    bool RuntimeImplementation::GetSceneDefinitionState(
        const SceneDefinitionHandle definitionHandle,
        SceneDefinitionState& state) const
    {
        AZStd::shared_lock lock(m_sceneDefinitionMutex);
        const SceneDefinitionSlot* slot = FindSceneDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return false;
        }

        AZ::u32 rigidBodyCount = 0;
        AZ::u32 softBodyCount = 0;
        for (const SceneBodyConfiguration& body : slot->m_configuration->m_bodies)
        {
            if (AZStd::holds_alternative<SceneRigidBodyConfiguration>(body))
            {
                ++rigidBodyCount;
            }
            else
            {
                ++softBodyCount;
            }
        }
        state = {
            .m_name = slot->m_configuration->m_name,
            .m_bodyCount = aznumeric_cast<AZ::u32>(slot->m_configuration->m_bodies.size()),
            .m_rigidBodyCount = rigidBodyCount,
            .m_softBodyCount = softBodyCount,
            .m_constraintCount = aznumeric_cast<AZ::u32>(slot->m_configuration->m_constraints.size()),
            .m_instanceCount = slot->m_instanceCount,
        };
        return true;
    }

    SceneInstanceHandle RuntimeImplementation::InstantiateScene(
        const WorldHandle worldHandle,
        const SceneDefinitionHandle definitionHandle)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::InstantiateScene");
        AZStd::lock_guard resourceLock(m_sceneResourceMutex);
        AZStd::shared_lock worldLock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        AZStd::shared_ptr<const SceneConfiguration> configuration;
        if (!AcquireSceneDefinition(definitionHandle, configuration))
        {
            return {};
        }

        const SceneInstanceHandle instanceHandle = world->InstantiateScene(definitionHandle, *configuration);
        if (!instanceHandle)
        {
            ReleaseSceneDefinition(definitionHandle);
        }
        return instanceHandle;
    }

    bool RuntimeImplementation::DestroySceneInstance(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle)
    {
        AZStd::lock_guard resourceLock(m_sceneResourceMutex);
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroySceneInstance(instanceHandle);
    }

    bool RuntimeImplementation::DestroySceneResources(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle,
        const SceneDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard resourceLock(m_sceneResourceMutex);
        SceneDefinitionState definitionState;
        if (!GetSceneDefinitionState(definitionHandle, definitionState)
            || definitionState.m_instanceCount != 1
            || !DestroySceneInstance(worldHandle, instanceHandle))
        {
            return false;
        }

        [[maybe_unused]] const bool definitionDestroyed = DestroySceneDefinition(definitionHandle);
        AZ_Assert(definitionDestroyed, "A preflighted component scene definition must be destroyable.");
        return definitionDestroyed;
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(instanceHandle);
    }

    bool RuntimeImplementation::GetSceneInstanceState(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle,
        SceneInstanceState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetSceneInstanceState(instanceHandle, state);
    }

    QueryResult RuntimeImplementation::GetSceneBodies(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle,
        const AZStd::span<BodyHandle> bodyHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }
        return world->GetSceneBodies(instanceHandle, bodyHandles);
    }

    QueryResult RuntimeImplementation::GetSceneConstraints(
        const WorldHandle worldHandle,
        const SceneInstanceHandle instanceHandle,
        const AZStd::span<ConstraintHandle> constraintHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }
        return world->GetSceneConstraints(instanceHandle, constraintHandles);
    }

    bool RuntimeImplementation::AcquireSceneDefinition(
        const SceneDefinitionHandle definitionHandle,
        AZStd::shared_ptr<const SceneConfiguration>& configuration)
    {
        AZStd::lock_guard lock(m_sceneDefinitionMutex);
        SceneDefinitionSlot* slot = FindSceneDefinitionUnlocked(definitionHandle);
        if (!slot || slot->m_instanceCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        ++slot->m_instanceCount;
        configuration = slot->m_configuration;
        return true;
    }

    void RuntimeImplementation::ReleaseSceneDefinition(
        const SceneDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_sceneDefinitionMutex);
        SceneDefinitionSlot* slot = FindSceneDefinitionUnlocked(definitionHandle);
        AZ_Assert(slot && slot->m_instanceCount > 0, "Scene definition ownership is inconsistent.");
        if (slot && slot->m_instanceCount > 0)
        {
            --slot->m_instanceCount;
        }
    }

    RuntimeImplementation::SceneDefinitionSlot* RuntimeImplementation::FindSceneDefinitionUnlocked(
        const SceneDefinitionHandle definitionHandle)
    {
        return const_cast<SceneDefinitionSlot*>(
            static_cast<const RuntimeImplementation&>(*this).FindSceneDefinitionUnlocked(definitionHandle));
    }

    const RuntimeImplementation::SceneDefinitionSlot* RuntimeImplementation::FindSceneDefinitionUnlocked(
        const SceneDefinitionHandle definitionHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(definitionHandle, parts)
            || parts.m_index >= m_sceneDefinitionSlots.size())
        {
            return nullptr;
        }

        const SceneDefinitionSlot& slot = m_sceneDefinitionSlots[parts.m_index];
        if (!slot.m_configuration || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    SceneInstanceHandle World::InstantiateScene(
        const SceneDefinitionHandle definitionHandle,
        const SceneConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        AZ::u32 instanceIndex = 0;
        const SceneInstanceHandle instanceHandle = ReserveWorldMemberSlot<SceneInstanceHandle>(
            m_sceneInstanceSlots,
            m_freeSceneInstanceSlots,
            instanceIndex);
        if (!instanceHandle)
        {
            return {};
        }

        SceneInstanceSlot& slot = m_sceneInstanceSlots[instanceIndex];
        AZStd::vector<ShapeHandle> shapeHandles;
        AZStd::vector<BodyHandle> bodyHandles;
        AZStd::vector<ConstraintHandle> constraintHandles;
        AZStd::unordered_map<CookedShapeHandle, ShapeHandle> shapesByCookedHandle;
        shapeHandles.reserve(configuration.m_bodies.size());
        bodyHandles.reserve(configuration.m_bodies.size());
        constraintHandles.reserve(configuration.m_constraints.size());
        shapesByCookedHandle.reserve(configuration.m_bodies.size());
        AZ::u32 rigidBodyCount = 0;
        AZ::u32 softBodyCount = 0;

        const auto rollback = [&]()
        {
            for (auto iterator = constraintHandles.rbegin(); iterator != constraintHandles.rend(); ++iterator)
            {
                [[maybe_unused]] const bool destroyed = DestroyConstraint(*iterator);
                AZ_Assert(destroyed, "Scene constraint rollback failed.");
            }
            for (auto iterator = bodyHandles.rbegin(); iterator != bodyHandles.rend(); ++iterator)
            {
                [[maybe_unused]] const bool destroyed = DestroyBody(*iterator);
                AZ_Assert(destroyed, "Scene body rollback failed.");
            }
            for (auto iterator = shapeHandles.rbegin(); iterator != shapeHandles.rend(); ++iterator)
            {
                [[maybe_unused]] const bool destroyed = DestroyShape(*iterator);
                AZ_Assert(destroyed, "Scene shape rollback failed.");
            }
            m_freeSceneInstanceSlots.push_back(instanceIndex);
        };

        for (const SceneBodyConfiguration& body : configuration.m_bodies)
        {
            BodyHandle bodyHandle;
            AZStd::visit(
                [&](const auto& bodyConfiguration)
                {
                    using Configuration = AZStd::remove_cvref_t<decltype(bodyConfiguration)>;
                    if constexpr (AZStd::is_same_v<Configuration, SceneRigidBodyConfiguration>)
                    {
                        ShapeHandle shapeHandle;
                        const auto shapeIterator = shapesByCookedHandle.find(bodyConfiguration.m_cookedShapeHandle);
                        if (shapeIterator != shapesByCookedHandle.end())
                        {
                            shapeHandle = shapeIterator->second;
                        }
                        else
                        {
                            shapeHandle = CreateShape(bodyConfiguration.m_cookedShapeHandle);
                            if (!shapeHandle)
                            {
                                return;
                            }
                            shapesByCookedHandle.emplace(bodyConfiguration.m_cookedShapeHandle, shapeHandle);
                            shapeHandles.push_back(shapeHandle);
                        }

                        BodyConfiguration resolved = bodyConfiguration.m_body;
                        resolved.m_shapeHandle = shapeHandle;
                        bodyHandle = CreateBody(resolved);
                        if (bodyHandle)
                        {
                            ++rigidBodyCount;
                        }
                    }
                    else
                    {
                        bodyHandle = CreateSoftBody(bodyConfiguration);
                        if (bodyHandle)
                        {
                            ++softBodyCount;
                        }
                    }
                },
                body);
            if (!bodyHandle)
            {
                rollback();
                return {};
            }
            bodyHandles.push_back(bodyHandle);
        }

        for (const SceneConstraintConfiguration& sceneConstraint : configuration.m_constraints)
        {
            ConstraintConfiguration resolved = sceneConstraint.m_constraint;
            resolved.m_firstBodyHandle = bodyHandles[sceneConstraint.m_firstBodyIndex];
            resolved.m_secondBodyHandle = bodyHandles[sceneConstraint.m_secondBodyIndex];
            ResolveConstraintDependencies(resolved, sceneConstraint, constraintHandles);
            const ConstraintHandle constraintHandle = CreateConstraint(resolved);
            if (!constraintHandle)
            {
                rollback();
                return {};
            }
            constraintHandles.push_back(constraintHandle);
        }

        for (const ShapeHandle shapeHandle : shapeHandles)
        {
            FindShape(shapeHandle)->m_sceneInstanceHandle = instanceHandle;
        }
        for (const BodyHandle bodyHandle : bodyHandles)
        {
            FindBody(bodyHandle)->m_sceneInstanceHandle = instanceHandle;
        }
        for (const ConstraintHandle constraintHandle : constraintHandles)
        {
            FindConstraint(constraintHandle)->m_sceneInstanceHandle = instanceHandle;
        }
        slot.m_shapeHandles = AZStd::move(shapeHandles);
        slot.m_bodyHandles = AZStd::move(bodyHandles);
        slot.m_constraintHandles = AZStd::move(constraintHandles);
        slot.m_definitionHandle = definitionHandle;
        slot.m_rigidBodyCount = rigidBodyCount;
        slot.m_softBodyCount = softBodyCount;
        return instanceHandle;
    }

    bool World::DestroySceneInstance(
        const SceneInstanceHandle instanceHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        SceneInstanceSlot* slot = FindSceneInstance(instanceHandle);
        if (!slot)
        {
            return false;
        }

        AZStd::unordered_map<BodyHandle, AZ::u32> internalBodyConstraints;
        AZStd::unordered_map<ConstraintHandle, AZ::u32> internalConstraintParents;
        internalBodyConstraints.reserve(slot->m_bodyHandles.size());
        internalConstraintParents.reserve(slot->m_constraintHandles.size());
        for (const ConstraintHandle constraintHandle : slot->m_constraintHandles)
        {
            const ConstraintSlot* constraintSlot = FindConstraint(constraintHandle);
            if (!constraintSlot || constraintSlot->m_sceneInstanceHandle != instanceHandle)
            {
                return false;
            }
            ++internalBodyConstraints[constraintSlot->m_firstBodyHandle];
            ++internalBodyConstraints[constraintSlot->m_secondBodyHandle];
            for (const ConstraintHandle dependencyHandle : constraintSlot->m_dependencyHandles)
            {
                if (dependencyHandle)
                {
                    ++internalConstraintParents[dependencyHandle];
                }
            }
        }
        for (const BodyHandle bodyHandle : slot->m_bodyHandles)
        {
            const BodySlot* bodySlot = FindBody(bodyHandle);
            if (!bodySlot
                || bodySlot->m_sceneInstanceHandle != instanceHandle
                || bodySlot->m_characterHandle
                || bodySlot->m_vehicleHandle
                || bodySlot->m_ragdollHandle
                || bodySlot->m_constraintCount != internalBodyConstraints[bodyHandle])
            {
                return false;
            }
        }
        for (const ConstraintHandle constraintHandle : slot->m_constraintHandles)
        {
            const ConstraintSlot* constraintSlot = FindConstraint(constraintHandle);
            if (constraintSlot->m_parentCount != internalConstraintParents[constraintHandle])
            {
                return false;
            }
        }

        for (auto iterator = slot->m_constraintHandles.rbegin(); iterator != slot->m_constraintHandles.rend(); ++iterator)
        {
            FindConstraint(*iterator)->m_sceneInstanceHandle = {};
            [[maybe_unused]] const bool destroyed = DestroyConstraint(*iterator);
            AZ_Assert(destroyed, "Preflighted scene constraint destruction failed.");
        }
        for (auto iterator = slot->m_bodyHandles.rbegin(); iterator != slot->m_bodyHandles.rend(); ++iterator)
        {
            FindBody(*iterator)->m_sceneInstanceHandle = {};
            [[maybe_unused]] const bool destroyed = DestroyBody(*iterator);
            AZ_Assert(destroyed, "Preflighted scene body destruction failed.");
        }
        for (auto iterator = slot->m_shapeHandles.rbegin(); iterator != slot->m_shapeHandles.rend(); ++iterator)
        {
            FindShape(*iterator)->m_sceneInstanceHandle = {};
            [[maybe_unused]] const bool destroyed = DestroyShape(*iterator);
            AZ_Assert(destroyed, "Preflighted scene shape destruction failed.");
        }

        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(instanceHandle, parts))
        {
            return false;
        }
        const SceneDefinitionHandle definitionHandle = slot->m_definitionHandle;
        slot->m_shapeHandles.clear();
        slot->m_bodyHandles.clear();
        slot->m_constraintHandles.clear();
        slot->m_definitionHandle = {};
        slot->m_rigidBodyCount = 0;
        slot->m_softBodyCount = 0;
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeSceneInstanceSlots.push_back(parts.m_index);
        }
        m_system.ReleaseSceneDefinition(definitionHandle);
        return true;
    }

    bool World::IsValid(
        const SceneInstanceHandle instanceHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        return FindSceneInstance(instanceHandle);
    }

    bool World::GetSceneInstanceState(
        const SceneInstanceHandle instanceHandle,
        SceneInstanceState& state) const
    {
        AZStd::lock_guard lock(m_mutex);
        const SceneInstanceSlot* slot = FindSceneInstance(instanceHandle);
        if (!slot)
        {
            return false;
        }
        state = {
            .m_definitionHandle = slot->m_definitionHandle,
            .m_bodyCount = aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()),
            .m_rigidBodyCount = slot->m_rigidBodyCount,
            .m_softBodyCount = slot->m_softBodyCount,
            .m_constraintCount = aznumeric_cast<AZ::u32>(slot->m_constraintHandles.size()),
        };
        return true;
    }

    QueryResult World::GetSceneBodies(
        const SceneInstanceHandle instanceHandle,
        const AZStd::span<BodyHandle> bodyHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        const SceneInstanceSlot* slot = FindSceneInstance(instanceHandle);
        if (!slot)
        {
            return {};
        }
        const size_t copyCount = AZStd::min(bodyHandles.size(), slot->m_bodyHandles.size());
        for (size_t bodyIndex = 0; bodyIndex < copyCount; ++bodyIndex)
        {
            bodyHandles[bodyIndex] = slot->m_bodyHandles[bodyIndex];
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()),
        };
    }

    QueryResult World::GetSceneConstraints(
        const SceneInstanceHandle instanceHandle,
        const AZStd::span<ConstraintHandle> constraintHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        const SceneInstanceSlot* slot = FindSceneInstance(instanceHandle);
        if (!slot)
        {
            return {};
        }
        const size_t copyCount = AZStd::min(constraintHandles.size(), slot->m_constraintHandles.size());
        for (size_t constraintIndex = 0; constraintIndex < copyCount; ++constraintIndex)
        {
            constraintHandles[constraintIndex] = slot->m_constraintHandles[constraintIndex];
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot->m_constraintHandles.size()),
        };
    }

    World::SceneInstanceSlot* World::FindSceneInstance(
        const SceneInstanceHandle instanceHandle)
    {
        return const_cast<SceneInstanceSlot*>(
            static_cast<const World&>(*this).FindSceneInstance(instanceHandle));
    }

    const World::SceneInstanceSlot* World::FindSceneInstance(
        const SceneInstanceHandle instanceHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(instanceHandle, parts)
            || parts.m_worldIndex != m_worldIndex
            || parts.m_index >= m_sceneInstanceSlots.size())
        {
            return nullptr;
        }

        const SceneInstanceSlot& slot = m_sceneInstanceSlots[parts.m_index];
        if (!slot.m_definitionHandle || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }
} // namespace Jolt
