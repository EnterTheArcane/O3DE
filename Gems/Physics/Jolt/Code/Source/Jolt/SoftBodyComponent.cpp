/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SoftBodyComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/ComponentDependencyManager.h>
#include <Jolt/ComponentUtilities.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        template<class Value, class Query>
        [[nodiscard]]
        AZStd::vector<Value> CopyQueryResults(
            const Query& query)
        {
            const QueryResult size = query(AZStd::span<Value>{});
            if (size.m_requiredHitCount == 0)
            {
                return {};
            }

            AZStd::vector<Value> values(size.m_requiredHitCount);
            const QueryResult result = query(AZStd::span<Value>(values));
            values.resize(result.m_hitCount);
            return values;
        }

        void ScaleDefinition(
            SoftBodyDefinitionConfiguration& definition,
            const float scale)
        {
            for (SoftBodyVertex& vertex : definition.m_vertices)
            {
                vertex.m_position *= scale;
            }
            for (SoftBodyEdgeConstraint& constraint : definition.m_edgeConstraints)
            {
                if (constraint.m_restLength >= 0.0f)
                {
                    constraint.m_restLength *= scale;
                }
            }
            for (SoftBodyLongRangeConstraint& constraint : definition.m_longRangeConstraints)
            {
                constraint.m_maximumDistance *= scale;
            }
            for (SoftBodyRodStretchShearConstraint& constraint : definition.m_rodStretchShearConstraints)
            {
                if (constraint.m_restLength >= 0.0f)
                {
                    constraint.m_restLength *= scale;
                }
            }
            const float volumeScale = scale * scale * scale;
            for (SoftBodyVolumeConstraint& constraint : definition.m_volumeConstraints)
            {
                if (!constraint.m_calculateRestVolume)
                {
                    constraint.m_restVolume *= volumeScale;
                }
            }
            for (SoftBodyInverseBind& inverseBind : definition.m_inverseBinds)
            {
                inverseBind.m_transform.SetTranslation(
                    inverseBind.m_transform.GetTranslation() * scale);
            }
            for (SoftBodySkinConstraint& constraint : definition.m_skinConstraints)
            {
                if (constraint.m_backstopDistance != AZStd::numeric_limits<float>::max())
                {
                    constraint.m_backstopDistance *= scale;
                }
                constraint.m_backstopRadius *= scale;
                if (constraint.m_maximumDistance != AZStd::numeric_limits<float>::max())
                {
                    constraint.m_maximumDistance *= scale;
                }
            }
        }
    } // namespace

    SoftBodyComponent::SoftBodyComponent() = default;

    SoftBodyComponent::SoftBodyComponent(
        SoftBodyComponentConfiguration configuration)
        : m_configuration(AZStd::make_unique<SoftBodyComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    void SoftBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        SoftBodyComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<MaterialHandle>>();
            serializeContext->RegisterGenericType<AZStd::vector<SoftBodyRodState>>();

            serializeContext
                ->Class<SoftBodyComponent, AZ::Component>()
                ->Field("Configuration", &SoftBodyComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SoftBodyComponent>("Jolt Soft Body", "Simulates deformable authored geometry.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SoftBodyComponent::m_configuration,
                        "Configuration",
                        "Soft-body geometry, constraints, materials, and runtime properties.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyBendType, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyBendType, Dihedral);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyBendType, Distance);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyContactDecision, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyContactDecision, Accept);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyContactDecision, Reject);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyLongRangeAttachmentType, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyLongRangeAttachmentType, EuclideanDistance);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, SoftBodyLongRangeAttachmentType, GeodesicDistance);

            behaviorContext->Class<SoftBodyVertex>("SoftBodyVertex")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVertex::m_position))
                ->Property("velocity", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVertex::m_velocity))
                ->Property("inverseMass", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVertex::m_inverseMass));

            behaviorContext->Class<SoftBodyFace>("SoftBodyFace")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("firstVertex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyFace::m_firstVertex))
                ->Property("secondVertex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyFace::m_secondVertex))
                ->Property("thirdVertex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyFace::m_thirdVertex))
                ->Property("materialIndex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyFace::m_materialIndex));

            behaviorContext->Class<SoftBodyDihedralBendConstraint>("SoftBodyDihedralBendConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "firstVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_firstVertex))
                ->Property(
                    "secondVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_secondVertex))
                ->Property(
                    "thirdVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_thirdVertex))
                ->Property(
                    "fourthVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_fourthVertex))
                ->Property(
                    "compliance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_compliance))
                ->Property(
                    "initialAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_initialAngle))
                ->Property(
                    "calculateInitialAngle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyDihedralBendConstraint::m_calculateInitialAngle));

            behaviorContext->Class<SoftBodyEdgeConstraint>("SoftBodyEdgeConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "firstVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyEdgeConstraint::m_firstVertex))
                ->Property(
                    "secondVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyEdgeConstraint::m_secondVertex))
                ->Property("compliance", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyEdgeConstraint::m_compliance))
                ->Property("restLength", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyEdgeConstraint::m_restLength));

            behaviorContext->Class<SoftBodyInverseBind>("SoftBodyInverseBind")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("transform", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyInverseBind::m_transform))
                ->Property("jointIndex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyInverseBind::m_jointIndex));

            behaviorContext->Class<SoftBodyLongRangeConstraint>("SoftBodyLongRangeConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "fixedVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyLongRangeConstraint::m_fixedVertex))
                ->Property(
                    "dynamicVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyLongRangeConstraint::m_dynamicVertex))
                ->Property(
                    "maximumDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyLongRangeConstraint::m_maximumDistance));

            behaviorContext->Class<SoftBodyRodBendTwistConstraint>("SoftBodyRodBendTwistConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "firstRod",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodBendTwistConstraint::m_firstRod))
                ->Property(
                    "secondRod",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodBendTwistConstraint::m_secondRod))
                ->Property(
                    "initialRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodBendTwistConstraint::m_initialRotation))
                ->Property(
                    "compliance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodBendTwistConstraint::m_compliance))
                ->Property(
                    "calculateInitialRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodBendTwistConstraint::m_calculateInitialRotation));

            behaviorContext->Class<SoftBodyRodStretchShearConstraint>("SoftBodyRodStretchShearConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "firstVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_firstVertex))
                ->Property(
                    "secondVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_secondVertex))
                ->Property(
                    "bishopRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_bishopRotation))
                ->Property(
                    "compliance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_compliance))
                ->Property(
                    "inverseMass",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_inverseMass))
                ->Property(
                    "restLength",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_restLength))
                ->Property(
                    "calculateBishopRotation",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRodStretchShearConstraint::m_calculateBishopRotation));

            behaviorContext->Class<SoftBodySkinWeight>("SoftBodySkinWeight")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "inverseBindIndex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinWeight::m_inverseBindIndex))
                ->Property("weight", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinWeight::m_weight));

            behaviorContext->Class<SoftBodySkinConstraint>("SoftBodySkinConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("vertex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinConstraint::m_vertex))
                ->Property(
                    "backstopDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinConstraint::m_backstopDistance))
                ->Property(
                    "backstopRadius",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinConstraint::m_backstopRadius))
                ->Property(
                    "maximumDistance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodySkinConstraint::m_maximumDistance))
                ->Method("GetWeight", &SoftBodySkinConstraint::GetWeight)
                ->Method("SetWeight", &SoftBodySkinConstraint::SetWeight);

            behaviorContext->Class<SoftBodyVolumeConstraint>("SoftBodyVolumeConstraint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "firstVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_firstVertex))
                ->Property(
                    "secondVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_secondVertex))
                ->Property(
                    "thirdVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_thirdVertex))
                ->Property(
                    "fourthVertex",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_fourthVertex))
                ->Property(
                    "compliance",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_compliance))
                ->Property(
                    "restVolume",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_restVolume))
                ->Property(
                    "calculateRestVolume",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyVolumeConstraint::m_calculateRestVolume));

            behaviorContext->Class<SoftBodyDefinitionState>("SoftBodyDefinitionState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "vertexCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_vertexCount),
                    nullptr)
                ->Property(
                    "faceCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_faceCount),
                    nullptr)
                ->Property(
                    "materialCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_materialCount),
                    nullptr)
                ->Property(
                    "dihedralBendConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_dihedralBendConstraintCount),
                    nullptr)
                ->Property(
                    "edgeConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_edgeConstraintCount),
                    nullptr)
                ->Property(
                    "longRangeConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_longRangeConstraintCount),
                    nullptr)
                ->Property(
                    "rodBendTwistConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_rodBendTwistConstraintCount),
                    nullptr)
                ->Property(
                    "rodStretchShearConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_rodStretchShearConstraintCount),
                    nullptr)
                ->Property(
                    "skinConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_skinConstraintCount),
                    nullptr)
                ->Property(
                    "volumeConstraintCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_volumeConstraintCount),
                    nullptr)
                ->Property(
                    "inverseBindCount",
                    BehaviorValueGetter(&SoftBodyDefinitionState::m_inverseBindCount),
                    nullptr);

            behaviorContext->Class<SoftBodyRuntimeConfiguration>("SoftBodyRuntimeConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("friction", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_friction))
                ->Property("gravityFactor", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_gravityFactor))
                ->Property("linearDamping", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_linearDamping))
                ->Property(
                    "maximumLinearVelocity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_maximumLinearVelocity))
                ->Property("pressure", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_pressure))
                ->Property("restitution", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_restitution))
                ->Property(
                    "skinnedMaximumDistanceMultiplier",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_skinnedMaximumDistanceMultiplier))
                ->Property("vertexRadius", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_vertexRadius))
                ->Property("iterationCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_iterationCount))
                ->Property("allowSleeping", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_allowSleeping))
                ->Property(
                    "enableSkinConstraints",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_enableSkinConstraints))
                ->Property("facesDoubleSided", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_facesDoubleSided))
                ->Property("updatePosition", JOLT_BEHAVIOR_VALUE_PROPERTY(&SoftBodyRuntimeConfiguration::m_updatePosition));

            behaviorContext->Class<SoftBodyRodState>("SoftBodyRodState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("rotation", BehaviorValueGetter(&SoftBodyRodState::m_rotation), nullptr)
                ->Property("angularVelocity", BehaviorValueGetter(&SoftBodyRodState::m_angularVelocity), nullptr);

            behaviorContext->EBus<SoftBodyRequestBus>("JoltSoftBodyRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &ISoftBodyRequests::EnableSimulation)
                ->Event("DisableSimulation", &ISoftBodyRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &ISoftBodyRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &ISoftBodyRequests::GetWorldHandle)
                ->Event("GetBodyHandle", &ISoftBodyRequests::GetBodyHandle)
                ->Event("GetDefinitionHandle", &ISoftBodyRequests::GetDefinitionHandle)
                ->Event("GetCenterOfMassTransform", &ISoftBodyRequests::GetCenterOfMassTransform)
                ->Event("GetDefinitionState", &ISoftBodyRequests::GetDefinitionState)
                ->Event("GetState", &ISoftBodyRequests::GetState)
                ->Event("CopyFaces", &ISoftBodyRequests::CopyFaces)
                ->Event("CopyMaterials", &ISoftBodyRequests::CopyMaterials)
                ->Event("CopyVertices", &ISoftBodyRequests::CopyVertices)
                ->Event("CopyRodStates", &ISoftBodyRequests::CopyRodStates)
                ->Event(
                    "CopyDefinitionDihedralBendConstraints",
                    &ISoftBodyRequests::CopyDefinitionDihedralBendConstraints)
                ->Event(
                    "CopyDefinitionEdgeConstraints",
                    &ISoftBodyRequests::CopyDefinitionEdgeConstraints)
                ->Event("CopyDefinitionFaces", &ISoftBodyRequests::CopyDefinitionFaces)
                ->Event("CopyDefinitionInverseBinds", &ISoftBodyRequests::CopyDefinitionInverseBinds)
                ->Event(
                    "CopyDefinitionLongRangeConstraints",
                    &ISoftBodyRequests::CopyDefinitionLongRangeConstraints)
                ->Event("CopyDefinitionMaterials", &ISoftBodyRequests::CopyDefinitionMaterials)
                ->Event(
                    "CopyDefinitionRodBendTwistConstraints",
                    &ISoftBodyRequests::CopyDefinitionRodBendTwistConstraints)
                ->Event(
                    "CopyDefinitionRodStretchShearConstraints",
                    &ISoftBodyRequests::CopyDefinitionRodStretchShearConstraints)
                ->Event(
                    "CopyDefinitionSkinConstraints",
                    &ISoftBodyRequests::CopyDefinitionSkinConstraints)
                ->Event("CopyDefinitionVertices", &ISoftBodyRequests::CopyDefinitionVertices)
                ->Event(
                    "CopyDefinitionVolumeConstraints",
                    &ISoftBodyRequests::CopyDefinitionVolumeConstraints)
                ->Event("GetInverseInertia", &ISoftBodyRequests::GetInverseInertia)
                ->Event("GetInverseMass", &ISoftBodyRequests::GetInverseMass)
                ->Event("GetRuntimeConfiguration", &ISoftBodyRequests::GetRuntimeConfiguration)
                ->Event("UpdateRuntimeConfiguration", &ISoftBodyRequests::UpdateRuntimeConfiguration)
                ->Event("GetLocalBounds", &ISoftBodyRequests::GetLocalBounds)
                ->Event("GetVolume", &ISoftBodyRequests::GetVolume)
                ->Event("ApplySkinPose", &ISoftBodyRequests::ApplySkinPoseFromTransforms)
                ->Event("RecalculateMassProperties", &ISoftBodyRequests::RecalculateMassProperties)
                ->Event("SetVertexInverseMass", &ISoftBodyRequests::SetVertexInverseMass)
                ->Event("SetVertexInverseMasses", &ISoftBodyRequests::SetVertexInverseMassesFromValues)
                ->Event("SetVertexVelocity", &ISoftBodyRequests::SetVertexVelocity)
                ->Event("SetVertexVelocities", &ISoftBodyRequests::SetVertexVelocitiesFromVectors)
                ->Event("UpdateManually", &ISoftBodyRequests::UpdateManually);

            behaviorContext->Class<SoftBodyComponent>("Jolt::SoftBodyComponent")
                ->RequestBus("JoltSoftBodyRequestBus");
        }
    }

    void SoftBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void SoftBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltBodyService"));
    }

    void SoftBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    bool SoftBodyComponent::EnableSimulation()
    {
        if (m_bodyHandle)
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
        if (!AZ::IsFiniteFloat(m_uniformScale) || m_uniformScale <= 0.0f)
        {
            return false;
        }

        SoftBodyDefinitionConfiguration definition = m_configuration->m_definition;
        definition.m_materials.clear();
        m_materialHandles = AZStd::make_unique<AZStd::vector<MaterialHandle>>();
        m_materialHandles->reserve(m_configuration->m_materials.size());
        for (const MaterialConfiguration& material : m_configuration->m_materials)
        {
            const MaterialHandle materialHandle = m_system->CreateMaterial(material);
            if (!materialHandle)
            {
                ReleaseDefinition();
                return false;
            }
            m_materialHandles->push_back(materialHandle);
            definition.m_materials.push_back(materialHandle);
        }
        if (!AZ::IsClose(m_uniformScale, 1.0f, AZ::Constants::Tolerance))
        {
            ScaleDefinition(definition, m_uniformScale);
        }

        m_definitionHandle = m_system->CreateSoftBodyDefinition(definition);
        if (!m_definitionHandle)
        {
            ReleaseDefinition();
            return false;
        }

        SoftBodyConfiguration body = m_configuration->m_body;
        body.m_definitionHandle = m_definitionHandle;
        body.m_transform = Internal::ToWorldTransform(entityTransform);
        body.m_entityId = GetEntityId();
        body.m_name = AZ::Name(GetEntity()->GetName());
        body.m_vertexRadius *= m_uniformScale;
        m_bodyHandle = m_system->CreateSoftBody(m_worldHandle, body);
        if (!m_bodyHandle)
        {
            ReleaseDefinition();
            return false;
        }
        if (!body.m_manualUpdate
            && !m_system->SetBodyMoveEventsEnabled(m_worldHandle, m_bodyHandle, true))
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyBody(m_worldHandle, m_bodyHandle);
            m_bodyHandle = {};
            ReleaseDefinition();
            return false;
        }

        BodyNotificationBus::Event(
            GetEntityId(),
            &IBodyNotifications::OnBodyCreated,
            m_worldHandle,
            m_bodyHandle);
        return true;
    }

    bool SoftBodyComponent::DisableSimulation()
    {
        return DestroySimulation(false);
    }

    bool SoftBodyComponent::DestroySimulation(const bool mandatory)
    {
        if (!m_bodyHandle)
        {
            ReleaseDefinition();
            return true;
        }

        RuntimeImplementation* system = m_system;
        const WorldHandle worldHandle = m_worldHandle;
        const BodyHandle bodyHandle = m_bodyHandle;
        ResourceDestructionPlan plan;
        const ResourceDestructionObserver observer{
            .m_context = this,
            .m_entityId = GetEntityId(),
            .m_componentId = GetId(),
            .m_notify = &SoftBodyComponent::NotifyResourceDestruction,
        };
        if (!plan.AddObserver(observer))
        {
            return false;
        }
        auto deferDestruction = [&](const ResourceDestructionReservation reservation)
        {
            AZStd::vector<MaterialHandle> materialHandles;
            if (m_materialHandles)
            {
                materialHandles = AZStd::move(*m_materialHandles);
            }
            m_materialHandles.reset();
            const SoftBodyDefinitionHandle definitionHandle = m_definitionHandle;
            m_definitionHandle = {};
            system->DeferSoftBodyDestruction(
                worldHandle,
                bodyHandle,
                AZStd::move(plan),
                reservation,
                definitionHandle,
                AZStd::move(materialHandles));
        };
        auto* dependencyManager = AZ::Interface<IComponentDependencyManager>::Get();
        const bool prepared = !dependencyManager
            || dependencyManager->PrepareBodyDestruction(
                GetEntityId(),
                m_worldHandle,
                bodyHandle,
                plan);
        if (!prepared && !mandatory)
        {
            return false;
        }
        if (!system->ReserveBodyDestruction(worldHandle, bodyHandle, plan))
        {
            if (!mandatory)
            {
                return false;
            }
            if (system->IsBodyDestructionReserved(worldHandle, bodyHandle))
            {
                return true;
            }

            deferDestruction(ResourceDestructionReservation::Pending);
            return true;
        }
        plan.NotifyDestroying();
        if (!system->DestroyReservedBody(worldHandle, bodyHandle, plan))
        {
            deferDestruction(ResourceDestructionReservation::Complete);
            return true;
        }
        plan.NotifyDestroyed();
        return true;
    }

    void SoftBodyComponent::NotifyResourceDestruction(
        void* context,
        const AZ::EntityId entityId,
        const AZ::ComponentId componentId,
        const ResourceDestructionPhase phase)
    {
        auto* componentApplication = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        AZ::Entity* entity = nullptr;
        if (componentApplication)
        {
            entity = componentApplication->FindEntity(entityId);
        }
        SoftBodyComponent* component = nullptr;
        if (entity)
        {
            component = azrtti_cast<SoftBodyComponent*>(entity->FindComponent(componentId));
        }
        if (!component || component != context)
        {
            return;
        }

        const WorldHandle worldHandle = component->m_worldHandle;
        const BodyHandle bodyHandle = component->m_bodyHandle;
        if (phase == ResourceDestructionPhase::Destroying)
        {
            BodyNotificationBus::Event(
                entityId,
                &IBodyNotifications::OnBodyDestroying,
                worldHandle,
                bodyHandle);
            return;
        }

        RuntimeImplementation* system = component->m_system;
        if (!system)
        {
            system = GetRuntime();
        }
        component->m_bodyHandle = {};
        if (system && component->m_definitionHandle)
        {
            [[maybe_unused]] const bool destroyed =
                system->DestroySoftBodyDefinition(component->m_definitionHandle);
        }
        component->m_definitionHandle = {};
        if (system && component->m_materialHandles)
        {
            for (const MaterialHandle materialHandle : *component->m_materialHandles)
            {
                [[maybe_unused]] const bool destroyed = system->DestroyMaterial(materialHandle);
            }
        }
        component->m_materialHandles.reset();
        if (!component->m_system)
        {
            component->m_worldHandle = {};
        }
        BodyNotificationBus::Event(
            entityId,
            &IBodyNotifications::OnBodyDestroyed,
            worldHandle,
            bodyHandle);
    }

    bool SoftBodyComponent::IsSimulationEnabled() const
    {
        return m_system && m_bodyHandle;
    }

    WorldHandle SoftBodyComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    BodyHandle SoftBodyComponent::GetBodyHandle() const
    {
        return m_bodyHandle;
    }

    AZ::u64 SoftBodyComponent::GetUserData() const
    {
        AZ::u64 userData = m_configuration->m_body.m_userData;
        if (m_system && m_bodyHandle)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyUserData(m_worldHandle, m_bodyHandle, userData);
        }
        return userData;
    }

    bool SoftBodyComponent::SetUserData(
        const AZ::u64 userData)
    {
        if (m_bodyHandle
            && (!m_system || !m_system->SetBodyUserData(m_worldHandle, m_bodyHandle, userData)))
        {
            return false;
        }

        m_configuration->m_body.m_userData = userData;
        return true;
    }

    SoftBodyDefinitionHandle SoftBodyComponent::GetDefinitionHandle() const
    {
        return m_definitionHandle;
    }

    WorldTransform SoftBodyComponent::GetCenterOfMassTransform() const
    {
        WorldTransform transform;
        if (m_system)
        {
            [[maybe_unused]] const bool found =
                m_system->GetBodyCenterOfMassTransform(m_worldHandle, m_bodyHandle, transform);
        }
        return transform;
    }

    SoftBodyDefinitionState SoftBodyComponent::GetDefinitionState() const
    {
        SoftBodyDefinitionState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded =
                m_system->GetSoftBodyDefinitionState(m_definitionHandle, state);
        }
        return state;
    }

    BodyState SoftBodyComponent::GetState() const
    {
        BodyState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded = m_system->GetBodyState(m_worldHandle, m_bodyHandle, state);
        }
        return state;
    }

    QueryResult SoftBodyComponent::QueryVertices(
        const AZStd::span<SoftBodyVertex> vertices) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyVertices(m_worldHandle, m_bodyHandle, vertices);
    }

    QueryResult SoftBodyComponent::QueryFaces(
        const AZStd::span<SoftBodyFace> faces) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyFaces(m_worldHandle, m_bodyHandle, faces);
    }

    QueryResult SoftBodyComponent::QueryMaterials(
        const AZStd::span<MaterialHandle> materials) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyMaterials(m_worldHandle, m_bodyHandle, materials);
    }

    QueryResult SoftBodyComponent::QueryRodStates(
        const AZStd::span<SoftBodyRodState> rods) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyRodStates(m_worldHandle, m_bodyHandle, rods);
    }

    QueryResult SoftBodyComponent::QueryDefinitionDihedralBendConstraints(
        const AZStd::span<SoftBodyDihedralBendConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionDihedralBendConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionEdgeConstraints(
        const AZStd::span<SoftBodyEdgeConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionEdgeConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionFaces(
        const AZStd::span<SoftBodyFace> faces) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionFaces(m_definitionHandle, faces);
    }

    QueryResult SoftBodyComponent::QueryDefinitionInverseBinds(
        const AZStd::span<SoftBodyInverseBind> inverseBinds) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionInverseBinds(
            m_definitionHandle,
            inverseBinds);
    }

    QueryResult SoftBodyComponent::QueryDefinitionLongRangeConstraints(
        const AZStd::span<SoftBodyLongRangeConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionLongRangeConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionMaterials(
        const AZStd::span<MaterialHandle> materials) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionMaterials(m_definitionHandle, materials);
    }

    QueryResult SoftBodyComponent::QueryDefinitionRodBendTwistConstraints(
        const AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionRodBendTwistConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionRodStretchShearConstraints(
        const AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionRodStretchShearConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionSkinConstraints(
        const AZStd::span<SoftBodySkinConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionSkinConstraints(
            m_definitionHandle,
            constraints);
    }

    QueryResult SoftBodyComponent::QueryDefinitionVertices(
        const AZStd::span<SoftBodyVertex> vertices) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionVertices(m_definitionHandle, vertices);
    }

    QueryResult SoftBodyComponent::QueryDefinitionVolumeConstraints(
        const AZStd::span<SoftBodyVolumeConstraint> constraints) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetSoftBodyDefinitionVolumeConstraints(
            m_definitionHandle,
            constraints);
    }

    AZStd::vector<SoftBodyVertex> SoftBodyComponent::CopyVertices() const
    {
        return CopyQueryResults<SoftBodyVertex>(
            [this](const AZStd::span<SoftBodyVertex> values)
            {
                return QueryVertices(values);
            });
    }

    AZStd::vector<SoftBodyFace> SoftBodyComponent::CopyFaces() const
    {
        return CopyQueryResults<SoftBodyFace>(
            [this](const AZStd::span<SoftBodyFace> values)
            {
                return QueryFaces(values);
            });
    }

    AZStd::vector<MaterialHandle> SoftBodyComponent::CopyMaterials() const
    {
        return CopyQueryResults<MaterialHandle>(
            [this](const AZStd::span<MaterialHandle> values)
            {
                return QueryMaterials(values);
            });
    }

    AZStd::vector<SoftBodyRodState> SoftBodyComponent::CopyRodStates() const
    {
        return CopyQueryResults<SoftBodyRodState>(
            [this](const AZStd::span<SoftBodyRodState> values)
            {
                return QueryRodStates(values);
            });
    }

    AZStd::vector<SoftBodyDihedralBendConstraint> SoftBodyComponent::CopyDefinitionDihedralBendConstraints() const
    {
        return CopyQueryResults<SoftBodyDihedralBendConstraint>(
            [this](const AZStd::span<SoftBodyDihedralBendConstraint> values)
            {
                return QueryDefinitionDihedralBendConstraints(values);
            });
    }

    AZStd::vector<SoftBodyEdgeConstraint> SoftBodyComponent::CopyDefinitionEdgeConstraints() const
    {
        return CopyQueryResults<SoftBodyEdgeConstraint>(
            [this](const AZStd::span<SoftBodyEdgeConstraint> values)
            {
                return QueryDefinitionEdgeConstraints(values);
            });
    }

    AZStd::vector<SoftBodyFace> SoftBodyComponent::CopyDefinitionFaces() const
    {
        return CopyQueryResults<SoftBodyFace>(
            [this](const AZStd::span<SoftBodyFace> values)
            {
                return QueryDefinitionFaces(values);
            });
    }

    AZStd::vector<SoftBodyInverseBind> SoftBodyComponent::CopyDefinitionInverseBinds() const
    {
        return CopyQueryResults<SoftBodyInverseBind>(
            [this](const AZStd::span<SoftBodyInverseBind> values)
            {
                return QueryDefinitionInverseBinds(values);
            });
    }

    AZStd::vector<SoftBodyLongRangeConstraint> SoftBodyComponent::CopyDefinitionLongRangeConstraints() const
    {
        return CopyQueryResults<SoftBodyLongRangeConstraint>(
            [this](const AZStd::span<SoftBodyLongRangeConstraint> values)
            {
                return QueryDefinitionLongRangeConstraints(values);
            });
    }

    AZStd::vector<MaterialHandle> SoftBodyComponent::CopyDefinitionMaterials() const
    {
        return CopyQueryResults<MaterialHandle>(
            [this](const AZStd::span<MaterialHandle> values)
            {
                return QueryDefinitionMaterials(values);
            });
    }

    AZStd::vector<SoftBodyRodBendTwistConstraint> SoftBodyComponent::CopyDefinitionRodBendTwistConstraints() const
    {
        return CopyQueryResults<SoftBodyRodBendTwistConstraint>(
            [this](const AZStd::span<SoftBodyRodBendTwistConstraint> values)
            {
                return QueryDefinitionRodBendTwistConstraints(values);
            });
    }

    AZStd::vector<SoftBodyRodStretchShearConstraint> SoftBodyComponent::CopyDefinitionRodStretchShearConstraints() const
    {
        return CopyQueryResults<SoftBodyRodStretchShearConstraint>(
            [this](const AZStd::span<SoftBodyRodStretchShearConstraint> values)
            {
                return QueryDefinitionRodStretchShearConstraints(values);
            });
    }

    AZStd::vector<SoftBodySkinConstraint> SoftBodyComponent::CopyDefinitionSkinConstraints() const
    {
        return CopyQueryResults<SoftBodySkinConstraint>(
            [this](const AZStd::span<SoftBodySkinConstraint> values)
            {
                return QueryDefinitionSkinConstraints(values);
            });
    }

    AZStd::vector<SoftBodyVertex> SoftBodyComponent::CopyDefinitionVertices() const
    {
        return CopyQueryResults<SoftBodyVertex>(
            [this](const AZStd::span<SoftBodyVertex> values)
            {
                return QueryDefinitionVertices(values);
            });
    }

    AZStd::vector<SoftBodyVolumeConstraint> SoftBodyComponent::CopyDefinitionVolumeConstraints() const
    {
        return CopyQueryResults<SoftBodyVolumeConstraint>(
            [this](const AZStd::span<SoftBodyVolumeConstraint> values)
            {
                return QueryDefinitionVolumeConstraints(values);
            });
    }

    bool SoftBodyComponent::GetInverseInertia(
        AZ::Matrix3x3& inverseInertia) const
    {
        return m_system
            && m_system->GetBodyInverseInertia(
                m_worldHandle,
                m_bodyHandle,
                inverseInertia);
    }

    bool SoftBodyComponent::GetInverseMass(
        float& inverseMass) const
    {
        return m_system
            && m_system->GetBodyInverseMass(
                m_worldHandle,
                m_bodyHandle,
                inverseMass);
    }

    bool SoftBodyComponent::GetRuntimeConfiguration(
        SoftBodyRuntimeConfiguration& configuration) const
    {
        return m_system
            && m_system->GetSoftBodyRuntimeConfiguration(
                m_worldHandle,
                m_bodyHandle,
                configuration);
    }

    bool SoftBodyComponent::UpdateRuntimeConfiguration(
        const SoftBodyRuntimeConfiguration& configuration)
    {
        if (!m_system || !m_system->UpdateSoftBodyRuntimeConfiguration(m_worldHandle, m_bodyHandle, configuration))
        {
            return false;
        }

        m_configuration->m_body.m_friction = configuration.m_friction;
        m_configuration->m_body.m_gravityFactor = configuration.m_gravityFactor;
        m_configuration->m_body.m_linearDamping = configuration.m_linearDamping;
        m_configuration->m_body.m_maximumLinearVelocity = configuration.m_maximumLinearVelocity;
        m_configuration->m_body.m_pressure = configuration.m_pressure;
        m_configuration->m_body.m_restitution = configuration.m_restitution;
        m_configuration->m_body.m_skinnedMaximumDistanceMultiplier =
            configuration.m_skinnedMaximumDistanceMultiplier;
        m_configuration->m_body.m_vertexRadius = configuration.m_vertexRadius;
        m_configuration->m_body.m_iterationCount = configuration.m_iterationCount;
        m_configuration->m_body.m_allowSleeping = configuration.m_allowSleeping;
        m_configuration->m_body.m_enableSkinConstraints = configuration.m_enableSkinConstraints;
        m_configuration->m_body.m_facesDoubleSided = configuration.m_facesDoubleSided;
        m_configuration->m_body.m_updatePosition = configuration.m_updatePosition;
        return true;
    }

    bool SoftBodyComponent::GetVolume(
        float& volume) const
    {
        return m_system
            && m_system->GetSoftBodyVolume(m_worldHandle, m_bodyHandle, volume);
    }

    bool SoftBodyComponent::GetLocalBounds(
        AZ::Aabb& bounds) const
    {
        return m_system
            && m_system->GetSoftBodyLocalBounds(m_worldHandle, m_bodyHandle, bounds);
    }

    bool SoftBodyComponent::ApplySkinPose(
        const AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
        const bool hardSkinAll)
    {
        return m_system
            && m_system->SkinSoftBody(
                m_worldHandle,
                m_bodyHandle,
                jointTransformsRelativeToCenterOfMass,
                hardSkinAll);
    }

    bool SoftBodyComponent::ApplySkinPoseFromTransforms(
        const AZStd::vector<AZ::Transform>& jointTransformsRelativeToCenterOfMass,
        const bool hardSkinAll)
    {
        return ApplySkinPose(jointTransformsRelativeToCenterOfMass, hardSkinAll);
    }

    bool SoftBodyComponent::RecalculateMassProperties(
        const bool activate)
    {
        return m_system
            && m_system->RecalculateSoftBodyMassProperties(
                m_worldHandle,
                m_bodyHandle,
                activate);
    }

    bool SoftBodyComponent::SetVertexInverseMass(
        const AZ::u32 vertexIndex,
        const float inverseMass)
    {
        return SetVertexInverseMasses(
            vertexIndex,
            AZStd::span<const float>(&inverseMass, 1));
    }

    bool SoftBodyComponent::SetVertexInverseMasses(
        const AZ::u32 startVertexIndex,
        const AZStd::span<const float> inverseMasses)
    {
        return m_system
            && m_system->SetSoftBodyVertexInverseMasses(
                m_worldHandle,
                m_bodyHandle,
                startVertexIndex,
                inverseMasses);
    }

    bool SoftBodyComponent::SetVertexInverseMassesFromValues(
        const AZ::u32 startVertexIndex,
        const AZStd::vector<float>& inverseMasses)
    {
        return SetVertexInverseMasses(
            startVertexIndex,
            AZStd::span<const float>(inverseMasses.data(), inverseMasses.size()));
    }

    bool SoftBodyComponent::SetVertexVelocity(
        const AZ::u32 vertexIndex,
        const AZ::Vector3& velocity)
    {
        return SetVertexVelocities(
            vertexIndex,
            AZStd::span<const AZ::Vector3>(&velocity, 1));
    }

    bool SoftBodyComponent::SetVertexVelocities(
        const AZ::u32 startVertexIndex,
        const AZStd::span<const AZ::Vector3> velocities)
    {
        return m_system
            && m_system->SetSoftBodyVertexVelocities(
                m_worldHandle,
                m_bodyHandle,
                startVertexIndex,
                velocities);
    }

    bool SoftBodyComponent::SetVertexVelocitiesFromVectors(
        const AZ::u32 startVertexIndex,
        const AZStd::vector<AZ::Vector3>& velocities)
    {
        return SetVertexVelocities(
            startVertexIndex,
            AZStd::span<const AZ::Vector3>(velocities.data(), velocities.size()));
    }

    bool SoftBodyComponent::UpdateManually(
        const float deltaTime)
    {
        if (!m_system
            || !m_configuration
            || !m_configuration->m_body.m_manualUpdate
            || !m_system->UpdateSoftBodyManually(m_worldHandle, m_bodyHandle, deltaTime))
        {
            return false;
        }

        BodyState state;
        if (!m_system->GetBodyState(m_worldHandle, m_bodyHandle, state))
        {
            return false;
        }

        m_syncingTransform = true;
        AZ::Transform transform = Internal::ToEntityTransform(state.m_transform, m_uniformScale);
        AZ::TransformBus::Event(
            GetEntityId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        m_syncingTransform = false;
        return true;
    }

    void SoftBodyComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<SoftBodyComponentConfiguration>(
                SoftBodyComponentConfiguration::CreateDefault());
        }

        m_system = GetRuntime();
        if (!m_system)
        {
            return;
        }
        m_worldHandle = m_system->GetDefaultWorldHandle();
        SoftBodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyRequestBus::Handler::BusConnect(GetEntityId());
        BodyNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        if (m_configuration->m_enabled)
        {
            [[maybe_unused]] const bool enabled = EnableSimulation();
        }
    }

    void SoftBodyComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        BodyNotificationBus::Handler::BusDisconnect();
        BodyRequestBus::Handler::BusDisconnect();
        SoftBodyRequestBus::Handler::BusDisconnect();
        [[maybe_unused]] const bool destroyed = DestroySimulation(true);
        AZ_Assert(destroyed, "Mandatory soft-body teardown must retain or release every native resource.");
        m_system = nullptr;
        if (!m_bodyHandle)
        {
            m_worldHandle = {};
        }
    }

    void SoftBodyComponent::OnBodyMoved(
        const BodyMoveEvent& event)
    {
        if (event.m_bodyHandle != m_bodyHandle || m_syncingTransform)
        {
            return;
        }

        m_syncingTransform = true;
        AZ::Transform transform = Internal::ToEntityTransform(event.m_transform, m_uniformScale);
        AZ::TransformBus::Event(
            GetEntityId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        m_syncingTransform = false;
    }

    void SoftBodyComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (!m_system || !m_bodyHandle || m_syncingTransform)
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

        [[maybe_unused]] const bool moved = m_system->SetBodyTransform(
            m_worldHandle,
            m_bodyHandle,
            Internal::ToWorldTransform(world),
            true);
    }

    void SoftBodyComponent::ReleaseDefinition()
    {
        if (m_system && m_definitionHandle)
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroySoftBodyDefinition(m_definitionHandle);
        }
        m_definitionHandle = {};
        if (m_system && m_materialHandles)
        {
            for (const MaterialHandle materialHandle : *m_materialHandles)
            {
                [[maybe_unused]] const bool destroyed = m_system->DestroyMaterial(materialHandle);
            }
        }
        m_materialHandles.reset();
    }
} // namespace Jolt
