/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/SystemInternal.h>

#include <Box3D/CharacterBus.h>
#include <Box3D/FloatEnvironment.h>
#include <Box3D/JointBus.h>
#include <Box3D/RigidBodyBus.h>
#include <Box3D/World.h>
#include <Box3D/WorldBus.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>

#include <cmath>

namespace Box3D
{
    namespace
    {
        constexpr AZ::u32 MaximumWorkerCount = 32;

        bool IsValidMaterialConfiguration(
            const MaterialConfiguration& configuration)
        {
            const auto isNonNegative = [](const float value)
            {
                return AZ::IsFiniteFloat(value) && value >= 0.0f;
            };
            const bool validPreset = configuration.m_debugMaterialPreset >= DebugMaterialPreset::Default
                && configuration.m_debugMaterialPreset <= DebugMaterialPreset::Metallic;
            return configuration.m_tangentVelocity.IsFinite()
                && configuration.m_debugColor.IsFinite()
                && isNonNegative(configuration.m_friction)
                && isNonNegative(configuration.m_restitution)
                && isNonNegative(configuration.m_rollingResistance)
                && isNonNegative(configuration.m_density)
                && isNonNegative(configuration.m_explosionScale)
                && validPreset;
        }
    } // namespace

    System::System(
        const SystemConfiguration& configuration,
        AZ::JobContext* defaultJobContext)
    {
        const DeterministicFloatScope floatScope;
        SetMaterialCallbacks(this);
        UpdateConfiguration(configuration);
        WorldConfiguration defaultWorldConfiguration;
        defaultWorldConfiguration.m_name = AZ_NAME_LITERAL("Default");
        defaultWorldConfiguration.m_jobContext = defaultJobContext;
        m_defaultWorldHandle = CreateWorld(defaultWorldConfiguration);
    }

    System::~System()
    {
        const DeterministicFloatScope floatScope;
        for (WorldSlot& slot : m_worldSlots)
        {
            slot.m_world.reset();
        }
        SetMaterialCallbacks(nullptr);
    }

    void System::UpdateConfiguration(
        const SystemConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::UpdateConfiguration");
        const DeterministicFloatScope floatScope;
        if (configuration != m_configuration)
        {
            for (const WorldSlot& slot : m_worldSlots)
            {
                if (slot.m_world)
                {
                    AZStd::lock_guard lock(slot.m_world->m_mutex);
                    if (slot.m_world->m_recording)
                    {
                        AZ_Warning("Box3D", false, "System configuration cannot be changed while a world is recording.");
                        return;
                    }
                }
            }
        }
        m_configuration = configuration;
        m_configuration.m_subStepCount = AZStd::max(m_configuration.m_subStepCount, AZ::u32{1});
        m_configuration.m_workerCount = AZStd::clamp(m_configuration.m_workerCount, AZ::u32{1}, MaximumWorkerCount);
        const auto nonNegativeOr = [](const float value, const float fallback)
        {
            if (AZ::IsFiniteFloat(value) && value >= 0.0f)
            {
                return value;
            }

            return fallback;
        };
        m_configuration.m_contactHertz = nonNegativeOr(m_configuration.m_contactHertz, 30.0f);
        m_configuration.m_contactDampingRatio = nonNegativeOr(m_configuration.m_contactDampingRatio, 10.0f);
        m_configuration.m_contactSpeed = nonNegativeOr(m_configuration.m_contactSpeed, 3.0f);
        m_configuration.m_contactRecycleDistance = nonNegativeOr(m_configuration.m_contactRecycleDistance, 0.05f);
        m_configuration.m_restitutionThreshold = nonNegativeOr(m_configuration.m_restitutionThreshold, 1.0f);
        m_configuration.m_hitEventThreshold = nonNegativeOr(m_configuration.m_hitEventThreshold, 1.0f);
        if (!AZ::IsFiniteFloat(m_configuration.m_maximumLinearSpeed) || m_configuration.m_maximumLinearSpeed <= 0.0f)
        {
            m_configuration.m_maximumLinearSpeed = 400.0f;
        }
        if (!AZ::IsFiniteFloat(m_configuration.m_lengthUnitsPerMeter) || m_configuration.m_lengthUnitsPerMeter <= 0.0f)
        {
            m_configuration.m_lengthUnitsPerMeter = 1.0f;
        }
        if (!AZ::IsFiniteFloat(m_configuration.m_stallWarningThresholdSeconds) || m_configuration.m_stallWarningThresholdSeconds <= 0.0f)
        {
            m_configuration.m_stallWarningThresholdSeconds = (AZStd::numeric_limits<float>::max)();
        }
        Box3D::SetLengthUnitsPerMeter(m_configuration.m_lengthUnitsPerMeter);
        Box3D::SetStallWarningThreshold(m_configuration.m_stallWarningThresholdSeconds);

        constexpr AZ::u32 maximumCapacity = static_cast<AZ::u32>((AZStd::numeric_limits<int>::max)());
        m_configuration.m_staticShapeCapacity = AZStd::min(m_configuration.m_staticShapeCapacity, maximumCapacity);
        m_configuration.m_dynamicShapeCapacity =
            AZStd::min(m_configuration.m_dynamicShapeCapacity, maximumCapacity - m_configuration.m_staticShapeCapacity);
        m_configuration.m_staticBodyCapacity = AZStd::min(m_configuration.m_staticBodyCapacity, maximumCapacity);
        m_configuration.m_dynamicBodyCapacity =
            AZStd::min(m_configuration.m_dynamicBodyCapacity, maximumCapacity - m_configuration.m_staticBodyCapacity);
        m_configuration.m_contactCapacity = AZStd::min(m_configuration.m_contactCapacity, maximumCapacity);

        UpdateCompatibilityFingerprint();
        for (WorldSlot& slot : m_worldSlots)
        {
            if (slot.m_world)
            {
                slot.m_world->Reconfigure(m_configuration);
            }
        }
    }

    const SystemConfiguration& System::GetConfiguration() const
    {
        return m_configuration;
    }

    WorldHandle System::CreateWorld(
        const WorldConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CreateWorld");
        if (configuration.m_name.IsEmpty() || FindWorld(configuration.m_name))
        {
            AZ_Error("Box3D", false, "A world requires a unique non-empty name.");
            return {};
        }
        if (!configuration.m_gravity.IsFinite()
            || !AZ::IsFiniteFloat(configuration.m_fixedTimeStep)
            || configuration.m_fixedTimeStep <= 0.0f
            || configuration.m_maximumCatchUpSteps == 0)
        {
            AZ_Error("Box3D", false, "A world requires finite gravity, a positive fixed time step, and at least one catch-up step.");
            return {};
        }

        AZ::u32 worldIndex = 0;
        if (!m_freeWorldSlots.empty())
        {
            worldIndex = m_freeWorldSlots.back();
            m_freeWorldSlots.pop_back();
        }
        else
        {
            if (m_worldSlots.size() >= Internal::MaximumWorldCount)
            {
                AZ_Error("Box3D", false, "The maximum number of Box3D worlds has been reached.");
                return {};
            }
            worldIndex = aznumeric_cast<AZ::u32>(m_worldSlots.size());
            m_worldSlots.emplace_back();
        }

        WorldSlot& slot = m_worldSlots[worldIndex];
        const WorldHandle worldHandle = Internal::MakeWorldHandle(worldIndex, slot.m_generation);
        slot.m_world = AZStd::make_unique<World>(
            *this,
            worldIndex,
            worldHandle,
            configuration,
            m_configuration,
            m_bodyGenerations[worldIndex],
            m_shapeGenerations[worldIndex],
            m_jointGenerations[worldIndex],
            m_characterGenerations[worldIndex]);
        if (!slot.m_world->IsValid())
        {
            slot.m_world.reset();
            m_freeWorldSlots.push_back(worldIndex);
            return {};
        }
        if (!m_defaultWorldHandle)
        {
            m_defaultWorldHandle = worldHandle;
            m_defaultWorldInstance = slot.m_world.get();
        }
        return worldHandle;
    }

    bool System::DestroyWorld(
        const WorldHandle worldHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyWorld");
        Internal::WorldHandleParts parts;
        if (!Internal::DecodeWorldHandle(worldHandle, parts) || parts.m_index >= m_worldSlots.size())
        {
            return false;
        }
        WorldSlot& slot = m_worldSlots[parts.m_index];
        if (!slot.m_world || slot.m_generation != parts.m_generation)
        {
            return false;
        }

        slot.m_world.reset();
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeWorldSlots.push_back(parts.m_index);
        }
        if (m_defaultWorldHandle == worldHandle)
        {
            m_defaultWorldHandle = {};
            m_defaultWorldInstance = nullptr;
            for (const WorldSlot& candidate : m_worldSlots)
            {
                if (candidate.m_world)
                {
                    m_defaultWorldHandle = candidate.m_world->GetHandle();
                    m_defaultWorldInstance = candidate.m_world.get();
                    break;
                }
            }
        }
        return true;
    }

    WorldHandle System::GetDefaultWorldHandle() const
    {
        return m_defaultWorldHandle;
    }

    const IWorldQueries* System::GetWorldQueries(
        const WorldHandle worldHandle) const
    {
        return FindWorldInstance(worldHandle);
    }

    WorldHandle System::FindWorld(
        const AZ::Name name) const
    {
        for (const WorldSlot& slot : m_worldSlots)
        {
            if (slot.m_world)
            {
                const AZ::Name& worldName = slot.m_world->GetName();
                const bool deferredMatch =
                    (worldName.GetHash() == 0 || name.GetHash() == 0) && worldName.GetStringView() == name.GetStringView();
                if (deferredMatch || (worldName.GetHash() != 0 && worldName == name))
                {
                    return slot.m_world->GetHandle();
                }
            }
        }
        return {};
    }

    bool System::GetWorldConfiguration(
        const WorldHandle worldHandle,
        WorldConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (!world)
        {
            return false;
        }
        configuration = world->GetConfiguration();
        return true;
    }

    AZ::Aabb System::GetWorldAabb(
        const WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetAabb();
        }

        return AZ::Aabb::CreateNull();
    }

    bool System::IsValid(
        const WorldHandle worldHandle) const
    {
        return FindWorldInstance(worldHandle);
    }

    bool System::SetWorldEnabled(
        const WorldHandle worldHandle,
        const bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetEnabled(enabled);
    }

    bool System::IsWorldEnabled(
        const WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->IsEnabled();
    }

    bool System::SetWorldGravity(
        const WorldHandle worldHandle,
        const AZ::Vector3& gravity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetGravity(gravity);
    }

    bool System::GetWorldGravity(
        const WorldHandle worldHandle,
        AZ::Vector3& gravity) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetGravity(gravity);
    }

    bool System::StepWorld(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        World* world = FindWorldInstance(worldHandle);
        if (!world || !world->Step(fixedTimeStep, m_configuration.m_subStepCount))
        {
            return false;
        }
        DispatchStepEvents(*world);
        return true;
    }

    void System::StepAutoSimulatedWorlds(
        const float deltaTime)
    {
        for (WorldSlot& slot : m_worldSlots)
        {
            if (slot.m_world)
            {
                const SimulationTick previousTick = slot.m_world->GetLastCompletedTick();
                slot.m_world->StepAutomatically(deltaTime, m_configuration.m_subStepCount);
                if (slot.m_world->GetLastCompletedTick() != previousTick)
                {
                    DispatchStepEvents(*slot.m_world);
                }
            }
        }
    }

    void System::DispatchStepEvents(
        const World& world) const
    {
        const WorldHandle worldHandle = world.GetHandle();
        for (const World::EntityBodyMove& notification : world.m_entityBodyMoves)
        {
            RigidBodyNotificationBus::Event(notification.m_entityId, &RigidBodyNotifications::OnBodyMoved, notification.m_event);
        }
        for (const World::EntityJointThreshold& notification : world.m_entityJointThresholds)
        {
            JointNotificationBus::Event(notification.m_entityId, &JointNotifications::OnThresholdExceeded, notification.m_event);
        }
        for (const World::EntityCharacterMove& notification : world.m_entityCharacterMoves)
        {
            CharacterNotificationBus::Event(notification.m_entityId, &CharacterNotifications::OnCharacterMoved, notification.m_state);
        }
        if (!WorldNotificationBus::HasHandlers(worldHandle))
        {
            return;
        }

        const StepEvents events = world.GetStepEvents();
        for (const BodyMoveEvent& event : events.m_bodyMoves)
        {
            WorldNotificationBus::Event(worldHandle, &WorldNotifications::OnBodyMoved, event);
        }
        for (const SensorEvent& event : events.m_sensorEvents)
        {
            WorldNotificationBus::Event(worldHandle, &WorldNotifications::OnSensor, event);
        }
        for (const ContactEvent& event : events.m_contactEvents)
        {
            WorldNotificationBus::Event(worldHandle, &WorldNotifications::OnContact, event);
        }
        for (const ContactHitEvent& event : events.m_contactHits)
        {
            WorldNotificationBus::Event(worldHandle, &WorldNotifications::OnContactHit, event);
        }
        for (const JointThresholdEvent& event : events.m_jointThresholds)
        {
            WorldNotificationBus::Event(worldHandle, &WorldNotifications::OnJointThresholdExceeded, event);
        }
    }

    SimulationTick System::GetLastCompletedTick(
        const WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetLastCompletedTick();
        }

        return 0;
    }

    AZ::u64 System::GetStateDigest(
        const WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetStateDigest();
        }

        return 0;
    }

    AZStd::string_view System::GetCompatibilityFingerprint() const
    {
        return m_compatibilityFingerprint;
    }

    MaterialHandle System::CreateMaterial(
        const MaterialConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CreateMaterial");
        if (!IsValidMaterialConfiguration(configuration))
        {
            return {};
        }

        AZ::u32 materialIndex = 0;
        if (!m_freeMaterialSlots.empty())
        {
            materialIndex = m_freeMaterialSlots.back();
            m_freeMaterialSlots.pop_back();
        }
        else
        {
            materialIndex = aznumeric_cast<AZ::u32>(m_materialSlots.size());
            m_materialSlots.emplace_back();
            m_materialConfigurations.emplace_back();
        }

        MaterialSlot& slot = m_materialSlots[materialIndex];
        slot.m_generation = m_materialGenerations.Acquire();
        if (slot.m_generation == 0)
        {
            m_materialConfigurations[materialIndex] = {};
            m_freeMaterialSlots.push_back(materialIndex);
            return {};
        }
        m_materialConfigurations[materialIndex] = configuration;
        return Internal::MakeRegistryHandle<MaterialHandle>(materialIndex, slot.m_generation);
    }

    bool System::UpdateMaterial(
        const MaterialHandle materialHandle,
        const MaterialConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::UpdateMaterial");
        if (!IsValidMaterialConfiguration(configuration) || UsesCookedMaterial(materialHandle))
        {
            return false;
        }

        AZ::u32 materialIndex = 0;
        MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (!slot)
        {
            return false;
        }
        MaterialConfiguration& retainedConfiguration = m_materialConfigurations[materialIndex];
        const MaterialConfiguration previousConfiguration = retainedConfiguration;
        retainedConfiguration = configuration;
        for (WorldSlot& worldSlot : m_worldSlots)
        {
            if (worldSlot.m_world && !worldSlot.m_world->RefreshMaterial(materialHandle))
            {
                retainedConfiguration = previousConfiguration;
                for (WorldSlot& rollbackWorldSlot : m_worldSlots)
                {
                    if (rollbackWorldSlot.m_world)
                    {
                        [[maybe_unused]] const bool restored = rollbackWorldSlot.m_world->RefreshMaterial(materialHandle);
                    }
                }
                return false;
            }
        }
        return true;
    }

    bool System::GetMaterial(
        const MaterialHandle materialHandle,
        MaterialConfiguration& configuration) const
    {
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (!slot)
        {
            return false;
        }
        configuration = m_materialConfigurations[materialIndex];
        return true;
    }

    bool System::DestroyMaterial(
        const MaterialHandle materialHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyMaterial");
        AZ::u32 materialIndex = 0;
        AZ::u32 generation = 0;
        MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (!slot || !Internal::DecodeRegistryHandle(materialHandle, materialIndex, generation))
        {
            return false;
        }
        if (UsesCookedMaterial(materialHandle))
        {
            return false;
        }
        for (const WorldSlot& worldSlot : m_worldSlots)
        {
            if (worldSlot.m_world && worldSlot.m_world->UsesMaterial(materialHandle))
            {
                return false;
            }
        }
        m_materialConfigurations[materialIndex] = {};
        slot->m_generation = 0;
        m_freeMaterialSlots.push_back(materialIndex);
        return true;
    }

    CookedShapeHandle System::CookShape(
        const ShapeConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CookShape");
        const DeterministicFloatScope floatScope;
        if (!configuration.m_materialConfigurations.empty())
        {
            AZ_Error("Box3D", false, "Cooking requires transient material handles, not serialized material configurations.");
            return {};
        }

        AZStd::vector<SurfaceMaterial> nativeMaterials;
        nativeMaterials.reserve(configuration.m_properties.m_materials.size());
        for (MaterialHandle materialHandle : configuration.m_properties.m_materials)
        {
            if (!FindMaterialSlot(materialHandle))
            {
                return {};
            }
            nativeMaterials.push_back(ResolveMaterial(materialHandle));
        }

        NativeGeometry nativeGeometry =
            CookGeometry(configuration.m_geometry, GeometryTransform{configuration.m_properties.m_localTransform}, nativeMaterials);
        const bool validGeometry = AZStd::visit(
            [](const auto& geometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                if constexpr (AZStd::is_same_v<Geometry, AZStd::monostate>)
                {
                    return false;
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeSphereGeometry> || AZStd::is_same_v<Geometry, NativeCapsuleGeometry>)
                {
                    return true;
                }
                else
                {
                    return static_cast<bool>(geometry);
                }
            },
            nativeGeometry);
        if (!validGeometry)
        {
            return {};
        }

        AZ::u32 cookedShapeIndex = 0;
        if (!m_freeCookedShapeSlots.empty())
        {
            cookedShapeIndex = m_freeCookedShapeSlots.back();
            m_freeCookedShapeSlots.pop_back();
        }
        else
        {
            if (m_cookedShapeSlots.size() >= AZStd::numeric_limits<AZ::u32>::max())
            {
                return {};
            }
            cookedShapeIndex = aznumeric_cast<AZ::u32>(m_cookedShapeSlots.size());
            m_cookedShapeSlots.emplace_back();
            m_cookedShapeResources.emplace_back();
        }

        CookedShapeSlot& slot = m_cookedShapeSlots[cookedShapeIndex];
        slot.m_generation = m_cookedShapeGenerations.Acquire();
        if (slot.m_generation == 0)
        {
            m_cookedShapeResources[cookedShapeIndex] = CookedShapeResources{};
            m_freeCookedShapeSlots.push_back(cookedShapeIndex);
            return {};
        }
        CookedShapeResources& resources = m_cookedShapeResources[cookedShapeIndex];
        resources.m_geometry = AZStd::move(nativeGeometry);
        resources.m_materials = configuration.m_properties.m_materials;
        return Internal::MakeRegistryHandle<CookedShapeHandle>(cookedShapeIndex, slot.m_generation);
    }

    bool System::DestroyCookedShape(
        const CookedShapeHandle cookedShapeHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyCookedShape");
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        AZ::u32 generation = 0;
        CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (!slot || !Internal::DecodeRegistryHandle(cookedShapeHandle, cookedShapeIndex, generation))
        {
            return false;
        }

        *slot = CookedShapeSlot{};
        m_cookedShapeResources[cookedShapeIndex] = CookedShapeResources{};
        m_freeCookedShapeSlots.push_back(cookedShapeIndex);
        return true;
    }

    bool System::IsValid(
        const CookedShapeHandle cookedShapeHandle) const
    {
        return FindCookedShapeSlot(cookedShapeHandle);
    }

    AZ::Aabb System::GetAabb(
        const CookedShapeHandle cookedShapeHandle) const
    {
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (slot)
        {
            return Box3D::GetAabb(m_cookedShapeResources[cookedShapeIndex].m_geometry);
        }

        return AZ::Aabb::CreateNull();
    }

    bool System::Raycast(
        const CookedShapeHandle cookedShapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        const float distance,
        GeometryHit& hit) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::RaycastCookedShape");
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (!slot || !start.IsFinite() || !direction.IsFinite() || direction.IsZero() || !AZ::IsFiniteFloat(distance) || distance <= 0.0f)
        {
            return false;
        }

        GeometryCastHit nativeHit;
        if (!Box3D::CastRay(m_cookedShapeResources[cookedShapeIndex].m_geometry, start, direction.GetNormalized() * distance, nativeHit))
        {
            return false;
        }
        hit = {nativeHit.m_position,
               nativeHit.m_normal,
               nativeHit.m_fraction * distance,
               nativeHit.m_fraction,
               nativeHit.m_materialIndex,
               nativeHit.m_triangleIndex,
               nativeHit.m_childIndex};
        return true;
    }

    BodyHandle System::CreateBody(
        const WorldHandle worldHandle,
        const RigidBodyConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->CreateBody(configuration);
        }

        return {};
    }

    bool System::DestroyBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->DestroyBody(bodyHandle);
    }

    bool System::GetBodyState(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyState(bodyHandle, state);
    }

    AZ::Name System::GetBodyName(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodyName(bodyHandle);
        }

        return {};
    }

    bool System::SetBodyName(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Name name)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyName(bodyHandle, name);
    }

    bool System::GetBodyProperties(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyProperties(bodyHandle, properties);
    }

    bool System::SetBodyProperties(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyProperties(bodyHandle, properties);
    }

    bool System::SetBodyAwake(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool awake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyAwake(bodyHandle, awake);
    }

    bool System::SetBodyEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyEnabled(bodyHandle, enabled);
    }

    bool System::SetBodyHitEventsEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyHitEventsEnabled(bodyHandle, enabled);
    }

    bool System::SetBodyTransform(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Transform& transform)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetBodyTransform(bodyHandle, transform);
    }

    bool System::GetBodyLocalPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& worldPoint,
        AZ::Vector3& localPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyLocalPoint(bodyHandle, worldPoint, localPoint);
    }

    bool System::GetBodyWorldPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& localPoint,
        AZ::Vector3& worldPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyWorldPoint(bodyHandle, localPoint, worldPoint);
    }

    bool System::GetBodyLocalVector(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& worldVector,
        AZ::Vector3& localVector) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyLocalVector(bodyHandle, worldVector, localVector);
    }

    bool System::GetBodyWorldVector(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& localVector,
        AZ::Vector3& worldVector) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyWorldVector(bodyHandle, localVector, worldVector);
    }

    bool System::SetLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& velocity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetLinearVelocity(bodyHandle, velocity);
    }

    bool System::SetAngularVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& velocity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetAngularVelocity(bodyHandle, velocity);
    }

    AZ::Vector3 System::GetLinearVelocityAtLocalPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& localPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetLinearVelocityAtLocalPoint(bodyHandle, localPoint);
        }

        return AZ::Vector3::CreateZero();
    }

    AZ::Vector3 System::GetLinearVelocityAtWorldPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& worldPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetLinearVelocityAtWorldPoint(bodyHandle, worldPoint);
        }

        return AZ::Vector3::CreateZero();
    }

    bool System::SetKinematicTarget(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Transform& transform,
        const float fixedTimeStep,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetKinematicTarget(bodyHandle, transform, fixedTimeStep, wake);
    }

    bool System::ApplyLinearImpulse(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& impulse,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyLinearImpulse(bodyHandle, impulse, wake);
    }

    bool System::ApplyLinearImpulseAtWorldPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& impulse,
        const AZ::Vector3& worldPoint,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyLinearImpulseAtWorldPoint(bodyHandle, impulse, worldPoint, wake);
    }

    bool System::ApplyAngularImpulse(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& impulse,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyAngularImpulse(bodyHandle, impulse, wake);
    }

    bool System::ApplyForce(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyForce(bodyHandle, force, wake);
    }

    bool System::ApplyForceAtWorldPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const AZ::Vector3& worldPoint,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyForceAtWorldPoint(bodyHandle, force, worldPoint, wake);
    }

    bool System::ApplyTorque(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& torque,
        const bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyTorque(bodyHandle, torque, wake);
    }

    bool System::GetMassProperties(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        MassProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetMassProperties(bodyHandle, properties);
    }

    bool System::SetMassProperties(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const MassProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetMassProperties(bodyHandle, properties);
    }

    bool System::RecomputeMassFromShapes(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->RecomputeMassFromShapes(bodyHandle);
    }

    AZ::Matrix3x3 System::GetWorldInverseInertia(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetWorldInverseInertia(bodyHandle);
        }

        return AZ::Matrix3x3::CreateZero();
    }

    AZ::Vector3 System::GetWorldCenterOfMass(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetWorldCenterOfMass(bodyHandle);
        }

        return AZ::Vector3::CreateZero();
    }

    bool System::GetBodyClosestPoint(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& target,
        AZ::Vector3& position,
        float& distance) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetBodyClosestPoint(bodyHandle, target, position, distance);
    }

    AZ::Aabb System::GetBodyAabb(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodyAabb(bodyHandle);
        }

        return AZ::Aabb::CreateNull();
    }

    BufferResult System::GetBodyShapes(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZStd::span<ShapeHandle> shapeHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodyShapes(bodyHandle, shapeHandles);
        }

        return {};
    }

    BufferResult System::GetBodyJoints(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZStd::span<JointHandle> jointHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodyJoints(bodyHandle, jointHandles);
        }

        return {};
    }

    ContactSnapshotResult System::GetBodyContacts(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZStd::span<ContactSnapshot> contacts,
        const AZStd::span<ContactPoint> points) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodyContacts(bodyHandle, contacts, points);
        }

        return {};
    }

    BufferResult System::GetBodySensorOverlaps(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZStd::span<SensorOverlap> overlaps) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetBodySensorOverlaps(bodyHandle, overlaps);
        }

        return {};
    }

    bool System::RaycastBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyRaycastRequest& request,
        QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->RaycastBody(bodyHandle, request, hit);
    }

    bool System::ShapeCastBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyShapeCastRequest& request,
        QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->ShapeCastBody(bodyHandle, request, hit);
    }

    bool System::OverlapBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyOverlapRequest& request) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->OverlapBody(bodyHandle, request);
    }

    ShapeHandle System::CreateShape(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const ShapeConfiguration& configuration)
    {
        return CreateShape(worldHandle, bodyHandle, configuration, 1.0f);
    }

    ShapeHandle System::CreateShape(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const ShapeConfiguration& configuration,
        const float uniformScale)
    {
        if (!configuration.m_materialConfigurations.empty())
        {
            AZ_Error("Box3D", false, "Direct shape creation requires material handles, not serialized material configurations.");
            return {};
        }
        World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->CreateShape(bodyHandle, configuration, uniformScale);
        }

        return {};
    }

    ShapeHandle System::CreateShapeFromCooked(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const CookedShapeHandle cookedShapeHandle,
        const ShapeProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* cookedShape = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (!world
            || !cookedShape
            || !properties.m_materials.empty()
            || !properties.m_localTransform.GetTranslation().IsZero()
            || !properties.m_localTransform.GetRotation().IsIdentity()
            || !AZ::IsClose(properties.m_localTransform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance))
        {
            return {};
        }

        ShapeProperties instanceProperties = properties;
        const CookedShapeResources& resources = m_cookedShapeResources[cookedShapeIndex];
        instanceProperties.m_materials = resources.m_materials;
        return world->CreateShapeFromCooked(bodyHandle, resources.m_geometry, instanceProperties);
    }

    bool System::UpdateShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const ShapeConfiguration& configuration)
    {
        return UpdateShape(worldHandle, shapeHandle, configuration, 1.0f);
    }

    bool System::UpdateShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const ShapeConfiguration& configuration,
        const float uniformScale)
    {
        if (!configuration.m_materialConfigurations.empty())
        {
            AZ_Error("Box3D", false, "Direct shape updates require material handles, not serialized material configurations.");
            return false;
        }
        World* world = FindWorldInstance(worldHandle);
        return world && world->UpdateShape(shapeHandle, configuration, uniformScale);
    }

    bool System::DestroyShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const bool updateBodyMass)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->DestroyShape(shapeHandle, updateBodyMass);
    }

    bool System::SetShapeCollisionFilter(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const CollisionFilter& collisionFilter)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeCollisionFilter(shapeHandle, collisionFilter);
    }

    bool System::SetShapeMaterials(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<const MaterialHandle> materials)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeMaterials(shapeHandle, materials);
    }

    AZ::Aabb System::GetShapeAabb(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetShapeAabb(shapeHandle);
        }

        return AZ::Aabb::CreateNull();
    }

    bool System::GetShapeState(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        ShapeState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetShapeState(shapeHandle, state);
    }

    BufferResult System::GetShapeMaterials(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<MaterialHandle> materialHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetShapeMaterials(shapeHandle, materialHandles);
        }

        return {};
    }

    bool System::SetShapeDensity(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const float density,
        const bool updateBodyMass)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeDensity(shapeHandle, density, updateBodyMass);
    }

    bool System::SetShapeFriction(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const float friction)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeFriction(shapeHandle, friction);
    }

    bool System::SetShapeRestitution(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const float restitution)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeRestitution(shapeHandle, restitution);
    }

    bool System::SetShapeEventSubscriptions(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const bool sensorEvents,
        const bool contactEvents,
        const bool hitEvents,
        const bool preSolveEvents)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetShapeEventSubscriptions(shapeHandle, sensorEvents, contactEvents, hitEvents, preSolveEvents);
    }

    bool System::GetShapeMassProperties(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        MassProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetShapeMassProperties(shapeHandle, properties);
    }

    bool System::GetShapeClosestPoint(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& target,
        AZ::Vector3& position,
        float& distance) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetShapeClosestPoint(shapeHandle, target, position, distance);
    }

    bool System::RaycastShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        const float distance,
        QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->RaycastShape(shapeHandle, start, direction, distance, hit);
    }

    ContactSnapshotResult System::GetShapeContacts(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<ContactSnapshot> contacts,
        const AZStd::span<ContactPoint> points) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetShapeContacts(shapeHandle, contacts, points);
        }

        return {};
    }

    BufferResult System::GetShapeSensorOverlaps(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<SensorOverlap> overlaps) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetShapeSensorOverlaps(shapeHandle, overlaps);
        }

        return {};
    }

    JointHandle System::CreateJoint(
        const WorldHandle worldHandle,
        const JointConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->CreateJoint(configuration);
        }

        return {};
    }

    bool System::SetJointEntityId(
        const WorldHandle worldHandle,
        const JointHandle jointHandle,
        const AZ::EntityId entityId)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetJointEntityId(jointHandle, entityId);
    }

    bool System::UpdateJoint(
        const WorldHandle worldHandle,
        const JointHandle jointHandle,
        const JointConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->UpdateJoint(jointHandle, configuration);
    }

    bool System::DestroyJoint(
        const WorldHandle worldHandle,
        const JointHandle jointHandle,
        const bool wakeAttachedBodies)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->DestroyJoint(jointHandle, wakeAttachedBodies);
    }

    bool System::WakeJointBodies(
        const WorldHandle worldHandle,
        const JointHandle jointHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->WakeJointBodies(jointHandle);
    }

    bool System::GetJointConfiguration(
        const WorldHandle worldHandle,
        const JointHandle jointHandle,
        JointConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetJointConfiguration(jointHandle, configuration);
    }

    bool System::GetJointMeasurements(
        const WorldHandle worldHandle,
        const JointHandle jointHandle,
        JointMeasurements& measurements) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetJointMeasurements(jointHandle, measurements);
    }

    CharacterHandle System::CreateCharacter(
        const WorldHandle worldHandle,
        const CharacterConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->CreateCharacter(configuration);
        }

        return {};
    }

    bool System::UpdateCharacter(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const CharacterConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->UpdateCharacter(characterHandle, configuration);
    }

    bool System::DestroyCharacter(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->DestroyCharacter(characterHandle);
    }

    bool System::MoveCharacter(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const AZ::Vector3& velocity,
        const float fixedTimeStep)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->MoveCharacter(characterHandle, velocity, fixedTimeStep);
    }

    bool System::GetCharacterState(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        CharacterState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetCharacterState(characterHandle, state);
    }

    bool System::GetCharacterConfiguration(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        CharacterConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetCharacterConfiguration(characterHandle, configuration);
    }

    bool System::RaycastClosest(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->RaycastClosest(request, hit);
    }

    BufferResult System::RaycastClosestBatch(
        const WorldHandle worldHandle,
        const AZStd::span<const RaycastRequest> requests,
        const AZStd::span<ClosestQueryResult> results) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->RaycastClosestBatch(requests, results);
        }

        return {0, requests.size()};
    }

    QueryResult System::Raycast(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->Raycast(request, hits);
        }

        return {};
    }

    QueryResult System::ShapeCast(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        const AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->ShapeCast(request, hits);
        }

        return {};
    }

    QueryResult System::Overlap(
        const WorldHandle worldHandle,
        const OverlapRequest& request,
        const AZStd::span<OverlapHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->Overlap(request, hits);
        }

        return {};
    }

    QueryResult System::Overlap(
        const WorldHandle worldHandle,
        const OverlapRequest& request,
        const AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->Overlap(request, hits);
        }

        return {};
    }

    QueryResult System::OverlapAabb(
        const WorldHandle worldHandle,
        const AabbOverlapRequest& request,
        const AZStd::span<OverlapHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->OverlapAabb(request, hits);
        }

        return {};
    }

    QueryResult System::OverlapAabb(
        const WorldHandle worldHandle,
        const AabbOverlapRequest& request,
        const AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->OverlapAabb(request, hits);
        }

        return {};
    }

    StepEvents System::GetStepEvents(
        const WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->GetStepEvents();
        }

        return {};
    }

    bool System::SetContactCallbacks(
        const WorldHandle worldHandle,
        const CollisionFilterCallback collisionFilterCallback,
        const PreSolveCallback preSolveCallback,
        void* userData)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->SetContactCallbacks(collisionFilterCallback, preSolveCallback, userData);
    }

    bool System::GetWorldStatistics(
        const WorldHandle worldHandle,
        const StatisticsFlags flags,
        WorldStatistics& statistics) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->GetStatistics(flags, statistics);
    }

    bool System::StartRecording(
        const WorldHandle worldHandle,
        const size_t initialCapacityBytes)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->StartRecording(initialCapacityBytes);
    }

    bool System::StopRecording(
        const WorldHandle worldHandle,
        AZStd::vector<AZ::u8>& data)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world)
        {
            return world->StopRecording(data);
        }
        data.clear();
        return false;
    }

    bool System::ValidateRecording(
        const AZStd::span<const AZ::u8> data,
        const AZ::u32 workerCount) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::ValidateRecording");
        return Box3D::ValidateRecording(data, workerCount);
    }

    AZStd::unique_ptr<IReplay> System::CreateReplay(
        const AZStd::span<const AZ::u8> data,
        const AZ::u32 workerCount) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CreateReplay");
        return Box3D::CreateReplay(data, workerCount);
    }

    bool System::DrawWorld(
        const WorldHandle worldHandle,
        const DebugDrawSettings& settings,
        IDebugRenderer& renderer) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world && world->Draw(settings, renderer);
    }

    bool System::RebuildStaticTree(
        const WorldHandle worldHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->RebuildStaticTree();
    }

    bool System::Explode(
        const WorldHandle worldHandle,
        const ExplosionConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->Explode(configuration);
    }

    bool System::ApplyWind(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WindConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world && world->ApplyWind(bodyHandle, configuration);
    }

    SurfaceMaterial System::ResolveMaterial(
        const MaterialHandle materialHandle) const
    {
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (!slot)
        {
            return {};
        }

        const MaterialConfiguration& configuration = m_materialConfigurations[materialIndex];
        SurfaceMaterial material;
        material.m_tangentVelocity = configuration.m_tangentVelocity;
        material.m_userId = Internal::HandleAccess::GetValue(materialHandle);
        if (configuration.m_debugAppearanceEnabled)
        {
            material.m_debugColor = (static_cast<AZ::u32>(configuration.m_debugMaterialPreset) << 24)
                | (static_cast<AZ::u32>(configuration.m_debugColor.GetR8()) << 16)
                | (static_cast<AZ::u32>(configuration.m_debugColor.GetG8()) << 8) | configuration.m_debugColor.GetB8();
        }
        material.m_friction = configuration.m_friction;
        material.m_restitution = configuration.m_restitution;
        material.m_rollingResistance = configuration.m_rollingResistance;
        return material;
    }

    World* System::FindWorldInstance(
        const WorldHandle worldHandle)
    {
        return const_cast<World*>(static_cast<const System&>(*this).FindWorldInstance(worldHandle));
    }

    const World* System::FindWorldInstance(
        const WorldHandle worldHandle) const
    {
        if (worldHandle == m_defaultWorldHandle)
        {
            return m_defaultWorldInstance;
        }
        Internal::WorldHandleParts parts;
        if (!Internal::DecodeWorldHandle(worldHandle, parts) || parts.m_index >= m_worldSlots.size())
        {
            return nullptr;
        }
        const WorldSlot& slot = m_worldSlots[parts.m_index];
        if (slot.m_generation == parts.m_generation)
        {
            return slot.m_world.get();
        }

        return nullptr;
    }

    System::MaterialSlot* System::FindMaterialSlot(
        const MaterialHandle materialHandle,
        AZ::u32* materialIndex)
    {
        return const_cast<MaterialSlot*>(static_cast<const System&>(*this).FindMaterialSlot(materialHandle, materialIndex));
    }

    const System::MaterialSlot* System::FindMaterialSlot(
        const MaterialHandle materialHandle,
        AZ::u32* materialIndex) const
    {
        AZ::u32 resolvedMaterialIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(materialHandle, resolvedMaterialIndex, generation)
            || resolvedMaterialIndex >= m_materialSlots.size())
        {
            return nullptr;
        }
        const MaterialSlot& slot = m_materialSlots[resolvedMaterialIndex];
        if (slot.m_generation != generation)
        {
            return nullptr;
        }
        if (materialIndex)
        {
            *materialIndex = resolvedMaterialIndex;
        }
        return &slot;
    }

    System::CookedShapeSlot* System::FindCookedShapeSlot(
        const CookedShapeHandle cookedShapeHandle,
        AZ::u32* cookedShapeIndex)
    {
        return const_cast<CookedShapeSlot*>(static_cast<const System&>(*this).FindCookedShapeSlot(cookedShapeHandle, cookedShapeIndex));
    }

    const System::CookedShapeSlot* System::FindCookedShapeSlot(
        const CookedShapeHandle cookedShapeHandle,
        AZ::u32* cookedShapeIndex) const
    {
        AZ::u32 resolvedCookedShapeIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(cookedShapeHandle, resolvedCookedShapeIndex, generation)
            || resolvedCookedShapeIndex >= m_cookedShapeSlots.size())
        {
            return nullptr;
        }
        const CookedShapeSlot& slot = m_cookedShapeSlots[resolvedCookedShapeIndex];
        if (slot.m_generation != generation)
        {
            return nullptr;
        }
        if (cookedShapeIndex)
        {
            *cookedShapeIndex = resolvedCookedShapeIndex;
        }
        return &slot;
    }

    bool System::UsesCookedMaterial(
        const MaterialHandle materialHandle) const
    {
        for (size_t cookedShapeIndex = 0; cookedShapeIndex < m_cookedShapeSlots.size(); ++cookedShapeIndex)
        {
            if (m_cookedShapeSlots[cookedShapeIndex].m_generation != 0)
            {
                const AZStd::vector<MaterialHandle>& materials = m_cookedShapeResources[cookedShapeIndex].m_materials;
                if (AZStd::find(materials.begin(), materials.end(), materialHandle) != materials.end())
                {
                    return true;
                }
            }
        }
        return false;
    }

    SurfaceTypeId System::ResolveSurfaceType(
        const AZ::u64 materialId) const
    {
        const MaterialHandle materialHandle = Internal::HandleAccess::Create<MaterialHandle>(materialId);
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (slot)
        {
            return m_materialConfigurations[materialIndex].m_surfaceTypeId;
        }

        return {};
    }

    float System::MixFriction(
        const float valueA,
        const AZ::u64 materialIdA,
        const float valueB,
        const AZ::u64 materialIdB) const
    {
        const MaterialMixCallback callback = m_configuration.m_frictionCallback;
        if (!callback)
        {
            return std::sqrt(valueA * valueB);
        }
        const float mixed = callback(valueA, ResolveSurfaceType(materialIdA), valueB, ResolveSurfaceType(materialIdB));
        if (AZ::IsFiniteFloat(mixed) && mixed >= 0.0f)
        {
            return mixed;
        }

        return std::sqrt(valueA * valueB);
    }

    float System::MixRestitution(
        const float valueA,
        const AZ::u64 materialIdA,
        const float valueB,
        const AZ::u64 materialIdB) const
    {
        const MaterialMixCallback callback = m_configuration.m_restitutionCallback;
        if (!callback)
        {
            return AZStd::max(valueA, valueB);
        }
        const float mixed = callback(valueA, ResolveSurfaceType(materialIdA), valueB, ResolveSurfaceType(materialIdB));
        if (AZ::IsFiniteFloat(mixed) && mixed >= 0.0f)
        {
            return mixed;
        }

        return AZStd::max(valueA, valueB);
    }

    void System::UpdateCompatibilityFingerprint()
    {
#if defined(BOX3D_DOUBLE_PRECISION)
        constexpr AZStd::string_view precision = "double";
#else
        constexpr AZStd::string_view precision = "float";
#endif
        const Version version = GetVersion();
        m_compatibilityFingerprint = AZStd::string::format(
            "Box3D/%d.%d.%d;precision=" AZ_STRING_FORMAT ";fp=precise;substeps=%u;workers=%u;sleep=%u;continuous=%u;warm=%u;speculative=%u;"
            "contact=%.9g,%.9g,%.9g;recycle=%.9g;restitution=%.9g;hit=%.9g;max-speed=%.9g;length-units=%.9g",
            version.m_major,
            version.m_minor,
            version.m_revision,
            AZ_STRING_ARG(precision),
            m_configuration.m_subStepCount,
            m_configuration.m_workerCount,
            m_configuration.m_enableSleep,
            m_configuration.m_enableContinuous,
            m_configuration.m_enableWarmStarting,
            m_configuration.m_enableSpeculative,
            m_configuration.m_contactHertz,
            m_configuration.m_contactDampingRatio,
            m_configuration.m_contactSpeed,
            m_configuration.m_contactRecycleDistance,
            m_configuration.m_restitutionThreshold,
            m_configuration.m_hitEventThreshold,
            m_configuration.m_maximumLinearSpeed,
            m_configuration.m_lengthUnitsPerMeter);
    }
} // namespace Box3D
