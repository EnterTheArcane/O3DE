/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/ConfigurationSerializer.h>
#include <Jolt/Cooking.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/Shape.h>
#include <Jolt/SceneAssetHandler.h>
#include <Jolt/SkeletonAsset.h>
#include <Jolt/SkeletonAssetHandler.h>

#include <Jolt/BodyBus.h>
#include <Jolt/SystemInternal.h>
#include <Jolt/VirtualCharacterBus.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Jobs/JobManagerBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string.h>

namespace Jolt
{
    class WorldNotificationBusBehaviorHandler final
        : public WorldNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            WorldNotificationBusBehaviorHandler,
            "{9A8745F3-EB20-4A7A-B681-BD2415D48F4B}",
            AZ::SystemAllocator,
            OnBodyActivation,
            OnBodyMoved,
            OnContact,
            OnVirtualCharacterMoved);

        void OnBodyActivation(
            const ActivationEvent& event) override
        {
            Call(FN_OnBodyActivation, event);
        }

        void OnBodyMoved(
            const BodyMoveEvent& event) override
        {
            Call(FN_OnBodyMoved, event);
        }

        void OnContact(
            const ContactEvent& event,
            const ContactPointView& points) override
        {
            Call(FN_OnContact, event, points);
        }

        void OnVirtualCharacterMoved(
            const VirtualCharacterMoveEvent& event) override
        {
            Call(FN_OnVirtualCharacterMoved, event);
        }
    };

    namespace
    {
        [[nodiscard]]
        ShapeQueryFaceBuffers PrepareShapeQueryFaceBuffers(
            const FaceCollectionMode faceCollectionMode,
            const AZ::u32 hitCapacity,
            AZStd::vector<WorldPosition>& queryFaceVertices,
            AZStd::vector<WorldPosition>& targetFaceVertices)
        {
            if (faceCollectionMode != FaceCollectionMode::Collect)
            {
                return {};
            }

            const size_t faceVertexCapacity =
                static_cast<size_t>(hitCapacity) * MaximumSupportingFaceVertexCount;
            queryFaceVertices.resize(faceVertexCapacity);
            targetFaceVertices.resize(faceVertexCapacity);
            return {
                .m_queryVertices = queryFaceVertices,
                .m_targetVertices = targetFaceVertices,
            };
        }

        void ResizeShapeQueryFaceBuffers(
            const AZ::u32 hitCount,
            AZStd::vector<WorldPosition>& queryFaceVertices,
            AZStd::vector<WorldPosition>& targetFaceVertices)
        {
            if (queryFaceVertices.empty() && targetFaceVertices.empty())
            {
                return;
            }

            const size_t faceVertexCount =
                static_cast<size_t>(hitCount) * MaximumSupportingFaceVertexCount;
            queryFaceVertices.resize(faceVertexCount);
            targetFaceVertices.resize(faceVertexCount);
        }

        template<typename... HandleTypes>
        void ReflectHandles(
            AZ::SerializeContext& serializeContext)
        {
            (serializeContext.Class<HandleTypes>(), ...);
        }

        template<typename HandleType>
        void ReflectHandle(
            AZ::BehaviorContext& behaviorContext,
            const char* reflectedName,
            const char* scriptName)
        {
            behaviorContext.Class<HandleType>(reflectedName)
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, scriptName)
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, scriptName)
                ->template Constructor<>()
                ->Method("IsValid", &HandleType::IsValid);
        }

        template<typename Element, typename Query>
        AZStd::vector<Element> CopyQueryResults(
            const Query& query)
        {
            const QueryResult required = query(AZStd::span<Element>{});
            AZStd::vector<Element> result(required.m_requiredHitCount);
            const QueryResult copied = query(result);
            if (!copied.IsComplete())
            {
                result.clear();
            }
            return result;
        }
    } // namespace

    SystemComponent::SystemComponent() = default;

    SystemComponent::~SystemComponent() = default;

    void SystemComponent::Reflect(
        AZ::ReflectContext* context)
    {
        ReflectConfigurationSerializers(context);

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DeterminismCertification, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DeterminismCertification, CrossPlatform);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DeterminismCertification, SameBinary);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, Precision, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, Precision, Double);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, Precision, Single);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Avx);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Avx2);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Avx512);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Neon);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Rvv);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Scalar);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Sse2);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Sse41);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, Sse42);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SimdLevel, WasmSimd);

            ReflectHandle<BodyHandle>(*behaviorContext, "JoltBodyHandle", "BodyHandle");
            ReflectHandle<BodySnapshotHandle>(*behaviorContext, "JoltBodySnapshotHandle", "BodySnapshotHandle");
            ReflectHandle<CharacterHandle>(*behaviorContext, "JoltCharacterHandle", "CharacterHandle");
            ReflectHandle<ConstraintHandle>(*behaviorContext, "JoltConstraintHandle", "ConstraintHandle");
            ReflectHandle<CookedShapeHandle>(*behaviorContext, "JoltCookedShapeHandle", "CookedShapeHandle");
            ReflectHandle<GroupFilterHandle>(*behaviorContext, "JoltGroupFilterHandle", "GroupFilterHandle");
            ReflectHandle<HairDefinitionHandle>(
                *behaviorContext,
                "JoltHairDefinitionHandle",
                "HairDefinitionHandle");
            ReflectHandle<HairHandle>(*behaviorContext, "JoltHairHandle", "HairHandle");
            ReflectHandle<MaterialHandle>(*behaviorContext, "JoltMaterialHandle", "MaterialHandle");
            ReflectHandle<PathHandle>(*behaviorContext, "JoltPathHandle", "PathHandle");
            ReflectHandle<RagdollDefinitionHandle>(
                *behaviorContext,
                "JoltRagdollDefinitionHandle",
                "RagdollDefinitionHandle");
            ReflectHandle<RagdollHandle>(*behaviorContext, "JoltRagdollHandle", "RagdollHandle");
            ReflectHandle<SceneDefinitionHandle>(
                *behaviorContext,
                "JoltSceneDefinitionHandle",
                "SceneDefinitionHandle");
            ReflectHandle<SceneInstanceHandle>(
                *behaviorContext,
                "JoltSceneInstanceHandle",
                "SceneInstanceHandle");
            ReflectHandle<ShapeHandle>(*behaviorContext, "JoltShapeHandle", "ShapeHandle");
            ReflectHandle<SkeletalAnimationHandle>(
                *behaviorContext,
                "JoltSkeletalAnimationHandle",
                "SkeletalAnimationHandle");
            ReflectHandle<SkeletonDefinitionHandle>(
                *behaviorContext,
                "JoltSkeletonDefinitionHandle",
                "SkeletonDefinitionHandle");
            ReflectHandle<SkeletonMapperHandle>(
                *behaviorContext,
                "JoltSkeletonMapperHandle",
                "SkeletonMapperHandle");
            ReflectHandle<SkeletonPoseHandle>(*behaviorContext, "JoltSkeletonPoseHandle", "SkeletonPoseHandle");
            ReflectHandle<StateSnapshotHandle>(*behaviorContext, "JoltStateSnapshotHandle", "StateSnapshotHandle");
            ReflectHandle<SoftBodyDefinitionHandle>(
                *behaviorContext,
                "JoltSoftBodyDefinitionHandle",
                "SoftBodyDefinitionHandle");
            ReflectHandle<VehicleHandle>(*behaviorContext, "JoltVehicleHandle", "VehicleHandle");
            ReflectHandle<VirtualCharacterHandle>(
                *behaviorContext,
                "JoltVirtualCharacterHandle",
                "VirtualCharacterHandle");
            ReflectHandle<WorldHandle>(*behaviorContext, "JoltWorldHandle", "WorldHandle");

            behaviorContext->Class<WorldPosition>("WorldPosition")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("x", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldPosition::m_x))
                ->Property("y", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldPosition::m_y))
                ->Property("z", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldPosition::m_z));

            behaviorContext->Class<Version>("JoltVersion")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "Version")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "Version")
                ->Constructor<>()
                ->Property("major", JOLT_BEHAVIOR_VALUE_PROPERTY(&Version::m_major))
                ->Property("minor", JOLT_BEHAVIOR_VALUE_PROPERTY(&Version::m_minor))
                ->Property("patch", JOLT_BEHAVIOR_VALUE_PROPERTY(&Version::m_patch));

            behaviorContext->Class<RuntimeInfo>("JoltRuntimeInfo")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "RuntimeInfo")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "RuntimeInfo")
                ->Constructor<>()
                ->Property("version", JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_version))
                ->Property(
                    "buildFingerprint",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_buildFingerprint))
                ->Property(
                    "hairDeterminism",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_hairDeterminism))
                ->Property(
                    "physicsDeterminism",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_physicsDeterminism))
                ->Property("precision", JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_precision))
                ->Property("simdLevel", JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_simdLevel))
                ->Property(
                    "broadPhaseStatistics",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_broadPhaseStatistics))
                ->Property(
                    "detailedProfiling",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_detailedProfiling))
                ->Property(
                    "narrowPhaseStatistics",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_narrowPhaseStatistics))
                ->Property(
                    "simulationStatistics",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&RuntimeInfo::m_simulationStatistics))
                ->Method(
                    "GetConfiguration",
                    [](const RuntimeInfo& runtimeInfo)
                    {
                        return AZStd::string(runtimeInfo.m_configuration);
                    })
                ->Method(
                    "GetPatchHash",
                    [](const RuntimeInfo& runtimeInfo)
                    {
                        return AZStd::string(runtimeInfo.m_patchHash);
                    })
                ->Method(
                    "GetPatchRevision",
                    [](const RuntimeInfo& runtimeInfo)
                    {
                        return AZStd::string(runtimeInfo.m_patchRevision);
                    })
                ->Method(
                    "GetSourceRevision",
                    [](const RuntimeInfo& runtimeInfo)
                    {
                        return AZStd::string(runtimeInfo.m_sourceRevision);
                    });

            behaviorContext->Class<WorldTransform>("WorldTransform")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldTransform::m_position))
                ->Property("rotation", JOLT_BEHAVIOR_VALUE_PROPERTY(&WorldTransform::m_rotation));
        }

        BodyId::Reflect(context);
        SystemConfiguration::Reflect(context);
        SceneConfiguration::Reflect(context);
        SceneAsset::Reflect(context);
        SkeletalAnimationConfiguration::Reflect(context);
        SkeletonAsset::Reflect(context);
        WorldTransform::Reflect(context);
        ReflectDebugDraw(context);
        ReflectDiagnostics(context);
        ReflectEvents(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<StateSnapshotHandle>>();
            serializeContext->RegisterGenericType<AZStd::vector<SkeletalAnimationKeyframe>>();
            serializeContext->RegisterGenericType<AZStd::vector<SkeletonJoint>>();
            serializeContext->RegisterGenericType<AZStd::vector<SkeletonMapperLockedTranslation>>();
            serializeContext->RegisterGenericType<AZStd::vector<SkeletonMapperMappingState>>();
            serializeContext->RegisterGenericType<AZStd::vector<SkeletonMapperUnmappedJoint>>();

            ReflectHandles<
                BodyHandle,
                BodySnapshotHandle,
                CharacterHandle,
                ConstraintHandle,
                CookedShapeHandle,
                GroupFilterHandle,
                HairDefinitionHandle,
                HairHandle,
                MaterialHandle,
                PathHandle,
                RagdollDefinitionHandle,
                RagdollHandle,
                SceneDefinitionHandle,
                SceneInstanceHandle,
                ShapeHandle,
                SkeletalAnimationHandle,
                SkeletonDefinitionHandle,
                SkeletonMapperHandle,
                SkeletonPoseHandle,
                SoftBodyDefinitionHandle,
                StateSnapshotHandle,
                VehicleHandle,
                VirtualCharacterHandle,
                WorldHandle>(*serializeContext);

            serializeContext
                ->Class<SystemComponent, AZ::Component>()
                ->Field("Configuration", &SystemComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SystemComponent>("Jolt", "Runs the standalone Jolt physics system.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Physics")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SystemComponent::m_configuration,
                        "Configuration",
                        "System and default-world settings.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<SkeletonRequestBus>("JoltSkeletonRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("CreateSkeletonDefinition", &ISkeletonRequests::CreateSkeletonDefinition)
                ->Event("DestroySkeletonDefinition", &ISkeletonRequests::DestroySkeletonDefinition)
                ->Event("IsSkeletonDefinitionValid", &ISkeletonRequests::IsSkeletonDefinitionValid)
                ->Event("CopySkeletonJoints", &ISkeletonRequests::CopySkeletonJoints)
                ->Event("FindSkeletonJoint", &ISkeletonRequests::FindSkeletonJoint)
                ->Event("CreateSkeletalAnimation", &ISkeletonRequests::CreateSkeletalAnimation)
                ->Event("UpdateSkeletalAnimation", &ISkeletonRequests::UpdateSkeletalAnimation)
                ->Event("DestroySkeletalAnimation", &ISkeletonRequests::DestroySkeletalAnimation)
                ->Event("IsSkeletalAnimationValid", &ISkeletonRequests::IsSkeletalAnimationValid)
                ->Event("GetSkeletalAnimationState", &ISkeletonRequests::GetSkeletalAnimationState)
                ->Event("GetSkeletalAnimatedJointName", &ISkeletonRequests::GetSkeletalAnimatedJointName)
                ->Event(
                    "CopySkeletalAnimationKeyframes",
                    &ISkeletonRequests::CopySkeletalAnimationKeyframes)
                ->Event("SetSkeletalAnimationLooping", &ISkeletonRequests::SetSkeletalAnimationLooping)
                ->Event("ScaleSkeletalAnimation", &ISkeletonRequests::ScaleSkeletalAnimation)
                ->Event("CreateSkeletonPose", &ISkeletonRequests::CreateSkeletonPose)
                ->Event("DestroySkeletonPose", &ISkeletonRequests::DestroySkeletonPose)
                ->Event("IsSkeletonPoseValid", &ISkeletonRequests::IsSkeletonPoseValid)
                ->Event("GetSkeletonPoseState", &ISkeletonRequests::GetSkeletonPoseState)
                ->Event("SetSkeletonPoseRootOffset", &ISkeletonRequests::SetSkeletonPoseRootOffset)
                ->Event("SetSkeletonPoseLocalTransforms", &ISkeletonRequests::SetSkeletonPoseLocalTransforms)
                ->Event("SetSkeletonPoseModelTransforms", &ISkeletonRequests::SetSkeletonPoseModelTransforms)
                ->Event("CopySkeletonPoseLocalTransforms", &ISkeletonRequests::CopySkeletonPoseLocalTransforms)
                ->Event("CopySkeletonPoseModelTransforms", &ISkeletonRequests::CopySkeletonPoseModelTransforms)
                ->Event("SampleSkeletalAnimation", &ISkeletonRequests::SampleSkeletalAnimation)
                ->Event("CreateSkeletonMapper", &ISkeletonRequests::CreateSkeletonMapper)
                ->Event("DestroySkeletonMapper", &ISkeletonRequests::DestroySkeletonMapper)
                ->Event("IsSkeletonMapperValid", &ISkeletonRequests::IsSkeletonMapperValid)
                ->Event("GetSkeletonMapperState", &ISkeletonRequests::GetSkeletonMapperState)
                ->Event("CopySkeletonMapperMappings", &ISkeletonRequests::CopySkeletonMapperMappings)
                ->Event("GetSkeletonMapperChainState", &ISkeletonRequests::GetSkeletonMapperChainState)
                ->Event("CopySkeletonMapperSourceChain", &ISkeletonRequests::CopySkeletonMapperSourceChain)
                ->Event("CopySkeletonMapperTargetChain", &ISkeletonRequests::CopySkeletonMapperTargetChain)
                ->Event(
                    "CopySkeletonMapperUnmappedJoints",
                    &ISkeletonRequests::CopySkeletonMapperUnmappedJoints)
                ->Event(
                    "CopySkeletonMapperLockedTranslations",
                    &ISkeletonRequests::CopySkeletonMapperLockedTranslations)
                ->Event("FindMappedSkeletonJoint", &ISkeletonRequests::FindMappedSkeletonJoint)
                ->Event(
                    "IsSkeletonJointTranslationLocked",
                    &ISkeletonRequests::IsSkeletonJointTranslationLocked)
                ->Event("MapSkeletonPose", &ISkeletonRequests::MapSkeletonPose)
                ->Event("MapSkeletonPoseReverse", &ISkeletonRequests::MapSkeletonPoseReverse);

            behaviorContext->EBus<WorldNotificationBus>("JoltWorldNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Handler<WorldNotificationBusBehaviorHandler>();
        }

        ShapeStats::Reflect(context);
        CookedShapeArchive::Reflect(context);
        SoftBodyDefinitionArchive::Reflect(context);
        ReflectQueries(context);
        ReflectWorldQueries(context);
    }

    void SystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltService"));
    }

    void SystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltService"));
    }

    void SystemComponent::Activate()
    {
        AZ::JobContext* jobContext = nullptr;
        AZ::JobManagerBus::BroadcastResult(jobContext, &AZ::JobManagerEvents::GetGlobalContext);
        m_system = AZStd::make_unique<System>(m_configuration, jobContext);
        if (!*m_system)
        {
            m_system.reset();
            return;
        }

        m_defaultWorldLastEventSequence = 0;
        m_worldEventStates.clear();

        AZ::TickBus::Handler::BusConnect();
        SkeletonRequestBus::Handler::BusConnect();
        WorldQueryRequestBus::Handler::BusConnect();
        if (AZ::Data::AssetManager::IsReady())
        {
            m_sceneAssetHandler = AZStd::make_unique<SceneAssetHandler>();
            m_skeletonAssetHandler = AZStd::make_unique<SkeletonAssetHandler>();
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::EnableCatalogForAsset,
                SceneAssetTypeId);
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::EnableCatalogForAsset,
                SkeletonAssetTypeId);
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::AddExtension,
                "jolt");
        }
    }

    void SystemComponent::Deactivate()
    {
        WorldQueryRequestBus::Handler::BusDisconnect();
        SkeletonRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        m_worldEventStates.clear();
        m_defaultWorldLastEventSequence = 0;
        m_skeletonAssetHandler.reset();
        m_sceneAssetHandler.reset();
        m_system.reset();
    }

    void SystemComponent::OnTick(
        const float deltaTime,
        [[maybe_unused]] const AZ::ScriptTimePoint time)
    {
        if (!m_system)
        {
            return;
        }

        m_system->StepAutoSimulatedWorlds(deltaTime);
        DispatchWorldEvents(
            m_system->GetDefaultWorldHandle(),
            m_defaultWorldLastEventSequence);
        for (WorldEventState& state : m_worldEventStates)
        {
            DispatchWorldEvents(state.m_worldHandle, state.m_lastSequence);
        }
    }

    void SystemComponent::DispatchWorldEvents(
        const WorldHandle worldHandle,
        AZ::u64& lastSequence)
    {
        if (!m_system || !worldHandle)
        {
            return;
        }

        const EventView events = m_system->GetEvents(worldHandle);
        if (events.GetSequence() == 0 || events.GetSequence() == lastSequence)
        {
            return;
        }
        lastSequence = events.GetSequence();

        for (const BodyMoveEvent& event : events.GetBodyMoves())
        {
            if (event.m_entityId.IsValid())
            {
                BodyNotificationBus::Event(
                    event.m_entityId,
                    &IBodyNotifications::OnBodyMoved,
                    event);
            }
        }

        for (const VirtualCharacterMoveEvent& event : events.GetVirtualCharacterMoves())
        {
            if (event.m_entityId.IsValid())
            {
                VirtualCharacterNotificationBus::Event(
                    event.m_entityId,
                    &IVirtualCharacterNotifications::OnCharacterMoved,
                    event);
            }
        }

        if (!WorldNotificationBus::HasHandlers(worldHandle))
        {
            return;
        }

        for (const ActivationEvent& event : events.GetActivations())
        {
            WorldNotificationBus::Event(
                worldHandle,
                &IWorldNotifications::OnBodyActivation,
                event);
        }
        for (const BodyMoveEvent& event : events.GetBodyMoves())
        {
            WorldNotificationBus::Event(
                worldHandle,
                &IWorldNotifications::OnBodyMoved,
                event);
        }
        for (const ContactEvent& event : events.GetContacts())
        {
            const ContactPointView points(events.GetContactPoints(event));
            WorldNotificationBus::Event(
                worldHandle,
                &IWorldNotifications::OnContact,
                event,
                points);
        }
        for (const VirtualCharacterMoveEvent& event : events.GetVirtualCharacterMoves())
        {
            WorldNotificationBus::Event(
                worldHandle,
                &IWorldNotifications::OnVirtualCharacterMoved,
                event);
        }
    }

    SkeletonDefinitionHandle SystemComponent::CreateSkeletonDefinition(
        const SkeletonDefinitionConfiguration& configuration)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CreateSkeletonDefinition(configuration);
    }

    bool SystemComponent::DestroySkeletonDefinition(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        return m_system && m_system->DestroySkeletonDefinition(skeletonHandle);
    }

    bool SystemComponent::IsSkeletonDefinitionValid(
        const SkeletonDefinitionHandle skeletonHandle) const
    {
        return m_system && m_system->IsValid(skeletonHandle);
    }

    AZStd::vector<SkeletonJoint> SystemComponent::CopySkeletonJoints(
        const SkeletonDefinitionHandle skeletonHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<SkeletonJoint>(
            [this, skeletonHandle](const AZStd::span<SkeletonJoint> joints)
            {
                return m_system->GetSkeletonJoints(skeletonHandle, joints);
            });
    }

    AZ::s32 SystemComponent::FindSkeletonJoint(
        const SkeletonDefinitionHandle skeletonHandle,
        const AZ::Name jointName) const
    {
        AZ::u32 jointIndex = 0;
        if (!m_system || !m_system->FindSkeletonJoint(skeletonHandle, jointName, jointIndex))
        {
            return -1;
        }

        return aznumeric_cast<AZ::s32>(jointIndex);
    }

    SkeletalAnimationHandle SystemComponent::CreateSkeletalAnimation(
        const SkeletalAnimationConfiguration& configuration)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CreateSkeletalAnimation(configuration);
    }

    bool SystemComponent::UpdateSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const SkeletalAnimationConfiguration& configuration)
    {
        return m_system && m_system->UpdateSkeletalAnimation(animationHandle, configuration);
    }

    bool SystemComponent::DestroySkeletalAnimation(
        const SkeletalAnimationHandle animationHandle)
    {
        return m_system && m_system->DestroySkeletalAnimation(animationHandle);
    }

    bool SystemComponent::IsSkeletalAnimationValid(
        const SkeletalAnimationHandle animationHandle) const
    {
        return m_system && m_system->IsValid(animationHandle);
    }

    bool SystemComponent::GetSkeletalAnimationState(
        const SkeletalAnimationHandle animationHandle,
        SkeletalAnimationState& state) const
    {
        state = {};
        return m_system && m_system->GetSkeletalAnimationState(animationHandle, state);
    }

    AZ::Name SystemComponent::GetSkeletalAnimatedJointName(
        const SkeletalAnimationHandle animationHandle,
        const AZ::u32 jointIndex) const
    {
        AZ::Name jointName;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetSkeletalAnimatedJointName(animationHandle, jointIndex, jointName);
        }
        return jointName;
    }

    AZStd::vector<SkeletalAnimationKeyframe> SystemComponent::CopySkeletalAnimationKeyframes(
        const SkeletalAnimationHandle animationHandle,
        const AZ::u32 jointIndex) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<SkeletalAnimationKeyframe>(
            [this, animationHandle, jointIndex](const AZStd::span<SkeletalAnimationKeyframe> keyframes)
            {
                return m_system->GetSkeletalAnimationKeyframes(animationHandle, jointIndex, keyframes);
            });
    }

    bool SystemComponent::SetSkeletalAnimationLooping(
        const SkeletalAnimationHandle animationHandle,
        const bool isLooping)
    {
        return m_system && m_system->SetSkeletalAnimationLooping(animationHandle, isLooping);
    }

    bool SystemComponent::ScaleSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const float scale)
    {
        return m_system && m_system->ScaleSkeletalAnimation(animationHandle, scale);
    }

    SkeletonPoseHandle SystemComponent::CreateSkeletonPose(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CreateSkeletonPose(skeletonHandle);
    }

    bool SystemComponent::DestroySkeletonPose(
        const SkeletonPoseHandle poseHandle)
    {
        return m_system && m_system->DestroySkeletonPose(poseHandle);
    }

    bool SystemComponent::IsSkeletonPoseValid(
        const SkeletonPoseHandle poseHandle) const
    {
        return m_system && m_system->IsValid(poseHandle);
    }

    bool SystemComponent::GetSkeletonPoseState(
        const SkeletonPoseHandle poseHandle,
        SkeletonPoseState& state) const
    {
        state = {};
        return m_system && m_system->GetSkeletonPoseState(poseHandle, state);
    }

    bool SystemComponent::SetSkeletonPoseRootOffset(
        const SkeletonPoseHandle poseHandle,
        const WorldPosition& rootOffset)
    {
        return m_system && m_system->SetSkeletonPoseRootOffset(poseHandle, rootOffset);
    }

    bool SystemComponent::SetSkeletonPoseLocalTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::vector<AZ::Transform>& localTransforms)
    {
        return m_system && m_system->SetSkeletonPoseLocalTransforms(poseHandle, localTransforms);
    }

    bool SystemComponent::SetSkeletonPoseModelTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::vector<AZ::Transform>& modelTransforms)
    {
        return m_system && m_system->SetSkeletonPoseModelTransforms(poseHandle, modelTransforms);
    }

    AZStd::vector<AZ::Transform> SystemComponent::CopySkeletonPoseLocalTransforms(
        const SkeletonPoseHandle poseHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<AZ::Transform>(
            [this, poseHandle](const AZStd::span<AZ::Transform> transforms)
            {
                return m_system->GetSkeletonPoseLocalTransforms(poseHandle, transforms);
            });
    }

    AZStd::vector<AZ::Transform> SystemComponent::CopySkeletonPoseModelTransforms(
        const SkeletonPoseHandle poseHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<AZ::Transform>(
            [this, poseHandle](const AZStd::span<AZ::Transform> transforms)
            {
                return m_system->GetSkeletonPoseModelTransforms(poseHandle, transforms);
            });
    }

    bool SystemComponent::SampleSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const SkeletonPoseHandle poseHandle,
        const float time)
    {
        return m_system && m_system->SampleSkeletalAnimation(animationHandle, poseHandle, time);
    }

    SkeletonMapperHandle SystemComponent::CreateSkeletonMapper(
        const SkeletonMapperConfiguration& configuration)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CreateSkeletonMapper(configuration);
    }

    bool SystemComponent::DestroySkeletonMapper(
        const SkeletonMapperHandle mapperHandle)
    {
        return m_system && m_system->DestroySkeletonMapper(mapperHandle);
    }

    bool SystemComponent::IsSkeletonMapperValid(
        const SkeletonMapperHandle mapperHandle) const
    {
        return m_system && m_system->IsValid(mapperHandle);
    }

    bool SystemComponent::GetSkeletonMapperState(
        const SkeletonMapperHandle mapperHandle,
        SkeletonMapperState& state) const
    {
        state = {};
        return m_system && m_system->GetSkeletonMapperState(mapperHandle, state);
    }

    AZStd::vector<SkeletonMapperMappingState> SystemComponent::CopySkeletonMapperMappings(
        const SkeletonMapperHandle mapperHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<SkeletonMapperMappingState>(
            [this, mapperHandle](const AZStd::span<SkeletonMapperMappingState> mappings)
            {
                return m_system->GetSkeletonMapperMappings(mapperHandle, mappings);
            });
    }

    bool SystemComponent::GetSkeletonMapperChainState(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex,
        SkeletonMapperChainState& state) const
    {
        state = {};
        return m_system && m_system->GetSkeletonMapperChainState(mapperHandle, chainIndex, state);
    }

    AZStd::vector<AZ::u32> SystemComponent::CopySkeletonMapperSourceChain(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<AZ::u32>(
            [this, mapperHandle, chainIndex](const AZStd::span<AZ::u32> jointIndices)
            {
                return m_system->GetSkeletonMapperSourceChain(mapperHandle, chainIndex, jointIndices);
            });
    }

    AZStd::vector<AZ::u32> SystemComponent::CopySkeletonMapperTargetChain(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<AZ::u32>(
            [this, mapperHandle, chainIndex](const AZStd::span<AZ::u32> jointIndices)
            {
                return m_system->GetSkeletonMapperTargetChain(mapperHandle, chainIndex, jointIndices);
            });
    }

    AZStd::vector<SkeletonMapperUnmappedJoint> SystemComponent::CopySkeletonMapperUnmappedJoints(
        const SkeletonMapperHandle mapperHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<SkeletonMapperUnmappedJoint>(
            [this, mapperHandle](const AZStd::span<SkeletonMapperUnmappedJoint> joints)
            {
                return m_system->GetSkeletonMapperUnmappedJoints(mapperHandle, joints);
            });
    }

    AZStd::vector<SkeletonMapperLockedTranslation> SystemComponent::CopySkeletonMapperLockedTranslations(
        const SkeletonMapperHandle mapperHandle) const
    {
        if (!m_system)
        {
            return {};
        }

        return CopyQueryResults<SkeletonMapperLockedTranslation>(
            [this, mapperHandle](const AZStd::span<SkeletonMapperLockedTranslation> translations)
            {
                return m_system->GetSkeletonMapperLockedTranslations(mapperHandle, translations);
            });
    }

    AZ::s32 SystemComponent::FindMappedSkeletonJoint(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 sourceJointIndex) const
    {
        AZ::u32 targetJointIndex = 0;
        if (!m_system
            || !m_system->GetMappedSkeletonJoint(mapperHandle, sourceJointIndex, targetJointIndex))
        {
            return -1;
        }

        return aznumeric_cast<AZ::s32>(targetJointIndex);
    }

    bool SystemComponent::IsSkeletonJointTranslationLocked(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 targetJointIndex) const
    {
        bool locked = false;
        return m_system
            && m_system->IsSkeletonJointTranslationLocked(mapperHandle, targetJointIndex, locked)
            && locked;
    }

    AZStd::vector<AZ::Transform> SystemComponent::MapSkeletonPose(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::vector<AZ::Transform>& sourceModelTransforms,
        const AZStd::vector<AZ::Transform>& targetLocalTransforms) const
    {
        SkeletonMapperState state;
        if (!m_system || !m_system->GetSkeletonMapperState(mapperHandle, state))
        {
            return {};
        }

        AZStd::vector<AZ::Transform> targetModelTransforms(state.m_targetJointCount);
        if (!m_system->MapSkeletonPose(
                mapperHandle,
                sourceModelTransforms,
                targetLocalTransforms,
                targetModelTransforms))
        {
            targetModelTransforms.clear();
        }
        return targetModelTransforms;
    }

    AZStd::vector<AZ::Transform> SystemComponent::MapSkeletonPoseReverse(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::vector<AZ::Transform>& targetModelTransforms) const
    {
        SkeletonMapperState state;
        if (!m_system || !m_system->GetSkeletonMapperState(mapperHandle, state))
        {
            return {};
        }

        AZStd::vector<AZ::Transform> sourceModelTransforms(state.m_sourceJointCount);
        if (!m_system->MapSkeletonPoseReverse(
                mapperHandle,
                targetModelTransforms,
                sourceModelTransforms))
        {
            sourceModelTransforms.clear();
        }
        return sourceModelTransforms;
    }

    WorldHandle SystemComponent::CreateWorld(
        const WorldConfiguration& configuration)
    {
        if (!m_system)
        {
            return {};
        }

        const WorldHandle worldHandle = m_system->CreateWorld(configuration);
        if (worldHandle)
        {
            m_worldEventStates.push_back({.m_worldHandle = worldHandle});
        }
        return worldHandle;
    }

    bool SystemComponent::DestroyWorld(
        const WorldHandle worldHandle)
    {
        if (!m_system || !m_system->DestroyWorld(worldHandle))
        {
            return false;
        }

        m_worldEventStates.erase(
            AZStd::remove_if(
                m_worldEventStates.begin(),
                m_worldEventStates.end(),
                [worldHandle](const WorldEventState& state)
                {
                    return state.m_worldHandle == worldHandle;
                }),
            m_worldEventStates.end());
        return true;
    }

    WorldHandle SystemComponent::GetDefaultWorldHandle() const
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->GetDefaultWorldHandle();
    }

    RuntimeInfo SystemComponent::GetRuntimeInfo() const
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->GetRuntimeInfo();
    }

    bool SystemComponent::IsWorldValid(
        const WorldHandle worldHandle) const
    {
        return m_system && m_system->IsValid(worldHandle);
    }

    SimulationResult SystemComponent::StepWorld(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        if (!m_system)
        {
            return {.m_errors = SimulationError::InvalidRequest};
        }

        const SimulationResult result = m_system->StepWorldDetailed(worldHandle, fixedTimeStep);
        if (result.m_stepCount == 0)
        {
            return result;
        }

        if (worldHandle == m_system->GetDefaultWorldHandle())
        {
            DispatchWorldEvents(worldHandle, m_defaultWorldLastEventSequence);
            return result;
        }

        for (WorldEventState& state : m_worldEventStates)
        {
            if (state.m_worldHandle == worldHandle)
            {
                DispatchWorldEvents(worldHandle, state.m_lastSequence);
                return result;
            }
        }

        AZ::u64 lastSequence = 0;
        DispatchWorldEvents(worldHandle, lastSequence);
        return result;
    }

    AZ::Vector3 SystemComponent::GetGravity(
        const WorldHandle worldHandle) const
    {
        AZ::Vector3 gravity = AZ::Vector3::CreateZero();
        if (m_system)
        {
            [[maybe_unused]] const bool found = m_system->GetWorldGravity(worldHandle, gravity);
        }
        return gravity;
    }

    bool SystemComponent::SetGravity(
        const WorldHandle worldHandle,
        const AZ::Vector3& gravity)
    {
        return m_system && m_system->SetWorldGravity(worldHandle, gravity);
    }

    SimulationConfiguration SystemComponent::GetSimulationConfiguration(
        const WorldHandle worldHandle) const
    {
        SimulationConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetSimulationConfiguration(worldHandle, configuration);
        }
        return configuration;
    }

    bool SystemComponent::UpdateSimulationConfiguration(
        const WorldHandle worldHandle,
        const SimulationConfiguration& configuration)
    {
        return m_system && m_system->UpdateSimulationConfiguration(worldHandle, configuration);
    }

    WorldRuntimeConfiguration SystemComponent::GetRuntimeConfiguration(
        const WorldHandle worldHandle) const
    {
        WorldRuntimeConfiguration configuration;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetWorldRuntimeConfiguration(worldHandle, configuration);
        }
        return configuration;
    }

    bool SystemComponent::UpdateRuntimeConfiguration(
        const WorldHandle worldHandle,
        const WorldRuntimeConfiguration& configuration)
    {
        return m_system && m_system->UpdateWorldRuntimeConfiguration(worldHandle, configuration);
    }

    StateSnapshotHandle SystemComponent::CaptureWorldState(
        const WorldHandle worldHandle)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CaptureWorldState(worldHandle);
    }

    StateSnapshotHandle SystemComponent::CaptureWorldStateConfigured(
        const WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::vector<BodyHandle>& bodyHandles)
    {
        if (!m_system)
        {
            return {};
        }

        return m_system->CaptureWorldState(
            worldHandle,
            configuration,
            bodyHandles);
    }

    AZStd::vector<StateSnapshotHandle> SystemComponent::CaptureWorldStateParts(
        const WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::vector<BodyHandle>& bodyHandles,
        const AZStd::vector<AZ::u32>& partitionBodyCounts)
    {
        AZStd::vector<StateSnapshotHandle> snapshotHandles(partitionBodyCounts.size());
        if (!m_system
            || !m_system->CaptureWorldStateParts(
                worldHandle,
                configuration,
                bodyHandles,
                partitionBodyCounts,
                snapshotHandles))
        {
            snapshotHandles.clear();
        }
        return snapshotHandles;
    }

    bool SystemComponent::ExportWorldStateArchive(
        const WorldHandle worldHandle,
        const AZStd::vector<StateSnapshotHandle>& snapshotHandles,
        StateSnapshotArchive& archive)
    {
        archive = {};
        return m_system
            && m_system->ExportWorldStateArchive(
                worldHandle,
                snapshotHandles,
                archive);
    }

    AZStd::vector<StateSnapshotHandle> SystemComponent::ImportWorldStateArchive(
        const WorldHandle worldHandle,
        const StateSnapshotArchive& archive)
    {
        if (archive.m_snapshotCount == 0
            || archive.m_snapshotCount > archive.m_binaryState.size())
        {
            return {};
        }

        AZStd::vector<StateSnapshotHandle> snapshotHandles(archive.m_snapshotCount);
        if (!m_system
            || !m_system->ImportWorldStateArchive(
                worldHandle,
                archive,
                snapshotHandles))
        {
            snapshotHandles.clear();
        }
        return snapshotHandles;
    }

    bool SystemComponent::RecaptureWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        return m_system && m_system->CaptureWorldState(worldHandle, snapshotHandle);
    }

    bool SystemComponent::RecaptureWorldStateConfigured(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::vector<BodyHandle>& bodyHandles)
    {
        return m_system
            && m_system->CaptureWorldState(
                worldHandle,
                snapshotHandle,
                configuration,
                bodyHandles);
    }

    bool SystemComponent::DestroyStateSnapshot(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        return m_system && m_system->DestroyStateSnapshot(worldHandle, snapshotHandle);
    }

    bool SystemComponent::IsStateSnapshotValid(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle) const
    {
        return m_system && m_system->IsValid(worldHandle, snapshotHandle);
    }

    bool SystemComponent::RestoreWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        return m_system && m_system->RestoreWorldState(worldHandle, snapshotHandle);
    }

    bool SystemComponent::RestoreWorldStateParts(
        const WorldHandle worldHandle,
        const AZStd::vector<StateSnapshotHandle>& snapshotHandles)
    {
        return m_system && m_system->RestoreWorldStateParts(worldHandle, snapshotHandles);
    }

    bool SystemComponent::ValidateWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle,
        StateValidationResult& result)
    {
        result = {};
        return m_system && m_system->ValidateWorldState(worldHandle, snapshotHandle, result);
    }

    bool SystemComponent::GetWorldStateDigest(
        const WorldHandle worldHandle,
        WorldStateDigest& digest) const
    {
        digest = {};
        return m_system && m_system->GetWorldStateDigest(worldHandle, digest);
    }

    bool SystemComponent::GetWorldStatistics(
        const WorldHandle worldHandle,
        WorldStatistics& statistics) const
    {
        statistics = {};
        return m_system && m_system->GetWorldStatistics(worldHandle, statistics);
    }

    bool SystemComponent::ConfigureDebugCapture(
        const WorldHandle worldHandle,
        const DebugCaptureConfiguration& configuration)
    {
        return m_system && m_system->ConfigureDebugCapture(worldHandle, configuration);
    }

    bool SystemComponent::GetDebugCaptureStatistics(
        const WorldHandle worldHandle,
        DebugCaptureStatistics& statistics) const
    {
        statistics = {};
        return m_system && m_system->GetDebugCaptureStatistics(worldHandle, statistics);
    }

    BodyCollection SystemComponent::GetBodies(
        const WorldHandle worldHandle,
        const BodyKind kind,
        const bool activeOnly,
        const AZ::u32 maximumBodyCount) const
    {
        BodyCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_bodies.resize(AZStd::min(maximumBodyCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->GetBodies(
            worldHandle,
            kind,
            activeOnly,
            result.m_bodies);
        result.m_bodies.resize(queryResult.m_hitCount);
        result.m_requiredBodyCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::GetBodyId(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyId& bodyId) const
    {
        bodyId = {};
        return m_system && m_system->GetBodyId(worldHandle, bodyHandle, bodyId);
    }

    ClosestShapeRaycastResult SystemComponent::RaycastShapeClosest(
        const WorldHandle worldHandle,
        const ShapeRaycastRequest& request) const
    {
        ClosestShapeRaycastResult result;
        result.m_found = m_system && m_system->RaycastShapeClosest(worldHandle, request, result.m_hit);
        return result;
    }

    ShapeRaycastHitCollection SystemComponent::RaycastShapeAll(
        const WorldHandle worldHandle,
        const ShapeRaycastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        ShapeRaycastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->RaycastShapeAll(
            worldHandle,
            request,
            result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    ShapePointHitCollection SystemComponent::CollideShapePoint(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        const AZ::u32 maximumHitCount) const
    {
        ShapePointHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollideShapePoint(
            worldHandle,
            shapeHandle,
            localPosition,
            nullptr,
            result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::CollideShapePointAny(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition) const
    {
        return m_system
            && m_system->CollideShapePointAny(
                worldHandle,
                shapeHandle,
                localPosition);
    }

    ShapeTriangleCollection SystemComponent::CollectShapeTriangles(
        const WorldHandle worldHandle,
        const ShapeTriangleCollectionRequest& request,
        const AZ::u32 maximumTriangleCount) const
    {
        ShapeTriangleCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_triangles.resize(AZStd::min(maximumTriangleCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollectShapeTriangles(
            worldHandle,
            request,
            result.m_triangles);
        result.m_triangles.resize(queryResult.m_hitCount);
        result.m_requiredTriangleCount = queryResult.m_requiredHitCount;
        return result;
    }

    ClosestRaycastResult SystemComponent::RaycastTransformedShapeClosest(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request) const
    {
        ClosestRaycastResult result;
        result.m_found = m_system
            && m_system->RaycastTransformedShapeClosest(
                worldHandle,
                shape,
                request,
                result.m_hit);
        return result;
    }

    RaycastHitCollection SystemComponent::RaycastTransformedShapeAll(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        RaycastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->RaycastTransformedShapeAll(
            worldHandle,
            shape,
            request,
            result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    OverlapHitCollection SystemComponent::CollideTransformedShapePoint(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position,
        const AZ::u32 maximumHitCount) const
    {
        OverlapHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollideTransformedShapePoint(
            worldHandle,
            shape,
            position,
            nullptr,
            result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::CollideTransformedShapePointAny(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position) const
    {
        return m_system
            && m_system->CollideTransformedShapePointAny(
                worldHandle,
                shape,
                position);
    }

    TransformedShapeCollection SystemComponent::CollectTransformedShapeChildren(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        const AZ::u32 maximumShapeCount) const
    {
        TransformedShapeCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_shapes.resize(AZStd::min(maximumShapeCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollectTransformedShapeChildren(
            worldHandle,
            shape,
            bounds,
            nullptr,
            result.m_shapes);
        result.m_shapes.resize(queryResult.m_hitCount);
        result.m_requiredShapeCount = queryResult.m_requiredHitCount;
        return result;
    }

    TransformedTriangleCollection SystemComponent::CollectTransformedShapeTriangles(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        const AZ::u32 maximumTriangleCount) const
    {
        TransformedTriangleCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_triangles.resize(AZStd::min(maximumTriangleCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollectTransformedShapeTriangles(
            worldHandle,
            shape,
            bounds,
            result.m_triangles);
        result.m_triangles.resize(queryResult.m_hitCount);
        result.m_requiredTriangleCount = queryResult.m_requiredHitCount;
        return result;
    }

    SurfaceNormalResult SystemComponent::GetTransformedShapeSurfaceNormal(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const SubShapeId subShapeId,
        const WorldPosition& position) const
    {
        SurfaceNormalResult result;
        result.m_found = m_system
            && m_system->GetTransformedShapeSurfaceNormal(
                worldHandle,
                shape,
                subShapeId,
                position,
                result.m_normal);
        return result;
    }

    SupportingFaceVertexCollection SystemComponent::GetTransformedShapeSupportingFace(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const SubShapeId subShapeId,
        const AZ::Vector3& direction,
        const AZ::u32 maximumVertexCount) const
    {
        SupportingFaceVertexCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_vertices.resize(AZStd::min(
            maximumVertexCount,
            MaximumSupportingFaceVertexCount));
        const QueryResult queryResult = m_system->GetTransformedShapeSupportingFace(
            worldHandle,
            shape,
            subShapeId,
            direction,
            result.m_vertices);
        result.m_vertices.resize(queryResult.m_hitCount);
        result.m_requiredVertexCount = queryResult.m_requiredHitCount;
        return result;
    }

    ClosestRaycastResult SystemComponent::RaycastClosest(
        const WorldHandle worldHandle,
        const RaycastRequest& request) const
    {
        ClosestRaycastResult result;
        result.m_found = m_system && m_system->RaycastClosest(worldHandle, request, result.m_hit);
        return result;
    }

    ClosestRaycastResultCollection SystemComponent::RaycastClosestBatch(
        const WorldHandle worldHandle,
        const RaycastRequestCollection& requestCollection) const
    {
        const AZStd::span<const RaycastRequest> requests = requestCollection.GetRequests();
        ClosestRaycastResultCollection collection;
        collection.m_requiredResultCount = aznumeric_cast<AZ::u32>(requests.size());
        collection.m_results.resize(requests.size());
        if (!m_system)
        {
            collection.m_results.clear();
            return collection;
        }

        const BufferResult result = m_system->RaycastClosestBatch(
            worldHandle,
            requests,
            collection.m_results);
        collection.m_results.resize(result.m_count);
        collection.m_requiredResultCount = result.m_requiredCount;
        return collection;
    }

    bool SystemComponent::RaycastAny(
        const WorldHandle worldHandle,
        const RaycastRequest& request) const
    {
        return m_system && m_system->RaycastAny(worldHandle, request);
    }

    RaycastHitCollection SystemComponent::RaycastAll(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        RaycastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->RaycastAll(worldHandle, request, result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    RaycastHitCollection SystemComponent::RaycastClosestPerBody(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        RaycastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->RaycastClosestPerBody(
            worldHandle,
            request,
            result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    OverlapHitCollection SystemComponent::OverlapPoint(
        const WorldHandle worldHandle,
        const PointOverlapRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        OverlapHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->OverlapPoint(worldHandle, request, result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::OverlapPointAny(
        const WorldHandle worldHandle,
        const PointOverlapRequest& request) const
    {
        return m_system && m_system->OverlapPointAny(worldHandle, request);
    }

    ShapeOverlapHitCollection SystemComponent::CollideShape(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        ShapeOverlapHitCollection result;
        if (!m_system)
        {
            return result;
        }

        const AZ::u32 hitCapacity = AZStd::min(maximumHitCount, MaximumScriptQueryHits);
        result.m_hits.resize(hitCapacity);
        const ShapeQueryFaceBuffers faceBuffers = PrepareShapeQueryFaceBuffers(
            request.m_faceCollectionMode,
            hitCapacity,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        const QueryResult queryResult = m_system->CollideShape(
            worldHandle,
            request,
            result.m_hits,
            faceBuffers);
        result.m_hits.resize(queryResult.m_hitCount);
        ResizeShapeQueryFaceBuffers(
            queryResult.m_hitCount,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    OverlapHitCollection SystemComponent::OverlapShape(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        OverlapHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->OverlapShape(worldHandle, request, result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::OverlapShapeAny(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request) const
    {
        return m_system && m_system->OverlapShapeAny(worldHandle, request);
    }

    ClosestShapeCastResult SystemComponent::CastShapeClosest(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request) const
    {
        ClosestShapeCastResult result;
        if (!m_system)
        {
            return result;
        }

        const ShapeQueryFaceBuffers faceBuffers = PrepareShapeQueryFaceBuffers(
            request.m_faceCollectionMode,
            1,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        result.m_found = m_system->CastShapeClosest(
            worldHandle,
            request,
            result.m_hit,
            faceBuffers);
        AZ::u32 hitCount = 0;
        if (result.m_found)
        {
            hitCount = 1;
        }
        ResizeShapeQueryFaceBuffers(
            hitCount,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        return result;
    }

    ShapeCastHitCollection SystemComponent::CastShapeAll(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        ShapeCastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        const AZ::u32 hitCapacity = AZStd::min(maximumHitCount, MaximumScriptQueryHits);
        result.m_hits.resize(hitCapacity);
        const ShapeQueryFaceBuffers faceBuffers = PrepareShapeQueryFaceBuffers(
            request.m_faceCollectionMode,
            hitCapacity,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        const QueryResult queryResult = m_system->CastShapeAll(
            worldHandle,
            request,
            result.m_hits,
            faceBuffers);
        result.m_hits.resize(queryResult.m_hitCount);
        ResizeShapeQueryFaceBuffers(
            queryResult.m_hitCount,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    ShapeCastHitCollection SystemComponent::CastShapeClosestPerBody(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        ShapeCastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        const AZ::u32 hitCapacity = AZStd::min(maximumHitCount, MaximumScriptQueryHits);
        result.m_hits.resize(hitCapacity);
        const ShapeQueryFaceBuffers faceBuffers = PrepareShapeQueryFaceBuffers(
            request.m_faceCollectionMode,
            hitCapacity,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        const QueryResult queryResult = m_system->CastShapeClosestPerBody(
            worldHandle,
            request,
            result.m_hits,
            faceBuffers);
        result.m_hits.resize(queryResult.m_hitCount);
        ResizeShapeQueryFaceBuffers(
            queryResult.m_hitCount,
            result.m_queryFaceVertices,
            result.m_targetFaceVertices);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    BroadPhaseHitCollection SystemComponent::OverlapBroadPhase(
        const WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        BroadPhaseHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->OverlapBroadPhase(worldHandle, request, result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    bool SystemComponent::OverlapBroadPhaseAny(
        const WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request) const
    {
        return m_system && m_system->OverlapBroadPhaseAny(worldHandle, request);
    }

    ClosestBroadPhaseCastResult SystemComponent::CastBroadPhaseClosest(
        const WorldHandle worldHandle,
        const BroadPhaseCastRequest& request) const
    {
        ClosestBroadPhaseCastResult result;
        result.m_found = m_system && m_system->CastBroadPhaseClosest(worldHandle, request, result.m_hit);
        return result;
    }

    BroadPhaseCastHitCollection SystemComponent::CastBroadPhaseAll(
        const WorldHandle worldHandle,
        const BroadPhaseCastRequest& request,
        const AZ::u32 maximumHitCount) const
    {
        BroadPhaseCastHitCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_hits.resize(AZStd::min(maximumHitCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CastBroadPhaseAll(worldHandle, request, result.m_hits);
        result.m_hits.resize(queryResult.m_hitCount);
        result.m_requiredHitCount = queryResult.m_requiredHitCount;
        return result;
    }

    TransformedShapeCollection SystemComponent::CollectShapesInBounds(
        const WorldHandle worldHandle,
        const ShapeCollectionRequest& request,
        const AZ::u32 maximumShapeCount) const
    {
        TransformedShapeCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_shapes.resize(AZStd::min(maximumShapeCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollectShapesInBounds(worldHandle, request, result.m_shapes);
        result.m_shapes.resize(queryResult.m_hitCount);
        result.m_requiredShapeCount = queryResult.m_requiredHitCount;
        return result;
    }

    SupportingFaceVertexCollection SystemComponent::GetSupportingFace(
        const WorldHandle worldHandle,
        const SupportingFaceRequest& request,
        const AZ::u32 maximumVertexCount) const
    {
        SupportingFaceVertexCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_vertices.resize(AZStd::min(
            maximumVertexCount,
            MaximumSupportingFaceVertexCount));
        const QueryResult queryResult = m_system->GetSupportingFace(
            worldHandle,
            request,
            result.m_vertices);
        result.m_vertices.resize(queryResult.m_hitCount);
        result.m_requiredVertexCount = queryResult.m_requiredHitCount;
        return result;
    }

    TransformedTriangleCollection SystemComponent::CollectTriangles(
        const WorldHandle worldHandle,
        const TriangleCollectionRequest& request,
        const AZ::u32 maximumTriangleCount) const
    {
        TransformedTriangleCollection result;
        if (!m_system)
        {
            return result;
        }

        result.m_triangles.resize(AZStd::min(maximumTriangleCount, MaximumScriptQueryHits));
        const QueryResult queryResult = m_system->CollectTriangles(
            worldHandle,
            request,
            result.m_triangles);
        result.m_triangles.resize(queryResult.m_hitCount);
        result.m_requiredTriangleCount = queryResult.m_requiredHitCount;
        return result;
    }

    BroadPhaseBoundsResult SystemComponent::GetBroadPhaseBounds(
        const WorldHandle worldHandle) const
    {
        BroadPhaseBoundsResult result;
        result.m_found = m_system && m_system->GetBroadPhaseBounds(worldHandle, result.m_bounds);
        return result;
    }

    bool SystemComponent::OptimizeBroadPhase(
        const WorldHandle worldHandle)
    {
        return m_system && m_system->OptimizeBroadPhase(worldHandle);
    }

    bool SystemComponent::WereBodiesInContact(
        const WorldHandle worldHandle,
        const BodyHandle firstBodyHandle,
        const BodyHandle secondBodyHandle) const
    {
        return m_system
            && m_system->WereBodiesInContact(
                worldHandle,
                firstBodyHandle,
                secondBodyHandle);
    }
} // namespace Jolt
