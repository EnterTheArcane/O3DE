/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HairComponent.h>

#include <Jolt/ComponentUtilities.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        void ScaleDefinition(
            HairDefinitionConfiguration& definition,
            const float scale)
        {
            for (HairVertex& vertex : definition.m_vertices)
            {
                vertex.m_position *= scale;
            }
            for (AZ::Vector3& vertex : definition.m_scalpVertices)
            {
                vertex *= scale;
            }
            for (AZ::Transform& inverseBindPose : definition.m_scalpInverseBindPoses)
            {
                inverseBindPose.SetTranslation(inverseBindPose.GetTranslation() * scale);
            }
            for (HairMaterialConfiguration& material : definition.m_materials)
            {
                material.m_radius.m_minimum *= scale;
                material.m_radius.m_maximum *= scale;
            }
            definition.m_simulationBoundsPadding *= scale;
        }

        AZ::Transform ScaleTranslation(
            AZ::Transform transform,
            const float scale)
        {
            transform.SetTranslation(transform.GetTranslation() * scale);
            return transform;
        }

        AZStd::vector<AZ::Transform> ScaleTranslations(
            const AZStd::span<const AZ::Transform> transforms,
            const float scale)
        {
            AZStd::vector<AZ::Transform> result;
            result.reserve(transforms.size());
            for (const AZ::Transform& transform : transforms)
            {
                result.push_back(ScaleTranslation(transform, scale));
            }
            return result;
        }

        class HairNotificationBusBehaviorHandler final
            : public HairNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(
                HairNotificationBusBehaviorHandler,
                "{148C6108-F9AE-40FA-BAD3-1D5F48B4355A}",
                AZ::SystemAllocator,
                OnHairCreated,
                OnHairDestroying,
                OnHairDestroyed);

            void OnHairCreated(
                const WorldHandle worldHandle,
                const HairHandle hairHandle) override
            {
                Call(FN_OnHairCreated, worldHandle, hairHandle);
            }

            void OnHairDestroying(
                const WorldHandle worldHandle,
                const HairHandle hairHandle) override
            {
                Call(FN_OnHairDestroying, worldHandle, hairHandle);
            }

            void OnHairDestroyed(
                const WorldHandle worldHandle,
                const HairHandle hairHandle) override
            {
                Call(FN_OnHairDestroyed, worldHandle, hairHandle);
            }
        };
    } // namespace

    HairComponent::HairComponent() = default;

    HairComponent::HairComponent(
        HairComponentConfiguration configuration)
        : m_configuration(AZStd::make_unique<HairComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    void HairComponent::Reflect(
        AZ::ReflectContext* context)
    {
        HairComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<HairGridCellState>>();
            serializeContext->RegisterGenericType<AZStd::vector<HairVertexState>>();

            serializeContext
                ->Class<HairComponent, AZ::Component>()
                ->Field("Configuration", &HairComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<HairComponent>("Jolt Hair", "Simulates authored hair on deterministic fixed steps.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponent::m_configuration,
                        "Configuration",
                        "Strands, scalp skinning, materials, and update policy.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<HairDefinitionState>("HairDefinitionState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("simulationBounds", BehaviorValueGetter(&HairDefinitionState::m_simulationBounds), nullptr)
                ->Property("gridCellCount", BehaviorValueGetter(&HairDefinitionState::m_gridCellCount), nullptr)
                ->Property("jointCount", BehaviorValueGetter(&HairDefinitionState::m_jointCount), nullptr)
                ->Property(
                    "maximumVerticesPerStrand",
                    BehaviorValueGetter(&HairDefinitionState::m_maximumVerticesPerStrand),
                    nullptr)
                ->Property(
                    "paddedSimulationVertexCount",
                    BehaviorValueGetter(&HairDefinitionState::m_paddedSimulationVertexCount),
                    nullptr)
                ->Property("renderVertexCount", BehaviorValueGetter(&HairDefinitionState::m_renderVertexCount), nullptr)
                ->Property("scalpVertexCount", BehaviorValueGetter(&HairDefinitionState::m_scalpVertexCount), nullptr)
                ->Property(
                    "simulationVertexCount",
                    BehaviorValueGetter(&HairDefinitionState::m_simulationVertexCount),
                    nullptr)
                ->Property("densityScale", BehaviorValueGetter(&HairDefinitionState::m_densityScale), nullptr)
                ->Property(
                    "maximumHairToScalpDistanceSquared",
                    BehaviorValueGetter(&HairDefinitionState::m_maximumHairToScalpDistanceSquared),
                    nullptr);

            behaviorContext->Class<HairState>("HairState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("worldTransform", BehaviorValueGetter(&HairState::m_worldTransform), nullptr)
                ->Property(
                    "scalpToHeadTransform",
                    BehaviorValueGetter(&HairState::m_scalpToHeadTransform),
                    nullptr)
                ->Property("renderVertexCount", BehaviorValueGetter(&HairState::m_renderVertexCount), nullptr)
                ->Property("simulationVertexCount", BehaviorValueGetter(&HairState::m_simulationVertexCount), nullptr)
                ->Property("scalpVertexCount", BehaviorValueGetter(&HairState::m_scalpVertexCount), nullptr)
                ->Property("gridCellCount", BehaviorValueGetter(&HairState::m_gridCellCount), nullptr)
                ->Property("gridSizeX", BehaviorValueGetter(&HairState::m_gridSizeX), nullptr)
                ->Property("gridSizeY", BehaviorValueGetter(&HairState::m_gridSizeY), nullptr)
                ->Property("gridSizeZ", BehaviorValueGetter(&HairState::m_gridSizeZ), nullptr)
                ->Property("teleported", BehaviorValueGetter(&HairState::m_teleported), nullptr);

            behaviorContext->Class<HairVertexState>("HairVertexState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("localPosition", BehaviorValueGetter(&HairVertexState::m_localPosition), nullptr)
                ->Property("localRotation", BehaviorValueGetter(&HairVertexState::m_localRotation), nullptr)
                ->Property("localLinearVelocity", BehaviorValueGetter(&HairVertexState::m_localLinearVelocity), nullptr)
                ->Property("localAngularVelocity", BehaviorValueGetter(&HairVertexState::m_localAngularVelocity), nullptr);

            behaviorContext->Class<HairGridCellState>("HairGridCellState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("velocity", &HairGridCellState::GetVelocity, nullptr)
                ->Property("density", &HairGridCellState::GetDensity, nullptr);

            behaviorContext->Class<HairReadbackResult>("HairReadbackResult")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("vertexStates", BehaviorValueGetter(&HairReadbackResult::m_vertexStates), nullptr)
                ->Property("renderPositions", BehaviorValueGetter(&HairReadbackResult::m_renderPositions), nullptr)
                ->Property("scalpPositions", BehaviorValueGetter(&HairReadbackResult::m_scalpPositions), nullptr)
                ->Property("gridCells", BehaviorValueGetter(&HairReadbackResult::m_gridCells), nullptr);

            behaviorContext->EBus<HairRequestBus>("JoltHairRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IHairRequests::EnableSimulation)
                ->Event("DisableSimulation", &IHairRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IHairRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IHairRequests::GetWorldHandle)
                ->Event("GetHairHandle", &IHairRequests::GetHairHandle)
                ->Event("GetDefinitionState", &IHairRequests::GetDefinitionState)
                ->Event("GetState", &IHairRequests::GetState)
                ->Event("SetScalpToHeadTransform", &IHairRequests::SetScalpToHeadTransform)
                ->Event("CopyVertexStates", &IHairRequests::CopyVertexStates)
                ->Event("CopyRenderPositions", &IHairRequests::CopyRenderPositions)
                ->Event("CopyScalpPositions", &IHairRequests::CopyScalpPositions)
                ->Event("CopyGridCellStates", &IHairRequests::CopyGridCellStates)
                ->Event("CopyNeutralDensity", &IHairRequests::CopyNeutralDensity)
                ->Event("CopySkinnedScalpVertices", &IHairRequests::CopySkinnedScalpVertices)
                ->Event("Update", &IHairRequests::UpdateFromTransforms)
                ->Event("EnableAutoUpdate", &IHairRequests::EnableAutoUpdateFromTransforms)
                ->Event("DisableAutoUpdate", &IHairRequests::DisableAutoUpdate);

            behaviorContext->EBus<HairNotificationBus>("JoltHairNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<HairNotificationBusBehaviorHandler>();

            behaviorContext->Class<HairComponent>("Jolt::HairComponent")
                ->RequestBus("JoltHairRequestBus");
        }
    }

    void HairComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltHairService"));
    }

    void HairComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltHairService"));
    }

    void HairComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    bool HairComponent::EnableSimulation()
    {
        if (m_hairHandle)
        {
            return true;
        }
        if (!m_system || !m_configuration)
        {
            return false;
        }

        AZ::Transform entityTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            entityTransform,
            GetEntityId(),
            &AZ::TransformInterface::GetWorldTM);
        m_uniformScale = entityTransform.GetUniformScale();
        if (!entityTransform.IsFinite() || m_uniformScale <= 0.0f)
        {
            return false;
        }

        HairDefinitionConfiguration definition = m_configuration->m_definition;
        if (!AZ::IsClose(m_uniformScale, 1.0f, AZ::Constants::Tolerance))
        {
            ScaleDefinition(definition, m_uniformScale);
        }
        m_definitionHandle = m_system->CreateHairDefinition(definition);
        if (!m_definitionHandle)
        {
            return false;
        }

        HairConfiguration configuration;
        configuration.m_worldTransform = Internal::ToWorldTransform(entityTransform);
        configuration.m_scalpToHeadTransform = ScaleTranslation(
            m_configuration->m_scalpToHeadTransform,
            m_uniformScale);
        configuration.m_definitionHandle = m_definitionHandle;
        configuration.m_objectLayer = m_configuration->m_objectLayer;
        m_hairHandle = m_system->CreateHair(m_worldHandle, configuration);
        if (!m_hairHandle)
        {
            ReleaseDefinition();
            return false;
        }

        if (m_configuration->m_autoUpdate
            && !EnableAutoUpdate(m_configuration->m_jointToHair, m_configuration->m_jointModelTransforms))
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyHair(m_worldHandle, m_hairHandle);
            m_hairHandle = {};
            ReleaseDefinition();
            return false;
        }

        HairNotificationBus::Event(
            GetEntityId(),
            &IHairNotifications::OnHairCreated,
            m_worldHandle,
            m_hairHandle);
        return true;
    }

    bool HairComponent::DisableSimulation()
    {
        if (!m_hairHandle)
        {
            ReleaseDefinition();
            return true;
        }

        const HairHandle hairHandle = m_hairHandle;
        HairNotificationBus::Event(
            GetEntityId(),
            &IHairNotifications::OnHairDestroying,
            m_worldHandle,
            hairHandle);
        if (!m_system->DestroyHair(m_worldHandle, hairHandle))
        {
            return false;
        }
        m_hairHandle = {};
        ReleaseDefinition();
        HairNotificationBus::Event(
            GetEntityId(),
            &IHairNotifications::OnHairDestroyed,
            m_worldHandle,
            hairHandle);
        return true;
    }

    bool HairComponent::IsSimulationEnabled() const
    {
        return m_system && m_hairHandle;
    }

    WorldHandle HairComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    HairHandle HairComponent::GetHairHandle() const
    {
        return m_hairHandle;
    }

    HairDefinitionState HairComponent::GetDefinitionState() const
    {
        HairDefinitionState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded =
                m_system->GetHairDefinitionState(m_definitionHandle, state);
        }
        return state;
    }

    HairState HairComponent::GetState() const
    {
        HairState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded = m_system->GetHairState(m_worldHandle, m_hairHandle, state);
        }
        return state;
    }

    bool HairComponent::SetScalpToHeadTransform(
        const AZ::Transform& scalpToHeadTransform)
    {
        if (!m_system
            || !m_configuration
            || !m_system->SetHairScalpToHeadTransform(
                m_worldHandle,
                m_hairHandle,
                ScaleTranslation(scalpToHeadTransform, m_uniformScale)))
        {
            return false;
        }

        m_configuration->m_scalpToHeadTransform = scalpToHeadTransform;
        return true;
    }

    bool HairComponent::QueryReadback(
        const HairReadbackBuffers& buffers,
        HairReadbackResult& result) const
    {
        return m_system
            && m_system->GetHairReadback(
                m_worldHandle,
                m_hairHandle,
                buffers,
                result);
    }

    QueryResult HairComponent::QueryVertexStates(
        const AZStd::span<HairVertexState> states) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetHairVertexStates(m_worldHandle, m_hairHandle, states);
    }

    QueryResult HairComponent::QueryRenderPositions(
        const AZStd::span<AZ::Vector3> positions) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetHairRenderPositions(m_worldHandle, m_hairHandle, positions);
    }

    QueryResult HairComponent::QueryScalpPositions(
        const AZStd::span<AZ::Vector3> positions) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetHairScalpPositions(m_worldHandle, m_hairHandle, positions);
    }

    QueryResult HairComponent::QueryGridCellStates(
        const AZStd::span<HairGridCellState> states) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetHairGridCellStates(m_worldHandle, m_hairHandle, states);
    }

    QueryResult HairComponent::QueryNeutralDensity(
        const AZStd::span<float> density) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetHairNeutralDensity(m_definitionHandle, density);
    }

    bool HairComponent::SkinScalpVertices(
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms,
        const AZStd::span<AZ::Transform> preparedJointTransforms,
        const AZStd::span<AZ::Vector3> scalpVertices) const
    {
        if (!m_system)
        {
            return false;
        }
        if (AZ::IsClose(m_uniformScale, 1.0f, AZ::Constants::Tolerance))
        {
            return m_system->SkinHairScalpVertices(
                m_definitionHandle,
                jointToHair,
                jointModelTransforms,
                preparedJointTransforms,
                scalpVertices);
        }
        if (preparedJointTransforms.size() < jointModelTransforms.size())
        {
            return false;
        }

        for (size_t jointIndex = 0; jointIndex < jointModelTransforms.size(); ++jointIndex)
        {
            preparedJointTransforms[jointIndex] =
                ScaleTranslation(jointModelTransforms[jointIndex], m_uniformScale);
        }
        return m_system->SkinHairScalpVertices(
            m_definitionHandle,
            ScaleTranslation(jointToHair, m_uniformScale),
            preparedJointTransforms.subspan(0, jointModelTransforms.size()),
            preparedJointTransforms,
            scalpVertices);
    }

    AZStd::vector<HairVertexState> HairComponent::CopyVertexStates() const
    {
        const QueryResult size = QueryVertexStates({});
        AZStd::vector<HairVertexState> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryVertexStates(result);
        return result;
    }

    AZStd::vector<AZ::Vector3> HairComponent::CopyRenderPositions() const
    {
        const QueryResult size = QueryRenderPositions({});
        AZStd::vector<AZ::Vector3> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryRenderPositions(result);
        return result;
    }

    AZStd::vector<AZ::Vector3> HairComponent::CopyScalpPositions() const
    {
        const QueryResult size = QueryScalpPositions({});
        AZStd::vector<AZ::Vector3> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryScalpPositions(result);
        return result;
    }

    AZStd::vector<HairGridCellState> HairComponent::CopyGridCellStates() const
    {
        const QueryResult size = QueryGridCellStates({});
        AZStd::vector<HairGridCellState> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryGridCellStates(result);
        return result;
    }

    AZStd::vector<float> HairComponent::CopyNeutralDensity() const
    {
        const QueryResult size = QueryNeutralDensity({});
        AZStd::vector<float> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryNeutralDensity(result);
        return result;
    }

    AZStd::vector<AZ::Vector3> HairComponent::CopySkinnedScalpVertices(
        const AZ::Transform& jointToHair,
        const AZStd::vector<AZ::Transform>& jointModelTransforms) const
    {
        const HairDefinitionState state = GetDefinitionState();
        AZStd::vector<AZ::Transform> preparedJointTransforms(state.m_jointCount);
        AZStd::vector<AZ::Vector3> scalpVertices(state.m_scalpVertexCount);
        if (!SkinScalpVertices(
            jointToHair,
            jointModelTransforms,
            preparedJointTransforms,
            scalpVertices))
        {
            scalpVertices.clear();
        }
        return scalpVertices;
    }

    bool HairComponent::Update(
        const float deltaTime,
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        if (!m_system)
        {
            return false;
        }
        if (AZ::IsClose(m_uniformScale, 1.0f, AZ::Constants::Tolerance))
        {
            return m_system->UpdateHair(
                m_worldHandle,
                m_hairHandle,
                deltaTime,
                jointToHair,
                jointModelTransforms);
        }
        const AZStd::vector<AZ::Transform> scaledTransforms =
            ScaleTranslations(jointModelTransforms, m_uniformScale);
        return m_system->UpdateHair(
            m_worldHandle,
            m_hairHandle,
            deltaTime,
            ScaleTranslation(jointToHair, m_uniformScale),
            scaledTransforms);
    }

    bool HairComponent::UpdateFromTransforms(
        const float deltaTime,
        const AZ::Transform& jointToHair,
        const AZStd::vector<AZ::Transform>& jointModelTransforms)
    {
        return Update(deltaTime, jointToHair, jointModelTransforms);
    }

    bool HairComponent::EnableAutoUpdate(
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        if (!m_system)
        {
            return false;
        }
        if (AZ::IsClose(m_uniformScale, 1.0f, AZ::Constants::Tolerance))
        {
            return m_system->EnableHairAutoUpdate(
                m_worldHandle,
                m_hairHandle,
                jointToHair,
                jointModelTransforms);
        }
        const AZStd::vector<AZ::Transform> scaledTransforms =
            ScaleTranslations(jointModelTransforms, m_uniformScale);
        return m_system->EnableHairAutoUpdate(
            m_worldHandle,
            m_hairHandle,
            ScaleTranslation(jointToHair, m_uniformScale),
            scaledTransforms);
    }

    bool HairComponent::EnableAutoUpdateFromTransforms(
        const AZ::Transform& jointToHair,
        const AZStd::vector<AZ::Transform>& jointModelTransforms)
    {
        return EnableAutoUpdate(jointToHair, jointModelTransforms);
    }

    bool HairComponent::DisableAutoUpdate()
    {
        return m_system && m_system->DisableHairAutoUpdate(m_worldHandle, m_hairHandle);
    }

    void HairComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<HairComponentConfiguration>(
                HairComponentConfiguration::CreateDefault());
        }
        m_system = GetRuntime();
        if (!m_system)
        {
            return;
        }

        m_worldHandle = m_system->GetDefaultWorldHandle();
        HairRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        if (m_configuration->m_enabled)
        {
            [[maybe_unused]] const bool enabled = EnableSimulation();
        }
    }

    void HairComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        HairRequestBus::Handler::BusDisconnect();
        [[maybe_unused]] const bool disabled = DisableSimulation();
        m_system = nullptr;
        m_worldHandle = {};
    }

    void HairComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (!m_system || !m_hairHandle || !world.IsFinite())
        {
            return;
        }
        const float uniformScale = world.GetUniformScale();
        if (!AZ::IsClose(uniformScale, m_uniformScale, AZ::Constants::Tolerance))
        {
            if (!DisableSimulation())
            {
                return;
            }
            [[maybe_unused]] const bool enabled = EnableSimulation();
            return;
        }

        [[maybe_unused]] const bool transformed = m_system->SetHairTransform(
            m_worldHandle,
            m_hairHandle,
            Internal::ToWorldTransform(world),
            true);
    }

    void HairComponent::ReleaseDefinition()
    {
        if (m_definitionHandle && m_system)
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyHairDefinition(m_definitionHandle);
        }
        m_definitionHandle = {};
    }
} // namespace Jolt
