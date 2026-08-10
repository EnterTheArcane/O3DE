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

        void OnBodyMoved(
            const BodyMoveEvent& event) override
        {
            Call(FN_OnBodyMoved, event);
        }

        void OnSensor(
            const SensorEvent& event) override
        {
            Call(FN_OnSensor, event);
        }

        void OnContact(
            const ContactEvent& event) override
        {
            Call(FN_OnContact, event);
        }

        void OnContactHit(
            const ContactHitEvent& event) override
        {
            Call(FN_OnContactHit, event);
        }

        void OnJointThresholdExceeded(
            const JointThresholdEvent& event) override
        {
            Call(FN_OnJointThresholdExceeded, event);
        }
    };

    namespace
    {
        template<typename Query>
        QueryHitCollection CollectQueryHits(
            const AZ::u32 maxHitCount,
            Query&& query)
        {
            QueryHitCollection result;
            result.m_hits.resize(maxHitCount);
            const QueryResult queryResult = query(AZStd::span<QueryHit>(result.m_hits));
            result.m_hits.resize(queryResult.m_hitCount);
            result.m_requiredHitCount = queryResult.m_requiredHitCount;
            return result;
        }

        template<typename Geometry>
        QueryHitCollection ShapeCast(
            const System* system,
            WorldHandle worldHandle,
            const Geometry& geometry,
            const ConvexCastParameters& parameters)
        {
            if (!system)
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
                [system, worldHandle, &request](const AZStd::span<QueryHit> hits)
                {
                    return system->ShapeCast(worldHandle, request, hits);
                });
        }

        template<typename Geometry>
        QueryHitCollection Overlap(
            const System* system,
            WorldHandle worldHandle,
            const Geometry& geometry,
            const ConvexOverlapParameters& parameters)
        {
            if (!system)
            {
                return {};
            }
            OverlapRequest request;
            request.m_geometry = geometry;
            request.m_transform = parameters.m_transform;
            request.m_filter = parameters.m_filter;
            return CollectQueryHits(
                parameters.m_maxHitCount,
                [system, worldHandle, &request](const AZStd::span<QueryHit> hits)
                {
                    return system->Overlap(worldHandle, request, hits);
                });
        }
    } // namespace

    SystemComponent::SystemComponent() = default;
    SystemComponent::~SystemComponent() = default;

    void SystemComponent::Reflect(
        AZ::ReflectContext* context)
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
            serializeContext->Class<SystemComponent, AZ::Component>()->Field("Configuration", &SystemComponent::m_configuration);
        }
    }

    void SystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DSystemService"));
    }

    void SystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DSystemService"));
    }

    void SystemComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("JobsService"));
    }

    void SystemComponent::Activate()
    {
        constexpr Version expectedVersion{0, 1, 0};
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
        if (m_system)
        {
            return m_system->GetDefaultWorldHandle();
        }

        return {};
    }

    CookedShapeHandle SystemComponent::CookShape(
        const ShapeConfiguration& configuration)
    {
        if (m_system)
        {
            return m_system->CookShape(configuration);
        }

        return {};
    }

    bool SystemComponent::DestroyCookedShape(
        const CookedShapeHandle cookedShapeHandle)
    {
        return m_system && m_system->DestroyCookedShape(cookedShapeHandle);
    }

    bool SystemComponent::IsCookedShapeValid(
        const CookedShapeHandle cookedShapeHandle) const
    {
        return m_system && m_system->IsValid(cookedShapeHandle);
    }

    AZ::Aabb SystemComponent::GetCookedShapeAabb(
        const CookedShapeHandle cookedShapeHandle) const
    {
        if (m_system)
        {
            return m_system->GetAabb(cookedShapeHandle);
        }

        return AZ::Aabb::CreateNull();
    }

    CookedRaycastResult SystemComponent::RaycastCookedShape(
        const CookedShapeHandle cookedShapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        const float distance) const
    {
        CookedRaycastResult result;
        result.m_found = m_system && m_system->Raycast(cookedShapeHandle, start, direction, distance, result.m_hit);
        return result;
    }

    StatisticsSnapshot SystemComponent::GetWorldStatistics(
        const WorldHandle worldHandle,
        const StatisticsFlags flags) const
    {
        WorldStatistics statistics;
        const bool found = m_system && m_system->GetWorldStatistics(worldHandle, flags, statistics);
        return StatisticsSnapshot::Create(statistics, found);
    }

    bool SystemComponent::StartRecording(
        const WorldHandle worldHandle,
        const size_t initialCapacityBytes)
    {
        return m_system && m_system->StartRecording(worldHandle, initialCapacityBytes);
    }

    RecordingResult SystemComponent::StopRecording(
        const WorldHandle worldHandle)
    {
        RecordingResult result;
        AZStd::vector<AZ::u8> data;
        result.m_stopped = m_system && m_system->StopRecording(worldHandle, data);
        if (result.m_stopped)
        {
            result.m_data = RecordingData(AZStd::move(data));
        }
        return result;
    }

    bool SystemComponent::ValidateRecording(
        const RecordingData& data,
        const AZ::u32 workerCount) const
    {
        return m_system && m_system->ValidateRecording(data.GetData(), workerCount);
    }

    ReplayHandle SystemComponent::CreateReplay(
        const RecordingData& data,
        const AZ::u32 workerCount)
    {
        if (!m_system)
        {
            return {};
        }
        AZStd::unique_ptr<IReplay> replay = m_system->CreateReplay(data.GetData(), workerCount);
        if (!replay)
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

    bool SystemComponent::DestroyReplay(
        const ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        AZ::u32 replayIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(replayHandle, replayIndex, generation) || replayIndex >= m_replaySlots.size())
        {
            return false;
        }
        ReplaySlot& slot = m_replaySlots[replayIndex];
        if (!slot.m_replay || slot.m_generation != generation)
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

    bool SystemComponent::IsReplayValid(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        return FindReplay(replayHandle);
    }

    ReplayInfoResult SystemComponent::GetReplayInfo(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return {replay->GetInfo(), true};
        }

        return {};
    }

    bool SystemComponent::StepReplay(
        const ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        return replay && replay->Step();
    }

    bool SystemComponent::RestartReplay(
        const ReplayHandle replayHandle)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (!replay)
        {
            return false;
        }
        replay->Restart();
        return true;
    }

    bool SystemComponent::SeekReplay(
        const ReplayHandle replayHandle,
        const AZ::u32 frame)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (!replay || frame >= replay->GetInfo().m_frameCount)
        {
            return false;
        }
        replay->Seek(frame);
        return true;
    }

    AZ::u32 SystemComponent::GetReplayFrame(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetFrame();
        }

        return 0;
    }

    bool SystemComponent::IsReplayAtEnd(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay && replay->IsAtEnd();
    }

    bool SystemComponent::HasReplayDiverged(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        return replay && replay->HasDiverged();
    }

    AZ::s32 SystemComponent::GetReplayDivergenceFrame(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetDivergenceFrame();
        }

        return -1;
    }

    bool SystemComponent::SetReplayWorkerCount(
        const ReplayHandle replayHandle,
        const AZ::u32 workerCount)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (!replay || workerCount == 0)
        {
            return false;
        }
        replay->SetWorkerCount(workerCount);
        return true;
    }

    bool SystemComponent::SetReplayKeyframePolicy(
        const ReplayHandle replayHandle,
        const size_t budgetBytes,
        const AZ::u32 minimumInterval)
    {
        AZStd::lock_guard lock(m_replayMutex);
        IReplay* replay = FindReplay(replayHandle);
        if (!replay || minimumInterval == 0)
        {
            return false;
        }
        replay->SetKeyframePolicy(budgetBytes, minimumInterval);
        return true;
    }

    size_t SystemComponent::GetReplayKeyframeBudget(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetKeyframeBudget();
        }

        return 0;
    }

    AZ::u32 SystemComponent::GetReplayMinimumKeyframeInterval(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetMinimumKeyframeInterval();
        }

        return 0;
    }

    AZ::u32 SystemComponent::GetReplayKeyframeInterval(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetKeyframeInterval();
        }

        return 0;
    }

    size_t SystemComponent::GetReplayKeyframeBytes(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetKeyframeBytes();
        }

        return 0;
    }

    AZ::u32 SystemComponent::GetReplayBodyCount(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetBodyCount();
        }

        return 0;
    }

    ReplayBodyResult SystemComponent::GetReplayBody(
        const ReplayHandle replayHandle,
        const AZ::u32 index) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayBodyResult result;
        result.m_found = replay && replay->GetBody(index, result.m_body);
        return result;
    }

    AZ::u32 SystemComponent::GetReplayQueryCount(
        const ReplayHandle replayHandle) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        if (replay)
        {
            return replay->GetFrameQueryCount();
        }

        return 0;
    }

    ReplayQueryResult SystemComponent::GetReplayQuery(
        const ReplayHandle replayHandle,
        const AZ::u32 index) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayQueryResult result;
        result.m_found = replay && replay->GetFrameQuery(index, result.m_query);
        return result;
    }

    ReplayQueryHitResult SystemComponent::GetReplayQueryHit(
        const ReplayHandle replayHandle,
        const AZ::u32 queryIndex,
        const AZ::u32 hitIndex) const
    {
        AZStd::lock_guard lock(m_replayMutex);
        const IReplay* replay = FindReplay(replayHandle);
        ReplayQueryHitResult result;
        result.m_found = replay && replay->GetFrameQueryHit(queryIndex, hitIndex, result.m_hit);
        return result;
    }

    bool SystemComponent::RebuildStaticTree(
        const WorldHandle worldHandle)
    {
        return m_system && m_system->RebuildStaticTree(worldHandle);
    }

    IReplay* SystemComponent::FindReplay(
        const ReplayHandle replayHandle)
    {
        return const_cast<IReplay*>(static_cast<const SystemComponent&>(*this).FindReplay(replayHandle));
    }

    const IReplay* SystemComponent::FindReplay(
        const ReplayHandle replayHandle) const
    {
        AZ::u32 replayIndex = 0;
        AZ::u32 generation = 0;
        if (!Internal::DecodeRegistryHandle(replayHandle, replayIndex, generation) || replayIndex >= m_replaySlots.size())
        {
            return nullptr;
        }
        const ReplaySlot& slot = m_replaySlots[replayIndex];
        if (slot.m_generation == generation)
        {
            return slot.m_replay.get();
        }

        return nullptr;
    }

    MaterialHandle SystemComponent::CreateMaterial(
        const MaterialConfiguration& configuration)
    {
        if (m_system)
        {
            return m_system->CreateMaterial(configuration);
        }

        return {};
    }

    bool SystemComponent::UpdateMaterial(
        const MaterialHandle materialHandle,
        const MaterialConfiguration& configuration)
    {
        return m_system && m_system->UpdateMaterial(materialHandle, configuration);
    }

    MaterialResult SystemComponent::GetMaterial(
        const MaterialHandle materialHandle) const
    {
        MaterialResult result;
        result.m_found = m_system && m_system->GetMaterial(materialHandle, result.m_configuration);
        return result;
    }

    bool SystemComponent::DestroyMaterial(
        const MaterialHandle materialHandle)
    {
        return m_system && m_system->DestroyMaterial(materialHandle);
    }

    WorldHandle SystemComponent::CreateWorld(
        const WorldConfiguration& configuration)
    {
        if (!m_system)
        {
            return {};
        }
        WorldConfiguration runtimeConfiguration = configuration;
        runtimeConfiguration.m_jobContext = m_jobContext;
        return m_system->CreateWorld(runtimeConfiguration);
    }

    bool SystemComponent::DestroyWorld(
        const WorldHandle worldHandle)
    {
        return m_system && m_system->DestroyWorld(worldHandle);
    }

    bool SystemComponent::StepWorld(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        return m_system && m_system->StepWorld(worldHandle, fixedTimeStep);
    }

    ClosestQueryResult SystemComponent::RaycastClosest(
        const WorldHandle worldHandle,
        const RaycastRequest& request) const
    {
        ClosestQueryResult result;
        result.m_found = m_system && m_system->RaycastClosest(worldHandle, request, result.m_hit);
        return result;
    }

    ClosestQueryResultCollection SystemComponent::RaycastClosestBatch(
        const WorldHandle worldHandle,
        const RaycastRequestCollection& requestCollection) const
    {
        const AZStd::span<const RaycastRequest> requests = requestCollection.GetRequests();
        ClosestQueryResultCollection collection;
        collection.m_requiredResultCount = requests.size();
        collection.m_results.resize(requests.size());
        if (!m_system)
        {
            collection.m_results.clear();
            return collection;
        }

        const BufferResult result = m_system->RaycastClosestBatch(worldHandle, requests, collection.m_results);
        collection.m_results.resize(result.m_count);
        collection.m_requiredResultCount = result.m_requiredCount;
        return collection;
    }

    QueryHitCollection SystemComponent::Raycast(
        WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZ::u32 maxHitCount) const
    {
        if (!m_system)
        {
            return {};
        }

        return CollectQueryHits(
            maxHitCount,
            [this, worldHandle, &request](const AZStd::span<QueryHit> hits)
            {
                return m_system->Raycast(worldHandle, request, hits);
            });
    }

    QueryHitCollection SystemComponent::OverlapAabb(
        WorldHandle worldHandle,
        const AabbOverlapRequest& request,
        const AZ::u32 maxHitCount) const
    {
        if (!m_system)
        {
            return {};
        }

        return CollectQueryHits(
            maxHitCount,
            [this, worldHandle, &request](const AZStd::span<QueryHit> hits)
            {
                return m_system->OverlapAabb(worldHandle, request, hits);
            });
    }

    size_t SystemComponent::GetContactPointCount(
        const WorldHandle worldHandle) const
    {
        if (m_system)
        {
            return m_system->GetStepEvents(worldHandle).m_contactPoints.size();
        }

        return 0;
    }

    ContactPoint SystemComponent::GetContactPointAt(
        const WorldHandle worldHandle,
        const size_t index) const
    {
        if (!m_system)
        {
            return {};
        }
        const AZStd::span<const ContactPoint> points = m_system->GetStepEvents(worldHandle).m_contactPoints;
        if (index < points.size())
        {
            return points[index];
        }

        return {};
    }

#define BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Name, Configuration) \
    QueryHitCollection SystemComponent::ShapeCast##Name( \
        WorldHandle worldHandle, const Configuration& geometry, const ConvexCastParameters& parameters) const \
    { \
        return ShapeCast(m_system.get(), worldHandle, geometry, parameters); \
    } \
    QueryHitCollection SystemComponent::Overlap##Name( \
        WorldHandle worldHandle, const Configuration& geometry, const ConvexOverlapParameters& parameters) const \
    { \
        return Overlap(m_system.get(), worldHandle, geometry, parameters); \
    }

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Sphere, SphereShapeConfiguration)

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Capsule, CapsuleShapeConfiguration)

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Box, BoxShapeConfiguration)

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(Cylinder, CylinderShapeConfiguration)

    BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS(ConvexHull, ConvexHullShapeConfiguration)

#undef BOX3D_IMPLEMENT_CONVEX_QUERY_METHODS

    void SystemComponent::OnTick(
        const float deltaTime,
        [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_system->StepAutoSimulatedWorlds(deltaTime);
    }

    int SystemComponent::GetTickOrder()
    {
        return AZ::ComponentTickBus::TICK_PHYSICS_SYSTEM;
    }
} // namespace Box3D
