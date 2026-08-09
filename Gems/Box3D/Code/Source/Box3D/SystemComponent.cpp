/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/SystemComponent.h>

#include <Box3D/CharacterConfiguration.h>
#include <Box3D/Effects.h>
#include <Box3D/Internal/HandleEncoding.h>
#include <Box3D/Joints.h>
#include <Box3D/Material.h>
#include <Box3D/NativeApi.h>
#include <Box3D/RigidBodyConfiguration.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobManagerBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/limits.h>

namespace Box3D
{
    class WorldNotificationBusBehaviorHandler final
        : public WorldNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            WorldNotificationBusBehaviorHandler,
            "{DA380918-67E3-4B54-8561-51B2EA83A348}",
            AZ::SystemAllocator,
            OnBodyMoved,
            OnSensor,
            OnContact,
            OnContactHit,
            OnJointThresholdExceeded);

        void OnBodyMoved(const BodyMoveEvent& event) override
        {
            Call(FN_OnBodyMoved, event);
        }

        void OnSensor(const SensorEvent& event) override
        {
            Call(FN_OnSensor, event);
        }

        void OnContact(const ContactEvent& event) override
        {
            Call(FN_OnContact, event);
        }

        void OnContactHit(const ContactHitEvent& event) override
        {
            Call(FN_OnContactHit, event);
        }

        void OnJointThresholdExceeded(const JointThresholdEvent& event) override
        {
            Call(FN_OnJointThresholdExceeded, event);
        }
    };

    namespace
    {
        template<class Query>
        QueryHitCollection CollectQueryHits(AZ::u32 maxHitCount, Query&& query)
        {
            QueryHitCollection result;
            result.m_hits.resize(maxHitCount);
            const QueryResult queryResult = query(AZStd::span<QueryHit>(result.m_hits));
            result.m_hits.resize(queryResult.m_hitCount);
            result.m_requiredHitCount = queryResult.m_requiredHitCount;
            return result;
        }

        template<class Geometry>
        QueryHitCollection ShapeCast(
            const System* system, WorldHandle worldHandle, const Geometry& geometry, const ConvexCastParameters& parameters)
        {
            if (system == nullptr)
            {
                return {};
            }
            ShapeCastRequest request;
            request.m_geometry = geometry;
            request.m_start = parameters.m_start;
            request.m_translation = parameters.m_translation;
            request.m_filter = parameters.m_filter;
            return CollectQueryHits(
                parameters.m_maxHitCount,
                [system, worldHandle, &request](AZStd::span<QueryHit> hits)
                {
                    return system->ShapeCast(worldHandle, request, hits);
                });
        }

        template<class Geometry>
        QueryHitCollection Overlap(
            const System* system, WorldHandle worldHandle, const Geometry& geometry, const ConvexOverlapParameters& parameters)
        {
            if (system == nullptr)
            {
                return {};
            }
            OverlapRequest request;
            request.m_geometry = geometry;
            request.m_transform = parameters.m_transform;
            request.m_filter = parameters.m_filter;
            return CollectQueryHits(
                parameters.m_maxHitCount,
                [system, worldHandle, &request](AZStd::span<QueryHit> hits)
                {
                    return system->Overlap(worldHandle, request, hits);
                });
        }
    } // namespace

    SystemComponent::SystemComponent() = default;
    SystemComponent::~SystemComponent() = default;

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<WorldHandle>("WorldHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &WorldHandle::IsValid)
                ->Method("Equal", &WorldHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<BodyHandle>("BodyHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &BodyHandle::IsValid)
                ->Method("Equal", &BodyHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<ShapeHandle>("ShapeHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &ShapeHandle::IsValid)
                ->Method("Equal", &ShapeHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<JointHandle>("JointHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &JointHandle::IsValid)
                ->Method("Equal", &JointHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<MaterialHandle>("MaterialHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &MaterialHandle::IsValid)
                ->Method("Equal", &MaterialHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<CharacterHandle>("CharacterHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &CharacterHandle::IsValid)
                ->Method("Equal", &CharacterHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<CookedShapeHandle>("CookedShapeHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &CookedShapeHandle::IsValid)
                ->Method("Equal", &CookedShapeHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);
            behaviorContext->Class<ReplayHandle>("ReplayHandle")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Method("IsValid", &ReplayHandle::IsValid)
                ->Method("Equal", &ReplayHandle::Equal)
                ->Attribute(AZ::Script::Attributes::Operator, AZ::Script::Attributes::OperatorType::Equal);

            behaviorContext->EBus<WorldRequestBus>("Box3DWorldRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("GetDefaultWorldHandle", &WorldRequests::GetDefaultWorldHandle)
                ->Event("CreateWorld", &WorldRequests::CreateWorld)
                ->Event("DestroyWorld", &WorldRequests::DestroyWorld)
                ->Event("StepWorld", &WorldRequests::StepWorld)
                ->Event("RaycastClosest", &WorldRequests::RaycastClosest)
                ->Event("RaycastClosestBatch", &WorldRequests::RaycastClosestBatch)
                ->Event("Raycast", &WorldRequests::Raycast)
                ->Event("OverlapAabb", &WorldRequests::OverlapAabb)
                ->Event("GetContactPointCount", &WorldRequests::GetContactPointCount)
                ->Event("GetContactPointAt", &WorldRequests::GetContactPointAt)
                ->Event("ShapeCastSphere", &WorldRequests::ShapeCastSphere)
                ->Event("OverlapSphere", &WorldRequests::OverlapSphere)
                ->Event("ShapeCastCapsule", &WorldRequests::ShapeCastCapsule)
                ->Event("OverlapCapsule", &WorldRequests::OverlapCapsule)
                ->Event("ShapeCastBox", &WorldRequests::ShapeCastBox)
                ->Event("OverlapBox", &WorldRequests::OverlapBox)
                ->Event("ShapeCastCylinder", &WorldRequests::ShapeCastCylinder)
                ->Event("OverlapCylinder", &WorldRequests::OverlapCylinder)
                ->Event("ShapeCastConvexHull", &WorldRequests::ShapeCastConvexHull)
                ->Event("OverlapConvexHull", &WorldRequests::OverlapConvexHull);
            behaviorContext->Class<MaterialResult>("MaterialResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("configuration", BehaviorValueProperty(&MaterialResult::m_configuration))
                ->Property("found", BehaviorValueProperty(&MaterialResult::m_found));
            behaviorContext->EBus<MaterialRequestBus>("Box3DMaterialRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("CreateMaterial", &MaterialRequests::CreateMaterial)
                ->Event("UpdateMaterial", &MaterialRequests::UpdateMaterial)
                ->Event("GetMaterial", &MaterialRequests::GetMaterial)
                ->Event("DestroyMaterial", &MaterialRequests::DestroyMaterial);
            behaviorContext->Class<CookedRaycastResult>("CookedRaycastResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("hit", BehaviorValueProperty(&CookedRaycastResult::m_hit))
                ->Property("found", BehaviorValueProperty(&CookedRaycastResult::m_found));
            behaviorContext->EBus<CookingRequestBus>("Box3DCookingRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("CookSphere", &CookingRequests::CookSphere)
                ->Event("CookCapsule", &CookingRequests::CookCapsule)
                ->Event("CookBox", &CookingRequests::CookBox)
                ->Event("CookCylinder", &CookingRequests::CookCylinder)
                ->Event("CookConvexHull", &CookingRequests::CookConvexHull)
                ->Event("CookTriangleMesh", &CookingRequests::CookTriangleMesh)
                ->Event("CookHeightfield", &CookingRequests::CookHeightfield)
                ->Event("CookCompound", &CookingRequests::CookCompound)
                ->Event("DestroyCookedShape", &CookingRequests::DestroyCookedShape)
                ->Event("IsCookedShapeValid", &CookingRequests::IsCookedShapeValid)
                ->Event("GetCookedShapeAabb", &CookingRequests::GetCookedShapeAabb)
                ->Event("RaycastCookedShape", &CookingRequests::RaycastCookedShape);
            behaviorContext->EBus<WorldNotificationBus>("Box3DWorldNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Handler<WorldNotificationBusBehaviorHandler>();
        }

        SystemConfiguration::Reflect(context);
        RigidBodyConfiguration::Reflect(context);
        CharacterConfiguration::Reflect(context);
        ExplosionConfiguration::Reflect(context);
        WindConfiguration::Reflect(context);
        MaterialConfiguration::Reflect(context);
        ShapeConfiguration::Reflect(context);
        ReflectDiagnostics(context);
        ReflectQueries(context);
        ReflectEvents(context);
        ReflectJoints(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SystemComponent, AZ::Component>()->Version(1)->Field(
                "Configuration", &SystemComponent::m_configuration);
        }
    }

    void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DSystemService"));
    }

    void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DSystemService"));
    }

    void SystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("JobsService"));
    }

    void SystemComponent::Activate()
    {
        constexpr Version expectedVersion{ 0, 1, 0 };
        const Version loadedVersion = GetVersion();
        if (loadedVersion != expectedVersion)
        {
            AZ_Error(
                "Box3D",
                false,
                "Box3D API mismatch: expected %u.%u.%u, loaded %u.%u.%u.",
                expectedVersion.m_major,
                expectedVersion.m_minor,
                expectedVersion.m_revision,
                loadedVersion.m_major,
                loadedVersion.m_minor,
                loadedVersion.m_revision);
            return;
        }

        AZ::JobManagerBus::BroadcastResult(m_jobContext, &AZ::JobManagerEvents::GetGlobalContext);
        m_system = AZStd::make_unique<System>(m_configuration, m_jobContext);
        CookingRequestBus::Handler::BusConnect();
        DiagnosticsRequestBus::Handler::BusConnect();
        MaterialRequestBus::Handler::BusConnect();
        WorldRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void SystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        WorldRequestBus::Handler::BusDisconnect();
        MaterialRequestBus::Handler::BusDisconnect();
        DiagnosticsRequestBus::Handler::BusDisconnect();
        CookingRequestBus::Handler::BusDisconnect();
        {
            AZStd::lock_guard lock(m_replayMutex);
            m_replaySlots.clear();
            m_freeReplaySlots = {};
        }
        m_system.reset();
        m_jobContext = nullptr;
    }

    WorldHandle SystemComponent::GetDefaultWorldHandle() const
    {
        return m_system != nullptr ? m_system->GetDefaultWorldHandle() : WorldHandle{};
    }

    CookedShapeHandle SystemComponent::CookShape(const ShapeConfiguration& configuration)
    {
        return m_system != nullptr ? m_system->CookShape(configuration) : CookedShapeHandle{};
    }

    bool SystemComponent::DestroyCookedShape(CookedShapeHandle cookedShapeHandle)
    {
        return m_system != nullptr && m_system->DestroyCookedShape(cookedShapeHandle);
    }

    bool SystemComponent::IsCookedShapeValid(CookedShapeHandle cookedShapeHandle) const
    {
        return m_system != nullptr && m_system->IsValid(cookedShapeHandle);
    }

    AZ::Aabb SystemComponent::GetCookedShapeAabb(CookedShapeHandle cookedShapeHandle) const
    {
        return m_system != nullptr ? m_system->GetAabb(cookedShapeHandle) : AZ::Aabb::CreateNull();
    }

    CookedRaycastResult SystemComponent::RaycastCookedShape(
        CookedShapeHandle cookedShapeHandle, const AZ::Vector3& start, const AZ::Vector3& direction, float distance) const
    {
        CookedRaycastResult result;
        result.m_found = m_system != nullptr && m_system->Raycast(cookedShapeHandle, start, direction, distance, result.m_hit);
        return result;
    }

    StatisticsSnapshot SystemComponent::GetWorldStatistics(WorldHandle worldHandle, StatisticsFlags flags) const
    {
        WorldStatistics statistics;
        const bool found = m_system != nullptr && m_system->GetWorldStatistics(worldHandle, flags, statistics);
        return StatisticsSnapshot::Create(statistics, found);
    }

    bool SystemComponent::StartRecording(WorldHandle worldHandle, size_t initialCapacityBytes)
    {
        return m_system != nullptr && m_system->StartRecording(worldHandle, initialCapacityBytes);
    }

    RecordingResult SystemComponent::StopRecording(WorldHandle worldHandle)
    {
        RecordingResult result;
        AZStd::vector<AZ::u8> data;
        result.m_stopped = m_system != nullptr && m_system->StopRecording(worldHandle, data);
        if (result.m_stopped)
        {
            result.m_data = RecordingData(AZStd::move(data));
        }
        return result;
    }

    bool SystemComponent::ValidateRecording(const RecordingData& data, AZ::u32 workerCount) const
    {
        return m_system != nullptr && m_system->ValidateRecording(data.GetData(), workerCount);
    }

    ReplayHandle SystemComponent::CreateReplay(const RecordingData& data, AZ::u32 workerCount)
    {
        if (m_system == nullptr)
        {
            return {};
        }
        AZStd::unique_ptr<IReplay> replay = m_system->CreateReplay(data.GetData(), workerCount);
        if (replay == nullptr)
        {
            return {};
        }

        AZStd::lock_guard lock(m_replayMutex);
        AZ::u32 replayIndex = 0;
        if (!m_freeReplaySlots.empty())
        {
            replayIndex = m_freeReplaySlots.back();
            m_freeReplaySlots.pop_back();
        }
        else
        {
            if (m_replaySlots.size() >= AZStd::numeric_limits<AZ::u32>::max())
            {
                return {};
            }
            replayIndex = aznumeric_cast<AZ::u32>(m_replaySlots.size());
            m_replaySlots.emplace_back();
        }
        ReplaySlot& slot = m_replaySlots[replayIndex];
        slot.m_replay = AZStd::move(replay);
        return Internal::MakeRegistryHandle<ReplayHandle>(replayIndex, slot.m_generation);
    }

    bool SystemComponent::DestroyReplay(ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        AZ::u32 replayIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(replayHandle, replayIndex, generation) || replayIndex >= m_replaySlots.size())
        {
            return false;
        }
        ReplaySlot& slot = m_replaySlots[replayIndex];
        if (slot.m_replay == nullptr || slot.m_generation != generation)
        {
            return false;
        }
        slot.m_replay.reset();
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeReplaySlots.push_back(replayIndex);
        }
        return true;
    }

    bool SystemComponent::IsReplayValid(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        return FindReplay(replayHandle) != nullptr;
    }

    ReplayInfoResult SystemComponent::GetReplayInfo(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? ReplayInfoResult{ replay->GetInfo(), true } : ReplayInfoResult{};
    }

    bool SystemComponent::StepReplay(ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr && replay->Step();
    }

    bool SystemComponent::RestartReplay(ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (replay == nullptr)
        {
            return false;
        }
        replay->Restart();
        return true;
    }

    bool SystemComponent::SeekReplay(ReplayHandle replayHandle, AZ::u32 frame)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (replay == nullptr || frame >= replay->GetInfo().m_frameCount)
        {
            return false;
        }
        replay->Seek(frame);
        return true;
    }

    AZ::u32 SystemComponent::GetReplayFrame(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetFrame() : 0;
    }

    bool SystemComponent::IsReplayAtEnd(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr && replay->IsAtEnd();
    }

    bool SystemComponent::HasReplayDiverged(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr && replay->HasDiverged();
    }

    AZ::s32 SystemComponent::GetReplayDivergenceFrame(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetDivergenceFrame() : -1;
    }

    bool SystemComponent::SetReplayWorkerCount(ReplayHandle replayHandle, AZ::u32 workerCount)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (replay == nullptr || workerCount == 0)
        {
            return false;
        }
        replay->SetWorkerCount(workerCount);
        return true;
    }

    bool SystemComponent::SetReplayKeyframePolicy(ReplayHandle replayHandle, size_t budgetBytes, AZ::u32 minimumInterval)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (replay == nullptr || minimumInterval == 0)
        {
            return false;
        }
        replay->SetKeyframePolicy(budgetBytes, minimumInterval);
        return true;
    }

    size_t SystemComponent::GetReplayKeyframeBudget(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetKeyframeBudget() : 0;
    }

    AZ::u32 SystemComponent::GetReplayMinimumKeyframeInterval(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetMinimumKeyframeInterval() : 0;
    }

    AZ::u32 SystemComponent::GetReplayKeyframeInterval(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetKeyframeInterval() : 0;
    }

    size_t SystemComponent::GetReplayKeyframeBytes(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetKeyframeBytes() : 0;
    }

    AZ::u32 SystemComponent::GetReplayBodyCount(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetBodyCount() : 0;
    }

    ReplayBodyResult SystemComponent::GetReplayBody(ReplayHandle replayHandle, AZ::u32 index) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayBodyResult result;
        result.m_found = replay != nullptr && replay->GetBody(index, result.m_body);
        return result;
    }

    AZ::u32 SystemComponent::GetReplayQueryCount(ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay != nullptr ? replay->GetFrameQueryCount() : 0;
    }

    ReplayQueryResult SystemComponent::GetReplayQuery(ReplayHandle replayHandle, AZ::u32 index) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayQueryResult result;
        result.m_found = replay != nullptr && replay->GetFrameQuery(index, result.m_query);
        return result;
    }

    ReplayQueryHitResult SystemComponent::GetReplayQueryHit(ReplayHandle replayHandle, AZ::u32 queryIndex, AZ::u32 hitIndex) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayQueryHitResult result;
        result.m_found = replay != nullptr && replay->GetFrameQueryHit(queryIndex, hitIndex, result.m_hit);
        return result;
    }

    bool SystemComponent::RebuildStaticTree(WorldHandle worldHandle)
    {
        return m_system != nullptr && m_system->RebuildStaticTree(worldHandle);
    }

    IReplay* SystemComponent::FindReplay(ReplayHandle replayHandle)
    {
        return const_cast<IReplay*>(static_cast<const SystemComponent&>(*this).FindReplay(replayHandle));
    }

    const IReplay* SystemComponent::FindReplay(ReplayHandle replayHandle) const
    {
        AZ::u32 replayIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(replayHandle, replayIndex, generation) || replayIndex >= m_replaySlots.size())
        {
            return nullptr;
        }
        const ReplaySlot& slot = m_replaySlots[replayIndex];
        return slot.m_generation == generation ? slot.m_replay.get() : nullptr;
    }

    MaterialHandle SystemComponent::CreateMaterial(const MaterialConfiguration& configuration)
    {
        return m_system != nullptr ? m_system->CreateMaterial(configuration) : MaterialHandle{};
    }

    bool SystemComponent::UpdateMaterial(MaterialHandle materialHandle, const MaterialConfiguration& configuration)
    {
        return m_system != nullptr && m_system->UpdateMaterial(materialHandle, configuration);
    }

    MaterialResult SystemComponent::GetMaterial(MaterialHandle materialHandle) const
    {
        MaterialResult result;
        result.m_found = m_system != nullptr && m_system->GetMaterial(materialHandle, result.m_configuration);
        return result;
    }

    bool SystemComponent::DestroyMaterial(MaterialHandle materialHandle)
    {
        return m_system != nullptr && m_system->DestroyMaterial(materialHandle);
    }

    WorldHandle SystemComponent::CreateWorld(const WorldConfiguration& configuration)
    {
        if (m_system == nullptr)
        {
            return {};
        }
        WorldConfiguration runtimeConfiguration = configuration;
        runtimeConfiguration.m_jobContext = m_jobContext;
        return m_system->CreateWorld(runtimeConfiguration);
    }

    bool SystemComponent::DestroyWorld(WorldHandle worldHandle)
    {
        return m_system != nullptr && m_system->DestroyWorld(worldHandle);
    }

    bool SystemComponent::StepWorld(WorldHandle worldHandle, float fixedTimeStep)
    {
        return m_system != nullptr && m_system->StepWorld(worldHandle, fixedTimeStep);
    }

    ClosestQueryResult SystemComponent::RaycastClosest(WorldHandle worldHandle, const RaycastRequest& request) const
    {
        ClosestQueryResult result;
        result.m_found = m_system != nullptr && m_system->RaycastClosest(worldHandle, request, result.m_hit);
        return result;
    }

    ClosestQueryResultCollection SystemComponent::RaycastClosestBatch(
        WorldHandle worldHandle, const RaycastRequestCollection& requestCollection) const
    {
        const AZStd::span<const RaycastRequest> requests = requestCollection.GetRequests();
        ClosestQueryResultCollection collection;
        collection.m_requiredResultCount = requests.size();
        collection.m_results.resize(requests.size());
        if (m_system == nullptr)
        {
            collection.m_results.clear();
            return collection;
        }

        const BufferResult result = m_system->RaycastClosestBatch(worldHandle, requests, collection.m_results);
        collection.m_results.resize(result.m_count);
        collection.m_requiredResultCount = result.m_requiredCount;
        return collection;
    }

    QueryHitCollection SystemComponent::Raycast(WorldHandle worldHandle, const RaycastRequest& request, AZ::u32 maxHitCount) const
    {
        return m_system != nullptr ? CollectQueryHits(
                                         maxHitCount,
                                         [this, worldHandle, &request](AZStd::span<QueryHit> hits)
                                         {
                                             return m_system->Raycast(worldHandle, request, hits);
                                         })
                                   : QueryHitCollection{};
    }

    QueryHitCollection SystemComponent::OverlapAabb(WorldHandle worldHandle, const AabbOverlapRequest& request, AZ::u32 maxHitCount) const
    {
        return m_system != nullptr ? CollectQueryHits(
                                         maxHitCount,
                                         [this, worldHandle, &request](AZStd::span<QueryHit> hits)
                                         {
                                             return m_system->OverlapAabb(worldHandle, request, hits);
                                         })
                                   : QueryHitCollection{};
    }

    size_t SystemComponent::GetContactPointCount(WorldHandle worldHandle) const
    {
        return m_system != nullptr ? m_system->GetStepEvents(worldHandle).m_contactPoints.size() : 0;
    }

    ContactPoint SystemComponent::GetContactPointAt(WorldHandle worldHandle, size_t index) const
    {
        if (m_system == nullptr)
        {
            return {};
        }
        const AZStd::span<const ContactPoint> points = m_system->GetStepEvents(worldHandle).m_contactPoints;
        return index < points.size() ? points[index] : ContactPoint{};
    }

#define BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Name, Configuration)                                                                          \
    QueryHitCollection SystemComponent::ShapeCast##Name(                                                                                   \
        WorldHandle worldHandle, const Configuration& geometry, const ConvexCastParameters& parameters) const                              \
    {                                                                                                                                      \
        return ShapeCast(m_system.get(), worldHandle, geometry, parameters);                                                               \
    }                                                                                                                                      \
                                                                                                                                           \
    QueryHitCollection SystemComponent::Overlap##Name(                                                                                     \
        WorldHandle worldHandle, const Configuration& geometry, const ConvexOverlapParameters& parameters) const                           \
    {                                                                                                                                      \
        return Overlap(m_system.get(), worldHandle, geometry, parameters);                                                                 \
    }

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Sphere, SphereShapeConfiguration)
    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Capsule, CapsuleShapeConfiguration)
    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Box, BoxShapeConfiguration)
    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Cylinder, CylinderShapeConfiguration)
    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(ConvexHull, ConvexHullShapeConfiguration)

#undef BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS

    void SystemComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_system->StepAutoSimulatedWorlds(deltaTime);
    }

    int SystemComponent::GetTickOrder()
    {
        return AZ::ComponentTickBus::TICK_PHYSICS_SYSTEM;
    }
} // namespace Box3D
