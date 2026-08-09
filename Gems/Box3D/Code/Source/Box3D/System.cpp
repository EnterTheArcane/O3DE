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

        bool IsValidMaterialConfiguration(const MaterialConfiguration& configuration)
        {
            const auto isNonNegative = [](float value)
            {
                return AZ::IsFiniteFloat(value) && value >= 0.0f;
            };
            const bool validPreset = configuration.m_debugMaterialPreset >= DebugMaterialPreset::Default &&
                configuration.m_debugMaterialPreset <= DebugMaterialPreset::Metallic;
            return configuration.m_tangentVelocity.IsFinite() && configuration.m_debugColor.IsFinite() &&
                isNonNegative(configuration.m_friction) && isNonNegative(configuration.m_restitution) &&
                isNonNegative(configuration.m_rollingResistance) && isNonNegative(configuration.m_density) &&
                isNonNegative(configuration.m_explosionScale) && validPreset;
        }
    } // namespace

    System::System(const SystemConfiguration& configuration, AZ::JobContext* defaultJobContext)
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

    void System::UpdateConfiguration(const SystemConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::UpdateConfiguration");
        const DeterministicFloatScope floatScope;
        if (configuration != m_configuration)
        {
            for (const WorldSlot& slot : m_worldSlots)
            {
                if (slot.m_world != nullptr)
                {
                    AZStd::lock_guard lock(slot.m_world->m_mutex);
                    if (slot.m_world->m_recording != nullptr)
                    {
                        AZ_Warning("Box3D", false, "System configuration cannot be changed while a world is recording.");
                        return;
                    }
                }
            }
        }
        m_configuration = configuration;
        m_configuration.m_subStepCount = AZStd::max(m_configuration.m_subStepCount, AZ::u32{ 1 });
        m_configuration.m_workerCount = AZStd::clamp(m_configuration.m_workerCount, AZ::u32{ 1 }, MaximumWorkerCount);
        const auto nonNegativeOr = [](float value, float fallback)
        {
            return AZ::IsFiniteFloat(value) && value >= 0.0f ? value : fallback;
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
            if (slot.m_world != nullptr)
            {
                slot.m_world->Reconfigure(m_configuration);
            }
        }
    }

    const SystemConfiguration& System::GetConfiguration() const
    {
        return m_configuration;
    }

    WorldHandle System::CreateWorld(const WorldConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CreateWorld");
        if (configuration.m_name.IsEmpty() || FindWorld(configuration.m_name))
        {
            AZ_Error("Box3D", false, "A world requires a unique non-empty name.");
            return {};
        }
        if (!configuration.m_gravity.IsFinite() || !AZ::IsFiniteFloat(configuration.m_fixedTimeStep) ||
            configuration.m_fixedTimeStep <= 0.0f || configuration.m_maximumCatchUpSteps == 0)
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

    bool System::DestroyWorld(WorldHandle worldHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyWorld");
        Internal::WorldHandleParts parts;
        if (!Internal::DecodeWorldHandle(worldHandle, parts) || parts.m_index >= m_worldSlots.size())
        {
            return false;
        }
        WorldSlot& slot = m_worldSlots[parts.m_index];
        if (slot.m_world == nullptr || slot.m_generation != parts.m_generation)
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
                if (candidate.m_world != nullptr)
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

    const IWorldQueries* System::GetWorldQueries(WorldHandle worldHandle) const
    {
        return FindWorldInstance(worldHandle);
    }

    WorldHandle System::FindWorld(AZ::Name name) const
    {
        for (const WorldSlot& slot : m_worldSlots)
        {
            if (slot.m_world != nullptr)
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

    bool System::GetWorldConfiguration(WorldHandle worldHandle, WorldConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        if (world == nullptr)
        {
            return false;
        }
        configuration = world->GetConfiguration();
        return true;
    }

    AZ::Aabb System::GetWorldAabb(WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetAabb() : AZ::Aabb::CreateNull();
    }

    bool System::IsValid(WorldHandle worldHandle) const
    {
        return FindWorldInstance(worldHandle) != nullptr;
    }

    bool System::SetWorldEnabled(WorldHandle worldHandle, bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetEnabled(enabled);
    }

    bool System::IsWorldEnabled(WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->IsEnabled();
    }

    bool System::SetWorldGravity(WorldHandle worldHandle, const AZ::Vector3& gravity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetGravity(gravity);
    }

    bool System::GetWorldGravity(WorldHandle worldHandle, AZ::Vector3& gravity) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetGravity(gravity);
    }

    bool System::StepWorld(WorldHandle worldHandle, float fixedTimeStep)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world == nullptr || !world->Step(fixedTimeStep, m_configuration.m_subStepCount))
        {
            return false;
        }
        DispatchStepEvents(*world);
        return true;
    }

    void System::StepAutoSimulatedWorlds(float deltaTime)
    {
        for (WorldSlot& slot : m_worldSlots)
        {
            if (slot.m_world != nullptr)
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

    void System::DispatchStepEvents(const World& world) const
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

    SimulationTick System::GetLastCompletedTick(WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetLastCompletedTick() : 0;
    }

    AZ::u64 System::GetStateDigest(WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetStateDigest() : 0;
    }

    AZStd::string_view System::GetCompatibilityFingerprint() const
    {
        return m_compatibilityFingerprint;
    }

    MaterialHandle System::CreateMaterial(const MaterialConfiguration& configuration)
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

    bool System::UpdateMaterial(MaterialHandle materialHandle, const MaterialConfiguration& configuration)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::UpdateMaterial");
        if (!IsValidMaterialConfiguration(configuration) || UsesCookedMaterial(materialHandle))
        {
            return false;
        }

        AZ::u32 materialIndex = 0;
        MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (slot == nullptr)
        {
            return false;
        }
        MaterialConfiguration& retainedConfiguration = m_materialConfigurations[materialIndex];
        const MaterialConfiguration previousConfiguration = retainedConfiguration;
        retainedConfiguration = configuration;
        for (WorldSlot& worldSlot : m_worldSlots)
        {
            if (worldSlot.m_world != nullptr && !worldSlot.m_world->RefreshMaterial(materialHandle))
            {
                retainedConfiguration = previousConfiguration;
                for (WorldSlot& rollbackWorldSlot : m_worldSlots)
                {
                    if (rollbackWorldSlot.m_world != nullptr)
                    {
                        [[maybe_unused]] const bool restored = rollbackWorldSlot.m_world->RefreshMaterial(materialHandle);
                    }
                }
                return false;
            }
        }
        return true;
    }

    bool System::GetMaterial(MaterialHandle materialHandle, MaterialConfiguration& configuration) const
    {
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (slot == nullptr)
        {
            return false;
        }
        configuration = m_materialConfigurations[materialIndex];
        return true;
    }

    bool System::DestroyMaterial(MaterialHandle materialHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyMaterial");
        AZ::u32 materialIndex = 0;
        AZ::u32 generation = 0;
        MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (slot == nullptr || !Internal::DecodeRegistryHandle(materialHandle, materialIndex, generation))
        {
            return false;
        }
        if (UsesCookedMaterial(materialHandle))
        {
            return false;
        }
        for (const WorldSlot& worldSlot : m_worldSlots)
        {
            if (worldSlot.m_world != nullptr && worldSlot.m_world->UsesMaterial(materialHandle))
            {
                return false;
            }
        }
        m_materialConfigurations[materialIndex] = {};
        slot->m_generation = 0;
        m_freeMaterialSlots.push_back(materialIndex);
        return true;
    }

    CookedShapeHandle System::CookShape(const ShapeConfiguration& configuration)
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
            if (FindMaterialSlot(materialHandle) == nullptr)
            {
                return {};
            }
            nativeMaterials.push_back(ResolveMaterial(materialHandle));
        }

        NativeGeometry nativeGeometry =
            CookGeometry(configuration.m_geometry, GeometryTransform{ configuration.m_properties.m_localTransform }, nativeMaterials);
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
                    return geometry != nullptr;
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

    bool System::DestroyCookedShape(CookedShapeHandle cookedShapeHandle)
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::DestroyCookedShape");
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        AZ::u32 generation = 0;
        CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (slot == nullptr || !Internal::DecodeRegistryHandle(cookedShapeHandle, cookedShapeIndex, generation))
        {
            return false;
        }

        *slot = CookedShapeSlot{};
        m_cookedShapeResources[cookedShapeIndex] = CookedShapeResources{};
        m_freeCookedShapeSlots.push_back(cookedShapeIndex);
        return true;
    }

    bool System::IsValid(CookedShapeHandle cookedShapeHandle) const
    {
        return FindCookedShapeSlot(cookedShapeHandle) != nullptr;
    }

    AZ::Aabb System::GetAabb(CookedShapeHandle cookedShapeHandle) const
    {
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        return slot != nullptr ? Box3D::GetAabb(m_cookedShapeResources[cookedShapeIndex].m_geometry) : AZ::Aabb::CreateNull();
    }

    bool System::Raycast(
        CookedShapeHandle cookedShapeHandle, const AZ::Vector3& start, const AZ::Vector3& direction, float distance, GeometryHit& hit) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::RaycastCookedShape");
        const DeterministicFloatScope floatScope;
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* slot = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (slot == nullptr || !start.IsFinite() || !direction.IsFinite() || direction.IsZero() || !AZ::IsFiniteFloat(distance) ||
            distance <= 0.0f)
        {
            return false;
        }

        GeometryCastHit nativeHit;
        if (!Box3D::CastRay(m_cookedShapeResources[cookedShapeIndex].m_geometry, start, direction.GetNormalized() * distance, nativeHit))
        {
            return false;
        }
        hit = { nativeHit.m_position,      nativeHit.m_normal,        nativeHit.m_fraction * distance, nativeHit.m_fraction,
                nativeHit.m_materialIndex, nativeHit.m_triangleIndex, nativeHit.m_childIndex };
        return true;
    }

    BodyHandle System::CreateBody(WorldHandle worldHandle, const RigidBodyConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->CreateBody(configuration) : BodyHandle{};
    }

    bool System::DestroyBody(WorldHandle worldHandle, BodyHandle bodyHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->DestroyBody(bodyHandle);
    }

    bool System::GetBodyState(WorldHandle worldHandle, BodyHandle bodyHandle, BodyState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyState(bodyHandle, state);
    }

    AZ::Name System::GetBodyName(WorldHandle worldHandle, BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodyName(bodyHandle) : AZ::Name{};
    }

    bool System::SetBodyName(WorldHandle worldHandle, BodyHandle bodyHandle, AZ::Name name)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyName(bodyHandle, name);
    }

    bool System::GetBodyProperties(WorldHandle worldHandle, BodyHandle bodyHandle, BodyProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyProperties(bodyHandle, properties);
    }

    bool System::SetBodyProperties(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyProperties(bodyHandle, properties);
    }

    bool System::SetBodyAwake(WorldHandle worldHandle, BodyHandle bodyHandle, bool awake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyAwake(bodyHandle, awake);
    }

    bool System::SetBodyEnabled(WorldHandle worldHandle, BodyHandle bodyHandle, bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyEnabled(bodyHandle, enabled);
    }

    bool System::SetBodyHitEventsEnabled(WorldHandle worldHandle, BodyHandle bodyHandle, bool enabled)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyHitEventsEnabled(bodyHandle, enabled);
    }

    bool System::SetBodyTransform(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Transform& transform)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetBodyTransform(bodyHandle, transform);
    }

    bool System::GetBodyLocalPoint(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldPoint, AZ::Vector3& localPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyLocalPoint(bodyHandle, worldPoint, localPoint);
    }

    bool System::GetBodyWorldPoint(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localPoint, AZ::Vector3& worldPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyWorldPoint(bodyHandle, localPoint, worldPoint);
    }

    bool System::GetBodyLocalVector(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldVector, AZ::Vector3& localVector) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyLocalVector(bodyHandle, worldVector, localVector);
    }

    bool System::GetBodyWorldVector(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localVector, AZ::Vector3& worldVector) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyWorldVector(bodyHandle, localVector, worldVector);
    }

    bool System::SetLinearVelocity(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& velocity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetLinearVelocity(bodyHandle, velocity);
    }

    bool System::SetAngularVelocity(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& velocity)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetAngularVelocity(bodyHandle, velocity);
    }

    AZ::Vector3 System::GetLinearVelocityAtLocalPoint(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& localPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetLinearVelocityAtLocalPoint(bodyHandle, localPoint) : AZ::Vector3::CreateZero();
    }

    AZ::Vector3 System::GetLinearVelocityAtWorldPoint(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& worldPoint) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetLinearVelocityAtWorldPoint(bodyHandle, worldPoint) : AZ::Vector3::CreateZero();
    }

    bool System::SetKinematicTarget(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Transform& transform, float fixedTimeStep, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetKinematicTarget(bodyHandle, transform, fixedTimeStep, wake);
    }

    bool System::ApplyLinearImpulse(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& impulse, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyLinearImpulse(bodyHandle, impulse, wake);
    }

    bool System::ApplyLinearImpulseAtWorldPoint(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& impulse, const AZ::Vector3& worldPoint, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyLinearImpulseAtWorldPoint(bodyHandle, impulse, worldPoint, wake);
    }

    bool System::ApplyAngularImpulse(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& impulse, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyAngularImpulse(bodyHandle, impulse, wake);
    }

    bool System::ApplyForce(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& force, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyForce(bodyHandle, force, wake);
    }

    bool System::ApplyForceAtWorldPoint(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& force, const AZ::Vector3& worldPoint, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyForceAtWorldPoint(bodyHandle, force, worldPoint, wake);
    }

    bool System::ApplyTorque(WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& torque, bool wake)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyTorque(bodyHandle, torque, wake);
    }

    bool System::GetMassProperties(WorldHandle worldHandle, BodyHandle bodyHandle, MassProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetMassProperties(bodyHandle, properties);
    }

    bool System::SetMassProperties(WorldHandle worldHandle, BodyHandle bodyHandle, const MassProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetMassProperties(bodyHandle, properties);
    }

    bool System::RecomputeMassFromShapes(WorldHandle worldHandle, BodyHandle bodyHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->RecomputeMassFromShapes(bodyHandle);
    }

    AZ::Matrix3x3 System::GetWorldInverseInertia(WorldHandle worldHandle, BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetWorldInverseInertia(bodyHandle) : AZ::Matrix3x3::CreateZero();
    }

    AZ::Vector3 System::GetWorldCenterOfMass(WorldHandle worldHandle, BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetWorldCenterOfMass(bodyHandle) : AZ::Vector3::CreateZero();
    }

    bool System::GetBodyClosestPoint(
        WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetBodyClosestPoint(bodyHandle, target, position, distance);
    }

    AZ::Aabb System::GetBodyAabb(WorldHandle worldHandle, BodyHandle bodyHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodyAabb(bodyHandle) : AZ::Aabb::CreateNull();
    }

    BufferResult System::GetBodyShapes(WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<ShapeHandle> shapeHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodyShapes(bodyHandle, shapeHandles) : BufferResult{};
    }

    BufferResult System::GetBodyJoints(WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<JointHandle> jointHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodyJoints(bodyHandle, jointHandles) : BufferResult{};
    }

    ContactSnapshotResult System::GetBodyContacts(
        WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<ContactSnapshot> contacts, AZStd::span<ContactPoint> points) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodyContacts(bodyHandle, contacts, points) : ContactSnapshotResult{};
    }

    BufferResult System::GetBodySensorOverlaps(WorldHandle worldHandle, BodyHandle bodyHandle, AZStd::span<SensorOverlap> overlaps) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetBodySensorOverlaps(bodyHandle, overlaps) : BufferResult{};
    }

    bool System::RaycastBody(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyRaycastRequest& request, QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->RaycastBody(bodyHandle, request, hit);
    }

    bool System::ShapeCastBody(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyShapeCastRequest& request, QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ShapeCastBody(bodyHandle, request, hit);
    }

    bool System::OverlapBody(WorldHandle worldHandle, BodyHandle bodyHandle, const BodyOverlapRequest& request) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->OverlapBody(bodyHandle, request);
    }

    ShapeHandle System::CreateShape(WorldHandle worldHandle, BodyHandle bodyHandle, const ShapeConfiguration& configuration)
    {
        return CreateShape(worldHandle, bodyHandle, configuration, 1.0f);
    }

    ShapeHandle System::CreateShape(
        WorldHandle worldHandle, BodyHandle bodyHandle, const ShapeConfiguration& configuration, float uniformScale)
    {
        if (!configuration.m_materialConfigurations.empty())
        {
            AZ_Error("Box3D", false, "Direct shape creation requires material handles, not serialized material configurations.");
            return {};
        }
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->CreateShape(bodyHandle, configuration, uniformScale) : ShapeHandle{};
    }

    ShapeHandle System::CreateShapeFromCooked(
        WorldHandle worldHandle, BodyHandle bodyHandle, CookedShapeHandle cookedShapeHandle, const ShapeProperties& properties)
    {
        World* world = FindWorldInstance(worldHandle);
        AZ::u32 cookedShapeIndex = 0;
        const CookedShapeSlot* cookedShape = FindCookedShapeSlot(cookedShapeHandle, &cookedShapeIndex);
        if (world == nullptr || cookedShape == nullptr || !properties.m_materials.empty() ||
            !properties.m_localTransform.GetTranslation().IsZero() || !properties.m_localTransform.GetRotation().IsIdentity() ||
            !AZ::IsClose(properties.m_localTransform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance))
        {
            return {};
        }

        ShapeProperties instanceProperties = properties;
        const CookedShapeResources& resources = m_cookedShapeResources[cookedShapeIndex];
        instanceProperties.m_materials = resources.m_materials;
        return world->CreateShapeFromCooked(bodyHandle, resources.m_geometry, instanceProperties);
    }

    bool System::UpdateShape(WorldHandle worldHandle, ShapeHandle shapeHandle, const ShapeConfiguration& configuration)
    {
        return UpdateShape(worldHandle, shapeHandle, configuration, 1.0f);
    }

    bool System::UpdateShape(WorldHandle worldHandle, ShapeHandle shapeHandle, const ShapeConfiguration& configuration, float uniformScale)
    {
        if (!configuration.m_materialConfigurations.empty())
        {
            AZ_Error("Box3D", false, "Direct shape updates require material handles, not serialized material configurations.");
            return false;
        }
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->UpdateShape(shapeHandle, configuration, uniformScale);
    }

    bool System::DestroyShape(WorldHandle worldHandle, ShapeHandle shapeHandle, bool updateBodyMass)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->DestroyShape(shapeHandle, updateBodyMass);
    }

    bool System::SetShapeCollisionFilter(WorldHandle worldHandle, ShapeHandle shapeHandle, const CollisionFilter& collisionFilter)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeCollisionFilter(shapeHandle, collisionFilter);
    }

    bool System::SetShapeMaterials(WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<const MaterialHandle> materials)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeMaterials(shapeHandle, materials);
    }

    AZ::Aabb System::GetShapeAabb(WorldHandle worldHandle, ShapeHandle shapeHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetShapeAabb(shapeHandle) : AZ::Aabb::CreateNull();
    }

    bool System::GetShapeState(WorldHandle worldHandle, ShapeHandle shapeHandle, ShapeState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetShapeState(shapeHandle, state);
    }

    BufferResult System::GetShapeMaterials(
        WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<MaterialHandle> materialHandles) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetShapeMaterials(shapeHandle, materialHandles) : BufferResult{};
    }

    bool System::SetShapeDensity(WorldHandle worldHandle, ShapeHandle shapeHandle, float density, bool updateBodyMass)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeDensity(shapeHandle, density, updateBodyMass);
    }

    bool System::SetShapeFriction(WorldHandle worldHandle, ShapeHandle shapeHandle, float friction)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeFriction(shapeHandle, friction);
    }

    bool System::SetShapeRestitution(WorldHandle worldHandle, ShapeHandle shapeHandle, float restitution)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeRestitution(shapeHandle, restitution);
    }

    bool System::SetShapeEventSubscriptions(
        WorldHandle worldHandle, ShapeHandle shapeHandle, bool sensorEvents, bool contactEvents, bool hitEvents, bool preSolveEvents)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetShapeEventSubscriptions(shapeHandle, sensorEvents, contactEvents, hitEvents, preSolveEvents);
    }

    bool System::GetShapeMassProperties(WorldHandle worldHandle, ShapeHandle shapeHandle, MassProperties& properties) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetShapeMassProperties(shapeHandle, properties);
    }

    bool System::GetShapeClosestPoint(
        WorldHandle worldHandle, ShapeHandle shapeHandle, const AZ::Vector3& target, AZ::Vector3& position, float& distance) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetShapeClosestPoint(shapeHandle, target, position, distance);
    }

    bool System::RaycastShape(
        WorldHandle worldHandle,
        ShapeHandle shapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        float distance,
        QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->RaycastShape(shapeHandle, start, direction, distance, hit);
    }

    ContactSnapshotResult System::GetShapeContacts(
        WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<ContactSnapshot> contacts, AZStd::span<ContactPoint> points) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetShapeContacts(shapeHandle, contacts, points) : ContactSnapshotResult{};
    }

    BufferResult System::GetShapeSensorOverlaps(WorldHandle worldHandle, ShapeHandle shapeHandle, AZStd::span<SensorOverlap> overlaps) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetShapeSensorOverlaps(shapeHandle, overlaps) : BufferResult{};
    }

    JointHandle System::CreateJoint(WorldHandle worldHandle, const JointConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->CreateJoint(configuration) : JointHandle{};
    }

    bool System::SetJointEntityId(WorldHandle worldHandle, JointHandle jointHandle, AZ::EntityId entityId)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetJointEntityId(jointHandle, entityId);
    }

    bool System::UpdateJoint(WorldHandle worldHandle, JointHandle jointHandle, const JointConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->UpdateJoint(jointHandle, configuration);
    }

    bool System::DestroyJoint(WorldHandle worldHandle, JointHandle jointHandle, bool wakeAttachedBodies)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->DestroyJoint(jointHandle, wakeAttachedBodies);
    }

    bool System::WakeJointBodies(WorldHandle worldHandle, JointHandle jointHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->WakeJointBodies(jointHandle);
    }

    bool System::GetJointConfiguration(WorldHandle worldHandle, JointHandle jointHandle, JointConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetJointConfiguration(jointHandle, configuration);
    }

    bool System::GetJointMeasurements(WorldHandle worldHandle, JointHandle jointHandle, JointMeasurements& measurements) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetJointMeasurements(jointHandle, measurements);
    }

    CharacterHandle System::CreateCharacter(WorldHandle worldHandle, const CharacterConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->CreateCharacter(configuration) : CharacterHandle{};
    }

    bool System::UpdateCharacter(WorldHandle worldHandle, CharacterHandle characterHandle, const CharacterConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->UpdateCharacter(characterHandle, configuration);
    }

    bool System::DestroyCharacter(WorldHandle worldHandle, CharacterHandle characterHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->DestroyCharacter(characterHandle);
    }

    bool System::MoveCharacter(WorldHandle worldHandle, CharacterHandle characterHandle, const AZ::Vector3& velocity, float fixedTimeStep)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->MoveCharacter(characterHandle, velocity, fixedTimeStep);
    }

    bool System::GetCharacterState(WorldHandle worldHandle, CharacterHandle characterHandle, CharacterState& state) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetCharacterState(characterHandle, state);
    }

    bool System::GetCharacterConfiguration(
        WorldHandle worldHandle, CharacterHandle characterHandle, CharacterConfiguration& configuration) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetCharacterConfiguration(characterHandle, configuration);
    }

    bool System::RaycastClosest(WorldHandle worldHandle, const RaycastRequest& request, QueryHit& hit) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->RaycastClosest(request, hit);
    }

    BufferResult System::RaycastClosestBatch(
        WorldHandle worldHandle, AZStd::span<const RaycastRequest> requests, AZStd::span<ClosestQueryResult> results) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->RaycastClosestBatch(requests, results) : BufferResult{ 0, requests.size() };
    }

    QueryResult System::Raycast(WorldHandle worldHandle, const RaycastRequest& request, AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->Raycast(request, hits) : QueryResult{};
    }

    QueryResult System::ShapeCast(WorldHandle worldHandle, const ShapeCastRequest& request, AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->ShapeCast(request, hits) : QueryResult{};
    }

    QueryResult System::Overlap(WorldHandle worldHandle, const OverlapRequest& request, AZStd::span<OverlapHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->Overlap(request, hits) : QueryResult{};
    }

    QueryResult System::Overlap(WorldHandle worldHandle, const OverlapRequest& request, AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->Overlap(request, hits) : QueryResult{};
    }

    QueryResult System::OverlapAabb(WorldHandle worldHandle, const AabbOverlapRequest& request, AZStd::span<OverlapHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->OverlapAabb(request, hits) : QueryResult{};
    }

    QueryResult System::OverlapAabb(WorldHandle worldHandle, const AabbOverlapRequest& request, AZStd::span<QueryHit> hits) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->OverlapAabb(request, hits) : QueryResult{};
    }

    StepEvents System::GetStepEvents(WorldHandle worldHandle) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr ? world->GetStepEvents() : StepEvents{};
    }

    bool System::SetContactCallbacks(
        WorldHandle worldHandle, CollisionFilterCallback collisionFilterCallback, PreSolveCallback preSolveCallback, void* userData)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->SetContactCallbacks(collisionFilterCallback, preSolveCallback, userData);
    }

    bool System::GetWorldStatistics(WorldHandle worldHandle, StatisticsFlags flags, WorldStatistics& statistics) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->GetStatistics(flags, statistics);
    }

    bool System::StartRecording(WorldHandle worldHandle, size_t initialCapacityBytes)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->StartRecording(initialCapacityBytes);
    }

    bool System::StopRecording(WorldHandle worldHandle, AZStd::vector<AZ::u8>& data)
    {
        World* world = FindWorldInstance(worldHandle);
        if (world != nullptr)
        {
            return world->StopRecording(data);
        }
        data.clear();
        return false;
    }

    bool System::ValidateRecording(AZStd::span<const AZ::u8> data, AZ::u32 workerCount) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::ValidateRecording");
        return Box3D::ValidateRecording(data, workerCount);
    }

    AZStd::unique_ptr<IReplay> System::CreateReplay(AZStd::span<const AZ::u8> data, AZ::u32 workerCount) const
    {
        AZ_PROFILE_SCOPE(Physics, "Box3D::System::CreateReplay");
        return Box3D::CreateReplay(data, workerCount);
    }

    bool System::DrawWorld(WorldHandle worldHandle, const DebugDrawSettings& settings, IDebugRenderer& renderer) const
    {
        const World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->Draw(settings, renderer);
    }

    bool System::RebuildStaticTree(WorldHandle worldHandle)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->RebuildStaticTree();
    }

    bool System::Explode(WorldHandle worldHandle, const ExplosionConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->Explode(configuration);
    }

    bool System::ApplyWind(WorldHandle worldHandle, BodyHandle bodyHandle, const WindConfiguration& configuration)
    {
        World* world = FindWorldInstance(worldHandle);
        return world != nullptr && world->ApplyWind(bodyHandle, configuration);
    }

    SurfaceMaterial System::ResolveMaterial(MaterialHandle materialHandle) const
    {
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        if (slot == nullptr)
        {
            return {};
        }

        const MaterialConfiguration& configuration = m_materialConfigurations[materialIndex];
        SurfaceMaterial material;
        material.m_tangentVelocity = configuration.m_tangentVelocity;
        material.m_userId = Internal::HandleAccess::GetValue(materialHandle);
        if (configuration.m_debugAppearanceEnabled)
        {
            material.m_debugColor = (static_cast<AZ::u32>(configuration.m_debugMaterialPreset) << 24) |
                (static_cast<AZ::u32>(configuration.m_debugColor.GetR8()) << 16) |
                (static_cast<AZ::u32>(configuration.m_debugColor.GetG8()) << 8) | configuration.m_debugColor.GetB8();
        }
        material.m_friction = configuration.m_friction;
        material.m_restitution = configuration.m_restitution;
        material.m_rollingResistance = configuration.m_rollingResistance;
        return material;
    }

    World* System::FindWorldInstance(WorldHandle worldHandle)
    {
        return const_cast<World*>(static_cast<const System&>(*this).FindWorldInstance(worldHandle));
    }

    const World* System::FindWorldInstance(WorldHandle worldHandle) const
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
        return slot.m_generation == parts.m_generation ? slot.m_world.get() : nullptr;
    }

    System::MaterialSlot* System::FindMaterialSlot(MaterialHandle materialHandle, AZ::u32* materialIndex)
    {
        return const_cast<MaterialSlot*>(static_cast<const System&>(*this).FindMaterialSlot(materialHandle, materialIndex));
    }

    const System::MaterialSlot* System::FindMaterialSlot(MaterialHandle materialHandle, AZ::u32* materialIndex) const
    {
        AZ::u32 resolvedMaterialIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(materialHandle, resolvedMaterialIndex, generation) ||
            resolvedMaterialIndex >= m_materialSlots.size())
        {
            return nullptr;
        }
        const MaterialSlot& slot = m_materialSlots[resolvedMaterialIndex];
        if (slot.m_generation != generation)
        {
            return nullptr;
        }
        if (materialIndex != nullptr)
        {
            *materialIndex = resolvedMaterialIndex;
        }
        return &slot;
    }

    System::CookedShapeSlot* System::FindCookedShapeSlot(CookedShapeHandle cookedShapeHandle, AZ::u32* cookedShapeIndex)
    {
        return const_cast<CookedShapeSlot*>(static_cast<const System&>(*this).FindCookedShapeSlot(cookedShapeHandle, cookedShapeIndex));
    }

    const System::CookedShapeSlot* System::FindCookedShapeSlot(CookedShapeHandle cookedShapeHandle, AZ::u32* cookedShapeIndex) const
    {
        AZ::u32 resolvedCookedShapeIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(cookedShapeHandle, resolvedCookedShapeIndex, generation) ||
            resolvedCookedShapeIndex >= m_cookedShapeSlots.size())
        {
            return nullptr;
        }
        const CookedShapeSlot& slot = m_cookedShapeSlots[resolvedCookedShapeIndex];
        if (slot.m_generation != generation)
        {
            return nullptr;
        }
        if (cookedShapeIndex != nullptr)
        {
            *cookedShapeIndex = resolvedCookedShapeIndex;
        }
        return &slot;
    }

    bool System::UsesCookedMaterial(MaterialHandle materialHandle) const
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

    SurfaceTypeId System::ResolveSurfaceType(AZ::u64 materialId) const
    {
        const MaterialHandle materialHandle = Internal::HandleAccess::Create<MaterialHandle>(materialId);
        AZ::u32 materialIndex = 0;
        const MaterialSlot* slot = FindMaterialSlot(materialHandle, &materialIndex);
        return slot != nullptr ? m_materialConfigurations[materialIndex].m_surfaceTypeId : SurfaceTypeId{};
    }

    float System::MixFriction(float valueA, AZ::u64 materialIdA, float valueB, AZ::u64 materialIdB) const
    {
        const MaterialMixCallback callback = m_configuration.m_frictionCallback;
        if (callback == nullptr)
        {
            return std::sqrt(valueA * valueB);
        }
        const float mixed = callback(valueA, ResolveSurfaceType(materialIdA), valueB, ResolveSurfaceType(materialIdB));
        return AZ::IsFiniteFloat(mixed) && mixed >= 0.0f ? mixed : std::sqrt(valueA * valueB);
    }

    float System::MixRestitution(float valueA, AZ::u64 materialIdA, float valueB, AZ::u64 materialIdB) const
    {
        const MaterialMixCallback callback = m_configuration.m_restitutionCallback;
        if (callback == nullptr)
        {
            return AZStd::max(valueA, valueB);
        }
        const float mixed = callback(valueA, ResolveSurfaceType(materialIdA), valueB, ResolveSurfaceType(materialIdB));
        return AZ::IsFiniteFloat(mixed) && mixed >= 0.0f ? mixed : AZStd::max(valueA, valueB);
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
