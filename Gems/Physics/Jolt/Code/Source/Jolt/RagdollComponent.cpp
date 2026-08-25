/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/RagdollComponent.h>

#include <Jolt/ComponentUtilities.h>
#include <Jolt/ShapeOwner.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        void ScalePosition(
            WorldPosition& position,
            const float scale)
        {
            position.m_x *= scale;
            position.m_y *= scale;
            position.m_z *= scale;
        }

        void ScaleConstraint(
            ConstraintGeometry& geometry,
            const float scale)
        {
            AZStd::visit(
                [scale](auto& configuration)
                {
                    using Configuration = AZStd::remove_cvref_t<decltype(configuration)>;
                    if constexpr (AZStd::is_same_v<Configuration, ConeConstraintConfiguration>
                        || AZStd::is_same_v<Configuration, FixedConstraintConfiguration>
                        || AZStd::is_same_v<Configuration, HingeConstraintConfiguration>
                        || AZStd::is_same_v<Configuration, PointConstraintConfiguration>
                        || AZStd::is_same_v<Configuration, SwingTwistConstraintConfiguration>)
                    {
                        ScalePosition(configuration.m_firstPoint, scale);
                        ScalePosition(configuration.m_secondPoint, scale);
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, DistanceConstraintConfiguration>)
                    {
                        ScalePosition(configuration.m_firstPoint, scale);
                        ScalePosition(configuration.m_secondPoint, scale);
                        if (configuration.m_minimumDistance >= 0.0f)
                        {
                            configuration.m_minimumDistance *= scale;
                            configuration.m_maximumDistance *= scale;
                        }
                    }
                    else if constexpr (AZStd::is_same_v<Configuration, SliderConstraintConfiguration>)
                    {
                        ScalePosition(configuration.m_firstPoint, scale);
                        ScalePosition(configuration.m_secondPoint, scale);
                        if (configuration.m_minimumLimit != -AZStd::numeric_limits<float>::max())
                        {
                            configuration.m_minimumLimit *= scale;
                        }
                        if (configuration.m_maximumLimit != AZStd::numeric_limits<float>::max())
                        {
                            configuration.m_maximumLimit *= scale;
                        }
                        configuration.m_targetPosition *= scale;
                        configuration.m_targetVelocity *= scale;
                    }
                },
                geometry);
        }

        BodyConfiguration CreateBodyConfiguration(
            const RagdollPartComponentConfiguration& part,
            const ShapeHandle shapeHandle,
            const AZ::Transform& modelTransform)
        {
            const BodyRuntimeConfiguration& runtime = part.m_body.m_runtime;
            BodyConfiguration body;
            body.m_shapeHandle = shapeHandle;
            body.m_transform = Internal::ToWorldTransform(modelTransform);
            body.m_linearVelocity = part.m_body.m_initialLinearVelocity;
            body.m_angularVelocity = part.m_body.m_initialAngularVelocity;
            body.m_objectLayer = part.m_body.m_objectLayer;
            body.m_allowedDofs = runtime.m_allowedDofs;
            body.m_massProperties = runtime.m_massProperties;
            body.m_motionQuality = runtime.m_motionQuality;
            body.m_motionType = part.m_motionType;
            body.m_friction = runtime.m_friction;
            body.m_restitution = runtime.m_restitution;
            body.m_linearDamping = runtime.m_linearDamping;
            body.m_angularDamping = runtime.m_angularDamping;
            body.m_maximumLinearVelocity = runtime.m_maximumLinearVelocity;
            body.m_maximumAngularVelocity = runtime.m_maximumAngularVelocity;
            body.m_gravityFactor = runtime.m_gravityFactor;
            body.m_velocityStepCount = runtime.m_velocityStepCount;
            body.m_positionStepCount = runtime.m_positionStepCount;
            body.m_allowDynamicOrKinematic = part.m_body.m_allowDynamicOrKinematic;
            body.m_allowSleeping = runtime.m_allowSleeping;
            body.m_applyGyroscopicForce = runtime.m_applyGyroscopicForce;
            body.m_collideKinematicVsNonDynamic = runtime.m_collideKinematicVsNonDynamic;
            body.m_enhancedInternalEdgeRemoval = runtime.m_enhancedInternalEdgeRemoval;
            body.m_isSensor = runtime.m_isSensor;
            body.m_useManifoldReduction = runtime.m_useManifoldReduction;
            return body;
        }

        class RagdollNotificationBusBehaviorHandler final
            : public RagdollNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(
                RagdollNotificationBusBehaviorHandler,
                "{342BC2AF-36FF-4B1A-98B8-9C047028B6D7}",
                AZ::SystemAllocator,
                OnRagdollCreated,
                OnRagdollDestroying,
                OnRagdollDestroyed);

            void OnRagdollCreated(
                const WorldHandle worldHandle,
                const RagdollHandle ragdollHandle) override
            {
                Call(FN_OnRagdollCreated, worldHandle, ragdollHandle);
            }

            void OnRagdollDestroying(
                const WorldHandle worldHandle,
                const RagdollHandle ragdollHandle) override
            {
                Call(FN_OnRagdollDestroying, worldHandle, ragdollHandle);
            }

            void OnRagdollDestroyed(
                const WorldHandle worldHandle,
                const RagdollHandle ragdollHandle) override
            {
                Call(FN_OnRagdollDestroyed, worldHandle, ragdollHandle);
            }
        };
    } // namespace

    struct RagdollComponent::RuntimeResources final
    {
        AZStd::vector<Internal::ShapeSet> m_partShapes;
        SkeletonDefinitionHandle m_skeletonHandle;
        RagdollDefinitionHandle m_definitionHandle;
    };

    RagdollComponent::RagdollComponent() = default;

    RagdollComponent::RagdollComponent(
        RagdollComponentConfiguration configuration)
        : m_configuration(AZStd::make_unique<RagdollComponentConfiguration>(AZStd::move(configuration)))
    {
    }

    RagdollComponent::~RagdollComponent() = default;

    void RagdollComponent::Reflect(
        AZ::ReflectContext* context)
    {
        RagdollComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->RegisterGenericType<AZStd::vector<AZ::Transform>>();
            serializeContext->RegisterGenericType<AZStd::vector<BodyHandle>>();
            serializeContext->RegisterGenericType<AZStd::vector<ConstraintHandle>>();

            serializeContext
                ->Class<RagdollComponent, AZ::Component>()
                ->Field("Configuration", &RagdollComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<RagdollComponent>("Jolt Ragdoll", "Simulates an authored articulated skeleton.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponent::m_configuration,
                        "Configuration",
                        "Skeleton, bodies, shapes, and constraints.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<RagdollState>("RagdollState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("rootTransform", BehaviorValueGetter(&RagdollState::m_rootTransform), nullptr)
                ->Property("bounds", BehaviorValueGetter(&RagdollState::m_bounds), nullptr)
                ->Property("definitionHandle", BehaviorValueGetter(&RagdollState::m_definitionHandle), nullptr)
                ->Property("entityId", BehaviorValueGetter(&RagdollState::m_entityId), nullptr)
                ->Property("name", BehaviorValueGetter(&RagdollState::m_name), nullptr)
                ->Property("bodyCount", BehaviorValueGetter(&RagdollState::m_bodyCount), nullptr)
                ->Property("collisionGroupId", BehaviorValueGetter(&RagdollState::m_collisionGroupId), nullptr)
                ->Property("constraintCount", BehaviorValueGetter(&RagdollState::m_constraintCount), nullptr)
                ->Property("isActive", BehaviorValueGetter(&RagdollState::m_isActive), nullptr)
                ->Property("isInSimulation", BehaviorValueGetter(&RagdollState::m_isInSimulation), nullptr);

            behaviorContext->EBus<RagdollRequestBus>("JoltRagdollRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("EnableSimulation", &IRagdollRequests::EnableSimulation)
                ->Event("DisableSimulation", &IRagdollRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &IRagdollRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &IRagdollRequests::GetWorldHandle)
                ->Event("GetRagdollHandle", &IRagdollRequests::GetRagdollHandle)
                ->Event("GetState", &IRagdollRequests::GetState)
                ->Event("CopyBodies", &IRagdollRequests::CopyBodies)
                ->Event("CopyConstraints", &IRagdollRequests::CopyConstraints)
                ->Event("CopyPose", &IRagdollRequests::CopyPose)
                ->Event("SetPose", &IRagdollRequests::SetPoseFromTransforms)
                ->Event("DriveKinematically", &IRagdollRequests::DriveKinematicallyFromTransforms)
                ->Event("DriveMotors", &IRagdollRequests::DriveMotorsFromTransforms)
                ->Event("DriveMotorsWithVelocity", &IRagdollRequests::DriveMotorsWithVelocityFromTransforms)
                ->Event("ResetWarmStart", &IRagdollRequests::ResetWarmStart)
                ->Event("ActivateRagdoll", &IRagdollRequests::ActivateRagdoll)
                ->Event("SetVelocity", &IRagdollRequests::SetVelocity)
                ->Event("SetLinearVelocity", &IRagdollRequests::SetLinearVelocity)
                ->Event("SetCollisionGroupId", &IRagdollRequests::SetCollisionGroupId)
                ->Event("AddLinearVelocity", &IRagdollRequests::AddLinearVelocity)
                ->Event("AddImpulse", &IRagdollRequests::AddImpulse);

            behaviorContext->EBus<RagdollNotificationBus>("JoltRagdollNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<RagdollNotificationBusBehaviorHandler>();

            behaviorContext->Class<RagdollComponent>("Jolt::RagdollComponent")
                ->RequestBus("JoltRagdollRequestBus");
        }
    }

    void RagdollComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltRagdollService"));
    }

    void RagdollComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltRagdollService"));
    }

    void RagdollComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    bool RagdollComponent::EnableSimulation()
    {
        if (m_ragdollHandle)
        {
            if (!m_system)
            {
                return false;
            }
            if (m_system->IsRagdollInSimulation(m_worldHandle, m_ragdollHandle))
            {
                return true;
            }

            return m_system->AddRagdollToSimulation(
                m_worldHandle,
                m_ragdollHandle,
                m_configuration->m_activate);
        }
        size_t partCount = 0;
        size_t jointCount = 0;
        if (m_configuration)
        {
            partCount = m_configuration->m_parts.size();
            jointCount = m_configuration->m_skeleton.m_joints.size();
        }
        if (!m_system
            || !m_configuration
            || partCount != jointCount)
        {
            AZ_Warning(
                "Jolt",
                false,
                "Ragdoll component '%s' has %zu parts and %zu skeleton joints.",
                GetEntity()->GetName().c_str(),
                partCount,
                jointCount);
            return false;
        }

        AZ::TransformBus::EventResult(
            m_entityTransform,
            GetEntityId(),
            &AZ::TransformInterface::GetWorldTM);
        const float uniformScale = m_entityTransform.GetUniformScale();
        if (!m_entityTransform.IsFinite() || uniformScale <= 0.0f)
        {
            return false;
        }

        m_resources = AZStd::make_unique<RuntimeResources>();
        m_resources->m_skeletonHandle = m_system->CreateSkeletonDefinition(m_configuration->m_skeleton);
        if (!m_resources->m_skeletonHandle)
        {
            AZ_Warning(
                "Jolt",
                false,
                "Ragdoll component '%s' could not create its skeleton.",
                GetEntity()->GetName().c_str());
            ReleaseResources();
            return false;
        }

        RagdollDefinitionConfiguration definition;
        definition.m_skeletonHandle = m_resources->m_skeletonHandle;
        definition.m_baseConstraintPriority = m_configuration->m_baseConstraintPriority;
        definition.m_minimumCollisionSeparation = m_configuration->m_minimumCollisionSeparation * uniformScale;
        definition.m_calculateConstraintPriorities = m_configuration->m_calculateConstraintPriorities;
        definition.m_disableSelfCollisions = m_configuration->m_disableSelfCollisions;
        definition.m_stabilize = m_configuration->m_stabilize;
        definition.m_parts.reserve(m_configuration->m_parts.size());
        m_resources->m_partShapes.resize(m_configuration->m_parts.size());

        const AZ::Quaternion entityRotation = m_entityTransform.GetRotation();
        const AZ::Transform entityRotationTransform = AZ::Transform::CreateFromQuaternion(entityRotation);
        for (size_t partIndex = 0; partIndex < m_configuration->m_parts.size(); ++partIndex)
        {
            const RagdollPartComponentConfiguration& source = m_configuration->m_parts[partIndex];
            Internal::ShapeSet& shapeSet = m_resources->m_partShapes[partIndex];
            if (!Internal::CreateShapeSet(
                    *m_system,
                    m_worldHandle,
                    source.m_shapes,
                    uniformScale,
                    shapeSet))
            {
                AZ_Warning(
                    "Jolt",
                    false,
                    "Ragdoll component '%s' could not create shapes for part %zu.",
                    GetEntity()->GetName().c_str(),
                    partIndex);
                ReleaseResources();
                return false;
            }

            AZ::Transform modelTransform = source.m_modelTransform;
            modelTransform.SetUniformScale(1.0f);
            modelTransform.SetTranslation(source.m_modelTransform.GetTranslation() * uniformScale);
            modelTransform = entityRotationTransform * modelTransform;

            RagdollPartConfiguration part;
            part.m_body = CreateBodyConfiguration(source, shapeSet.m_rootShapeHandle, modelTransform);
            if (source.m_toParent)
            {
                part.m_toParent = *source.m_toParent;
                ScaleConstraint(*part.m_toParent, uniformScale);
            }
            definition.m_parts.push_back(AZStd::move(part));
        }
        definition.m_additionalConstraints = m_configuration->m_additionalConstraints;
        for (AdditionalRagdollConstraint& constraint : definition.m_additionalConstraints)
        {
            ScaleConstraint(constraint.m_geometry, uniformScale);
        }

        m_resources->m_definitionHandle = m_system->CreateRagdollDefinition(m_worldHandle, definition);
        if (!m_resources->m_definitionHandle)
        {
            AZ_Warning(
                "Jolt",
                false,
                "Ragdoll component '%s' could not create its definition.",
                GetEntity()->GetName().c_str());
            ReleaseResources();
            return false;
        }

        RagdollConfiguration configuration;
        configuration.m_definitionHandle = m_resources->m_definitionHandle;
        configuration.m_rootPosition = Internal::ToWorldTransform(m_entityTransform).m_position;
        configuration.m_entityId = GetEntityId();
        configuration.m_name = AZ::Name(GetEntity()->GetName());
        configuration.m_collisionGroupId = m_configuration->m_collisionGroupId;
        configuration.m_activate = m_configuration->m_activate;
        m_ragdollHandle = m_system->CreateRagdoll(m_worldHandle, configuration);
        if (!m_ragdollHandle)
        {
            AZ_Warning(
                "Jolt",
                false,
                "Ragdoll component '%s' could not create its instance.",
                GetEntity()->GetName().c_str());
            ReleaseResources();
            return false;
        }

        AZStd::array<BodyHandle, 1> rootBody;
        if (QueryBodies(rootBody).m_hitCount != 1
            || !m_system->SetBodyMoveEventsEnabled(m_worldHandle, rootBody.front(), true))
        {
            [[maybe_unused]] const bool destroyed = m_system->DestroyRagdoll(m_worldHandle, m_ragdollHandle);
            m_ragdollHandle = {};
            ReleaseResources();
            return false;
        }
        m_rootBodyHandle = rootBody.front();
        RagdollNotificationBus::Event(
            GetEntityId(),
            &IRagdollNotifications::OnRagdollCreated,
            m_worldHandle,
            m_ragdollHandle);
        return true;
    }

    bool RagdollComponent::DisableSimulation()
    {
        if (!m_ragdollHandle)
        {
            return true;
        }
        if (!m_system)
        {
            return false;
        }
        if (!m_system->IsRagdollInSimulation(m_worldHandle, m_ragdollHandle))
        {
            return true;
        }

        return m_system->RemoveRagdollFromSimulation(m_worldHandle, m_ragdollHandle);
    }

    bool RagdollComponent::IsSimulationEnabled() const
    {
        return m_system
            && m_ragdollHandle
            && m_system->IsRagdollInSimulation(m_worldHandle, m_ragdollHandle);
    }

    WorldHandle RagdollComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    RagdollHandle RagdollComponent::GetRagdollHandle() const
    {
        return m_ragdollHandle;
    }

    RagdollState RagdollComponent::GetState() const
    {
        RagdollState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded =
                m_system->GetRagdollState(m_worldHandle, m_ragdollHandle, state);
        }
        return state;
    }

    QueryResult RagdollComponent::QueryBodies(
        const AZStd::span<BodyHandle> bodyHandles) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetRagdollBodies(m_worldHandle, m_ragdollHandle, bodyHandles);
    }

    QueryResult RagdollComponent::QueryConstraints(
        const AZStd::span<ConstraintHandle> constraintHandles) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetRagdollConstraints(m_worldHandle, m_ragdollHandle, constraintHandles);
    }

    QueryResult RagdollComponent::QueryPose(
        WorldPosition& rootPosition,
        const AZStd::span<AZ::Transform> modelTransforms) const
    {
        if (!m_system)
        {
            return {};
        }
        return m_system->GetRagdollPose(
            m_worldHandle,
            m_ragdollHandle,
            rootPosition,
            modelTransforms);
    }

    AZStd::vector<BodyHandle> RagdollComponent::CopyBodies() const
    {
        const QueryResult size = QueryBodies({});
        AZStd::vector<BodyHandle> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryBodies(result);
        return result;
    }

    AZStd::vector<ConstraintHandle> RagdollComponent::CopyConstraints() const
    {
        const QueryResult size = QueryConstraints({});
        AZStd::vector<ConstraintHandle> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryConstraints(result);
        return result;
    }

    AZStd::vector<AZ::Transform> RagdollComponent::CopyPose() const
    {
        WorldPosition rootPosition;
        const QueryResult size = QueryPose(rootPosition, {});
        AZStd::vector<AZ::Transform> result(size.m_requiredHitCount);
        [[maybe_unused]] const QueryResult query = QueryPose(rootPosition, result);
        return result;
    }

    bool RagdollComponent::SetPose(
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        return m_system
            && m_system->SetRagdollPose(m_worldHandle, m_ragdollHandle, rootPosition, modelTransforms);
    }

    bool RagdollComponent::SetPoseFromTransforms(
        const WorldPosition rootPosition,
        const AZStd::vector<AZ::Transform>& modelTransforms)
    {
        return SetPose(rootPosition, modelTransforms);
    }

    bool RagdollComponent::DriveKinematically(
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        return m_system
            && m_system->DriveRagdollKinematically(
                m_worldHandle,
                m_ragdollHandle,
                rootPosition,
                modelTransforms,
                deltaTime);
    }

    bool RagdollComponent::DriveKinematicallyFromTransforms(
        const WorldPosition rootPosition,
        const AZStd::vector<AZ::Transform>& modelTransforms,
        const float deltaTime)
    {
        return DriveKinematically(rootPosition, modelTransforms, deltaTime);
    }

    bool RagdollComponent::DriveMotors(
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        return m_system
            && m_system->DriveRagdollMotors(m_worldHandle, m_ragdollHandle, modelTransforms);
    }

    bool RagdollComponent::DriveMotorsFromTransforms(
        const AZStd::vector<AZ::Transform>& modelTransforms)
    {
        return DriveMotors(modelTransforms);
    }

    bool RagdollComponent::DriveMotorsWithVelocity(
        const AZStd::span<const AZ::Transform> previousModelTransforms,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        return m_system
            && m_system->DriveRagdollMotors(
                m_worldHandle,
                m_ragdollHandle,
                previousModelTransforms,
                modelTransforms,
                deltaTime);
    }

    bool RagdollComponent::DriveMotorsWithVelocityFromTransforms(
        const AZStd::vector<AZ::Transform>& previousModelTransforms,
        const AZStd::vector<AZ::Transform>& modelTransforms,
        const float deltaTime)
    {
        return DriveMotorsWithVelocity(previousModelTransforms, modelTransforms, deltaTime);
    }

    bool RagdollComponent::ResetWarmStart()
    {
        return m_system
            && m_system->ResetRagdollWarmStart(m_worldHandle, m_ragdollHandle);
    }

    bool RagdollComponent::ActivateRagdoll()
    {
        return m_system
            && m_system->ActivateRagdoll(m_worldHandle, m_ragdollHandle);
    }

    bool RagdollComponent::SetVelocity(
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        return m_system
            && m_system->SetRagdollVelocity(
                m_worldHandle,
                m_ragdollHandle,
                linearVelocity,
                angularVelocity);
    }

    bool RagdollComponent::SetLinearVelocity(
        const AZ::Vector3& linearVelocity)
    {
        return m_system
            && m_system->SetRagdollLinearVelocity(m_worldHandle, m_ragdollHandle, linearVelocity);
    }

    bool RagdollComponent::SetCollisionGroupId(
        const AZ::u32 collisionGroupId)
    {
        return m_system
            && m_system->SetRagdollCollisionGroupId(m_worldHandle, m_ragdollHandle, collisionGroupId);
    }

    bool RagdollComponent::AddLinearVelocity(
        const AZ::Vector3& linearVelocity)
    {
        return m_system
            && m_system->AddRagdollLinearVelocity(m_worldHandle, m_ragdollHandle, linearVelocity);
    }

    bool RagdollComponent::AddImpulse(
        const AZ::Vector3& impulse)
    {
        return m_system
            && m_system->AddRagdollImpulse(m_worldHandle, m_ragdollHandle, impulse);
    }

    void RagdollComponent::Activate()
    {
        if (!m_configuration)
        {
            m_configuration = AZStd::make_unique<RagdollComponentConfiguration>(
                RagdollComponentConfiguration::CreateDefault());
        }
        m_system = GetRuntime();
        if (!m_system)
        {
            return;
        }

        m_worldHandle = m_system->GetDefaultWorldHandle();
        RagdollRequestBus::Handler::BusConnect(GetEntityId());
        BodyNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        if (m_configuration->m_enabled)
        {
            [[maybe_unused]] const bool enabled = EnableSimulation();
        }
    }

    void RagdollComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        BodyNotificationBus::Handler::BusDisconnect();
        RagdollRequestBus::Handler::BusDisconnect();
        [[maybe_unused]] const bool destroyed = DestroyInstance();
        m_system = nullptr;
        m_worldHandle = {};
    }

    void RagdollComponent::OnBodyMoved(
        const BodyMoveEvent& event)
    {
        if (event.m_bodyHandle != m_rootBodyHandle || m_syncingTransform)
        {
            return;
        }

        const float uniformScale = m_entityTransform.GetUniformScale();
        const AZ::Transform transform = Internal::ToEntityTransform(event.m_transform, uniformScale);
        m_syncingTransform = true;
        AZ::TransformBus::Event(
            GetEntityId(),
            &AZ::TransformInterface::SetWorldTM,
            transform);
        m_syncingTransform = false;
        m_entityTransform = transform;
    }

    void RagdollComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (!m_system || !m_ragdollHandle || m_syncingTransform)
        {
            return;
        }
        if (!AZ::IsClose(world.GetUniformScale(), m_entityTransform.GetUniformScale(), AZ::Constants::Tolerance))
        {
            const bool wasEnabled = IsSimulationEnabled();
            if (!DestroyInstance())
            {
                return;
            }
            m_entityTransform = world;
            if (wasEnabled)
            {
                [[maybe_unused]] const bool enabled = EnableSimulation();
            }
            return;
        }

        WorldPosition rootPosition;
        AZStd::vector<AZ::Transform> pose = CopyPose();
        if (QueryPose(rootPosition, pose).m_hitCount != pose.size())
        {
            return;
        }
        const AZ::Vector3 translationDelta = world.GetTranslation() - m_entityTransform.GetTranslation();
        rootPosition.m_x += translationDelta.GetX();
        rootPosition.m_y += translationDelta.GetY();
        rootPosition.m_z += translationDelta.GetZ();

        const AZ::Quaternion rotationDelta =
            world.GetRotation() * m_entityTransform.GetRotation().GetConjugate();
        if (!rotationDelta.IsClose(AZ::Quaternion::CreateIdentity(), AZ::Constants::Tolerance))
        {
            const AZ::Transform rotationTransform = AZ::Transform::CreateFromQuaternion(rotationDelta);
            for (AZ::Transform& modelTransform : pose)
            {
                modelTransform = rotationTransform * modelTransform;
            }
        }
        if (SetPose(rootPosition, pose))
        {
            m_entityTransform = world;
        }
    }

    bool RagdollComponent::DestroyInstance()
    {
        if (!m_ragdollHandle)
        {
            ReleaseResources();
            return true;
        }

        const RagdollHandle ragdollHandle = m_ragdollHandle;
        RagdollNotificationBus::Event(
            GetEntityId(),
            &IRagdollNotifications::OnRagdollDestroying,
            m_worldHandle,
            ragdollHandle);
        if (!m_system || !m_system->DestroyRagdoll(m_worldHandle, ragdollHandle))
        {
            return false;
        }

        m_ragdollHandle = {};
        m_rootBodyHandle = {};
        ReleaseResources();
        RagdollNotificationBus::Event(
            GetEntityId(),
            &IRagdollNotifications::OnRagdollDestroyed,
            m_worldHandle,
            ragdollHandle);
        return true;
    }

    void RagdollComponent::ReleaseResources()
    {
        if (!m_resources || !m_system)
        {
            m_resources.reset();
            return;
        }
        if (m_resources->m_definitionHandle)
        {
            [[maybe_unused]] const bool destroyed =
                m_system->DestroyRagdollDefinition(m_worldHandle, m_resources->m_definitionHandle);
        }
        for (auto iterator = m_resources->m_partShapes.rbegin(); iterator != m_resources->m_partShapes.rend(); ++iterator)
        {
            m_system->DestroyOrDeferShapeSet(m_worldHandle, AZStd::move(*iterator));
        }
        if (m_resources->m_skeletonHandle)
        {
            [[maybe_unused]] const bool destroyed =
                m_system->DestroySkeletonDefinition(m_resources->m_skeletonHandle);
        }
        m_resources.reset();
    }
} // namespace Jolt
