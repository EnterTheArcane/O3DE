/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Source/ArticulationLinkComponent.h>

#include <ArticulationUtils.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzFramework/Physics/SystemBus.h>
#include <PhysX/MathConversion.h>
#include <PhysX/PhysXLocks.h>
#include <PhysX/Utils.h>
#include <Scene/PhysXScene.h>
#include <System/PhysXSystem.h>

namespace PhysX
{
    // Definitions are put in .cpp so we can have AZStd::unique_ptr<T> member with forward declared T in the header
    // This causes AZStd::unique_ptr<T> ctor/dtor to be generated when full type info is available
    ArticulationLinkComponent::ArticulationLinkComponent()
    {
        InitPhysicsTickHandler();
    }

    ArticulationLinkComponent::~ArticulationLinkComponent() = default;

    ArticulationLinkComponent::ArticulationLinkComponent(ArticulationLinkConfiguration& config)
        : m_config(config)
    {
        InitPhysicsTickHandler();
    }

    void ArticulationLinkComponent::Reflect(AZ::ReflectContext* context)
    {
        ArticulationLinkData::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ArticulationLinkComponent, AZ::Component>()
                ->Version(1)
                ->Field("ArticulationLinkData", &ArticulationLinkComponent::m_articulationLinkData)
                ->Field("ArticulationLinkConfiguration", &ArticulationLinkComponent::m_config);
        }
    }

    void ArticulationLinkComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("PhysicsWorldBodyService"));
        provided.push_back(AZ_CRC_CE("PhysicsRigidBodyService"));
        provided.push_back(AZ_CRC_CE("PhysicsDynamicRigidBodyService"));
        provided.push_back(AZ_CRC_CE("ArticulationLinkService"));
    }

    void ArticulationLinkComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void ArticulationLinkComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("PhysicsRigidBodyService"));
    }

    void ArticulationLinkComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    const AZ::Entity* ArticulationLinkComponent::GetArticulationRootEntity() const
    {
        if (m_articulationRootEntityId.IsValid())
        {
            AZ::Entity* rootEntity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(rootEntity, &AZ::ComponentApplicationBus::Events::FindEntity, m_articulationRootEntityId);
            if (rootEntity)
            {
                return rootEntity;
            }
        }
        bool rootFound = false;
        AZ::Entity* currentEntity = GetEntity();
        while (!rootFound)
        {
            AZ::EntityId parentId = currentEntity->GetTransform()->GetParentId();
            if (!parentId.IsValid())
            {
                rootFound = true;
            }
            else
            {
                AZ::Entity* parentEntity = nullptr;
                AZ::ComponentApplicationBus::BroadcastResult(parentEntity, &AZ::ComponentApplicationBus::Events::FindEntity, parentId);

                if (parentEntity && parentEntity->FindComponent<ArticulationLinkComponent>())
                {
                    currentEntity = parentEntity;
                }
                else
                {
                    rootFound = true;
                }
            }
        }
        return currentEntity;
    }

    AZStd::vector<AzPhysics::SimulatedBodyHandle> ArticulationLinkComponent::GetSimulatedBodyHandles() const
    {
        return m_articulationLinks;
    }

    void ArticulationLinkComponent::Activate()
    {
        m_articulationRootEntityId = GetArticulationRootEntity()->GetId();
        m_offsetInCorrectUnits = m_config.HingePropertiesVisible() ? AZ::DegToRad(m_config.m_offset) : m_config.m_offset;
        auto* sceneInterface = AZ::Interface<AzPhysics::SceneInterface>::Get();
        if (!sceneInterface)
        {
            return;
        }
        Physics::DefaultWorldBus::BroadcastResult(m_attachedSceneHandle, &Physics::DefaultWorldRequests::GetDefaultSceneHandle);
        if (m_attachedSceneHandle == AzPhysics::InvalidSceneHandle)
        {
            AZ_Error("ArticulationLinkComponent", false, "Invalid Scene Handle");
            return;
        }

        // set the transform to not update when the parent's transform changes, to avoid conflict with physics transform updates
        GetEntity()->GetTransform()->SetOnParentChangedBehavior(AZ::OnParentChangedBehavior::DoNotUpdate);

        if (IsRootArticulation())
        {
            AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
            if (m_attachedSceneHandle != AzPhysics::InvalidSceneHandle)
            {
                sceneInterface->RegisterSceneSimulationFinishHandler(m_attachedSceneHandle, m_sceneFinishSimHandler);

                // Create a handler that in the case that the scene was removed before the deactivation of the component,
                // ensures that all articulations are destroyed.
                m_sceneRemovedHandler = AzPhysics::SystemEvents::OnSceneRemovedEvent::Handler(
                    [this](AzPhysics::SceneHandle sceneHandle)
                    {
                        if (sceneHandle == m_attachedSceneHandle && m_articulation)
                        {
                            DestroyArticulation();
                        }
                    });

                AZ::Interface<AzPhysics::SystemInterface>::Get()->RegisterSceneRemovedEvent(m_sceneRemovedHandler);

                CreateArticulation();
                m_link = GetArticulationLink(GetEntityId());
            }
        }
        else
        {
            // the articulation is owned by the entity which has the root link
            // if this entity is not the root of the articulation, cache a pointer to the PxArticulationLink corresponding to this entity
            // parents are guaranteed to activate before children, so we can go up the hierarchy to find the root
            const auto* articulationRootEntity = GetArticulationRootEntity();
            if (articulationRootEntity)
            {
                auto* rootArticulationLinkComponent = articulationRootEntity->FindComponent<ArticulationLinkComponent>();
                AZ_Assert(rootArticulationLinkComponent, "Articulation root has to have ArticulationLinkComponent");

                m_link = rootArticulationLinkComponent->GetArticulationLink(GetEntityId());
                AZ_Assert(m_link, "Scene not found for the root articulation link component");

                AzPhysics::Scene* scene = sceneInterface->GetScene(rootArticulationLinkComponent->m_attachedSceneHandle);
                AZ_Assert(scene, "Scene not found for the root articulation link component");

                auto* pxScene = static_cast<physx::PxScene*>(scene->GetNativePointer());
                if (m_link && pxScene)
                {
                    PHYSX_SCENE_READ_LOCK(pxScene);
                    m_driveJoint = m_link->getInboundJoint()->is<physx::PxArticulationJointReducedCoordinate>();
                }

            }
        }

        FillSimulatedBodyHandle();

        ArticulationJointRequestBus::Handler::BusConnect(GetEntityId());
        AzPhysics::SimulatedBodyComponentRequestsBus::Handler::BusConnect(GetEntityId());

        Physics::RigidBodyNotificationBus::Event(
            GetEntityId(), &Physics::RigidBodyNotificationBus::Events::OnPhysicsEnabled, GetEntityId());
    }

    void ArticulationLinkComponent::Deactivate()
    {
        AzPhysics::SimulatedBodyComponentRequestsBus::Handler::BusDisconnect();
        ArticulationJointRequestBus::Handler::BusDisconnect();

        if (IsRootArticulation())
        {
            m_sceneRemovedHandler.Disconnect();
            m_sceneFinishSimHandler.Disconnect();

            if (m_articulation)
            {
                DestroyArticulation();
            }

            AZ::TransformNotificationBus::Handler::BusDisconnect();
        }
        else
        {
            m_driveJoint = nullptr;
        }

        m_link = nullptr;
        m_articulationRootEntityId.SetInvalid();

        // set the behavior when the parent's transform changes back to default, since physics is no longer controlling the transform
        GetEntity()->GetTransform()->SetOnParentChangedBehavior(AZ::OnParentChangedBehavior::Update);

        Physics::RigidBodyNotificationBus::Event(
            GetEntityId(), &Physics::RigidBodyNotificationBus::Events::OnPhysicsDisabled, GetEntityId());
    }

    void ArticulationLinkComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local, [[maybe_unused]] const AZ::Transform& world)
    {
        if (m_enabled)
        {
            return;
        }
        AZ_Warning("ArticulationLinkComponent", IsRootArticulation(), "Pose can be adjusted only for the root articulation link.");
        if (m_articulation && IsRootArticulation())
        {
            physx::PxArticulationKinematicFlags kinematicFlag{};
            kinematicFlag.raise(physx::PxArticulationKinematicFlag::ePOSITION);
            PHYSX_SCENE_WRITE_LOCK(m_articulation->getScene());
            m_articulation->setRootGlobalPose(PxMathConvert(world));
            m_articulation->updateKinematic(kinematicFlag);
        }
    }

    void ArticulationLinkComponent::CreateArticulation()
    {
        AzPhysics::SceneInterface* sceneInterface = AZ::Interface<AzPhysics::SceneInterface>::Get();
        AZ_Assert(sceneInterface, "PhysX Scene Interface not found");
        if (!sceneInterface)
        {
            return;
        }

        physx::PxPhysics* pxPhysics = GetPhysXSystem()->GetPxPhysics();
        m_articulation = pxPhysics->createArticulationReducedCoordinate();

        const auto& rootLinkConfiguration = m_articulationLinkData->m_articulationLinkConfiguration;
        SetRootSpecificProperties(rootLinkConfiguration);

        CreateChildArticulationLinks(nullptr, *m_articulationLinkData.get());

        // Add articulation to the scene
        auto* scene = static_cast<PhysXScene*>(sceneInterface->GetScene(m_attachedSceneHandle));
        physx::PxScene* pxScene = static_cast<physx::PxScene*>(scene->GetNativePointer());

        PHYSX_SCENE_WRITE_LOCK(pxScene);
        pxScene->addArticulation(*m_articulation);

        // A single root link has no inbound joint to sample.
        if (m_articulation->getNbLinks() > 1)
        {
            m_articulationCache = m_articulation->createCache();
            AZ_Assert(m_articulationCache, "Failed to allocate articulation force cache");
            if (m_articulationCache)
            {
                scene->RegisterArticulationRoot(*this);
            }
        }
    }

    void ArticulationLinkComponent::SetRootSpecificProperties(const ArticulationLinkConfiguration& rootLinkConfiguration)
    {
        m_articulation->setSleepThreshold(rootLinkConfiguration.m_sleepMinEnergy);
        if (rootLinkConfiguration.m_startAsleep)
        {
            m_articulation->putToSleep();
        }

        physx::PxArticulationFlags articulationFlags(0);
        if (rootLinkConfiguration.m_isFixedBase)
        {
            articulationFlags.raise(physx::PxArticulationFlag::eFIX_BASE);
        }

        if (!rootLinkConfiguration.m_selfCollide)
        {
            // Disable collisions between the articulation's links (note that parent/child collisions
            // are disabled internally in either case).
            articulationFlags.raise(physx::PxArticulationFlag::eDISABLE_SELF_COLLISION);
        }

        m_articulation->setArticulationFlags(articulationFlags);

        m_articulation->setSolverIterationCounts(
            rootLinkConfiguration.m_solverPositionIterations, rootLinkConfiguration.m_solverVelocityIterations);
    }

    void ArticulationLinkComponent::CreateChildArticulationLinks(
        physx::PxArticulationLink* parentLink, const ArticulationLinkData& thisLinkData)
    {
        const ArticulationLinkConfiguration& articulationLinkConfiguration = thisLinkData.m_articulationLinkConfiguration;

        physx::PxTransform thisLinkTransform;
        if (parentLink)
        {
            physx::PxTransform parentLinkTransform = parentLink->getGlobalPose();
            physx::PxTransform thisLinkRelativeTransform = PxMathConvert(thisLinkData.m_localTransform);
            thisLinkTransform = parentLinkTransform * thisLinkRelativeTransform;
        }
        else
        {
            thisLinkTransform = PxMathConvert(GetEntity()->GetTransform()->GetWorldTM());
        }

        physx::PxArticulationLink* thisPxLink = m_articulation->createLink(parentLink, thisLinkTransform);
        if (!thisPxLink)
        {
            AZ_Error("PhysX", false, "Failed to create articulation link at root %s", GetEntity()->GetName().c_str());
            return;
        }

        AzPhysics::SimulatedBodyHandle articulationLinkHandle =
            AZ::Interface<AzPhysics::SceneInterface>::Get()->AddSimulatedBody(m_attachedSceneHandle, &articulationLinkConfiguration);
        if (articulationLinkHandle == AzPhysics::InvalidSimulatedBodyHandle)
        {
            AZ_Error(
                "PhysX", false, "Failed to create a simulated body for the articulation link at root %s", GetEntity()->GetName().c_str());
            return;
        }

        m_articulationLinks.emplace_back(articulationLinkHandle);

        AzPhysics::SimulatedBody* simulatedBody =
            AZ::Interface<AzPhysics::SceneInterface>::Get()->GetSimulatedBodyFromHandle(m_attachedSceneHandle, articulationLinkHandle);

        ArticulationLink* articulationLink = azrtti_cast<ArticulationLink*>(simulatedBody);
        articulationLink->SetPxArticulationLink(thisPxLink);
        articulationLink->SetupFromLinkData(thisLinkData);

        if (parentLink)
        {

            physx::PxArticulationJointReducedCoordinate* inboundJoint =
                thisPxLink->getInboundJoint()->is<physx::PxArticulationJointReducedCoordinate>();
            AZ::Transform offsetTransform {AZ::Transform::Identity()};
            // Sets the joint pose in the follower link actor frame.
            if (!AZ::IsClose(articulationLinkConfiguration.m_offset , 0.f))
            {
                if (articulationLinkConfiguration.m_articulationJointType == ArticulationJointType::Hinge)
                {
                    AZ_TracePrintf("PhysX", "Applying offset of %f deg to joint between %s and its parent link", articulationLinkConfiguration.m_offset, thisPxLink->getName());
                    offsetTransform.SetFromEulerDegrees(AZ::Vector3(articulationLinkConfiguration.m_offset,0,0));
                }
                else if (articulationLinkConfiguration.m_articulationJointType == ArticulationJointType::Prismatic)
                {
                    AZ_TracePrintf("PhysX", "Applying offset of %f meters to joint between %s and its parent link",  articulationLinkConfiguration.m_offset, thisPxLink->getName());
                    offsetTransform.SetTranslation(AZ::Vector3(articulationLinkConfiguration.m_offset,0,0));
                }
            }

            // Sets the joint pose in the lead link actor frame.
            inboundJoint->setParentPose(PxMathConvert(thisLinkData.m_jointLeadLocalFrame* offsetTransform));

            inboundJoint->setChildPose(PxMathConvert(thisLinkData.m_jointFollowerLocalFrame ) );
            const auto& friction = articulationLinkConfiguration.m_jointFrictionParameters;
            // Sets the joint type and limits.
            switch (articulationLinkConfiguration.m_articulationJointType)
            {
            case ArticulationJointType::Fix:
                inboundJoint->setJointType(physx::PxArticulationJointType::eFIX);
                break;
            case ArticulationJointType::Hinge:
                inboundJoint->setJointType(physx::PxArticulationJointType::eREVOLUTE);
                if (articulationLinkConfiguration.m_isLimited)
                {
                    // The lower limit should be strictly smaller than the higher limit.
                    physx::PxArticulationLimit limits;

                    limits.low = AZ::DegToRad( -articulationLinkConfiguration.m_offset + AZStd::min(
                        articulationLinkConfiguration.m_angularLimitNegative, articulationLinkConfiguration.m_angularLimitPositive));
                    limits.high = AZ::DegToRad( -articulationLinkConfiguration.m_offset +  AZStd::max(
                        articulationLinkConfiguration.m_angularLimitNegative, articulationLinkConfiguration.m_angularLimitPositive));

                    // From PhysX documentation: If the limits should be equal, use PxArticulationMotion::eLOCKED
                    if (AZ::IsClose(limits.low, limits.high, AZ::Constants::FloatEpsilon))
                    {
                        inboundJoint->setMotion(physx::PxArticulationAxis::eTWIST, physx::PxArticulationMotion::eLOCKED);
                    }
                    else
                    {
                        inboundJoint->setMotion(
                            physx::PxArticulationAxis::eTWIST, physx::PxArticulationMotion::eLIMITED); // limit the x rotation axis (eTWIST)
                    }

                    AZ_Warning(
                        "ArticulationLinkComponent",
                        (limits.low < 0.0 && limits.high > 0.0),
                        "The initial position of joint %s is outside joint limits, moving joint to avoid instability.",
                        thisPxLink->getName());
                    if (limits.low > 0.0 && limits.low + AZ::Constants::FloatEpsilon < limits.high)
                    {
                        inboundJoint->setJointPosition(physx::PxArticulationAxis::eTWIST, limits.low + AZ::Constants::FloatEpsilon);
                    }
                    else if (limits.high < 0.0 && limits.high - AZ::Constants::FloatEpsilon > limits.low)
                    {
                        inboundJoint->setJointPosition(physx::PxArticulationAxis::eTWIST, limits.high - AZ::Constants::FloatEpsilon);
                    }

                    inboundJoint->setLimitParams(physx::PxArticulationAxis::eTWIST, limits);
                }
                else
                {
                    inboundJoint->setMotion(
                        physx::PxArticulationAxis::eTWIST, physx::PxArticulationMotion::eFREE); // free on the x rotation axis (eTWIST)
                }
                if (articulationLinkConfiguration.m_motorConfiguration.m_useMotor)
                {
                    physx::PxArticulationDrive drive;
                    drive.driveType = physx::PxArticulationDriveType::eFORCE;
                    drive.envelope.maxEffort = articulationLinkConfiguration.m_motorConfiguration.m_driveForceLimit;
                    drive.damping = articulationLinkConfiguration.m_motorConfiguration.m_driveDamping;
                    drive.stiffness = articulationLinkConfiguration.m_motorConfiguration.m_driveStiffness;
                    inboundJoint->setDriveParams(physx::PxArticulationAxis::eTWIST, drive);
                }
                if (friction.IsValid())
                {
                    inboundJoint->setFrictionParams(
                        physx::PxArticulationAxis::eTWIST,
                        physx::PxJointFrictionParams(friction.m_staticFrictionEffort, friction.m_dynamicFrictionEffort, friction.m_viscousFrictionCoefficient));
                }
                else
                {
                    AZ_Error("PhysX", false, "Invalid articulation friction parameters.");
                }
                if (articulationLinkConfiguration.m_armature.GetX() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eTWIST, articulationLinkConfiguration.m_armature.GetX());
                }
                if (articulationLinkConfiguration.m_armature.GetY() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eSWING1, articulationLinkConfiguration.m_armature.GetY());
                }
                if (articulationLinkConfiguration.m_armature.GetZ() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eSWING2, articulationLinkConfiguration.m_armature.GetZ());
                }
                break;
            case ArticulationJointType::Prismatic:
                inboundJoint->setJointType(physx::PxArticulationJointType::ePRISMATIC);
                if (articulationLinkConfiguration.m_isLimited)
                {
                    // The lower limit should be strictly smaller than the higher limit.
                    physx::PxArticulationLimit limits;
                    limits.low = -articulationLinkConfiguration.m_offset +
                        AZStd::min(articulationLinkConfiguration.m_linearLimitLower, articulationLinkConfiguration.m_linearLimitUpper);
                    limits.high = -articulationLinkConfiguration.m_offset +
                        AZStd::max(articulationLinkConfiguration.m_linearLimitLower, articulationLinkConfiguration.m_linearLimitUpper);

                    // From PhysX documentation: If the limits should be equal, use PxArticulationMotion::eLOCKED
                    if (limits.low == limits.high)
                    {
                        inboundJoint->setMotion(physx::PxArticulationAxis::eX, physx::PxArticulationMotion::eLOCKED);
                    }
                    else
                    {
                        inboundJoint->setMotion(
                            physx::PxArticulationAxis::eX, physx::PxArticulationMotion::eLIMITED); // limit the x movement axis (eX)
                    }
                    inboundJoint->setLimitParams(physx::PxArticulationAxis::eX, limits);
                }
                else
                {
                    inboundJoint->setMotion(
                        physx::PxArticulationAxis::eX, physx::PxArticulationMotion::eFREE); // free on the x movement axis (eX)
                }
                if (articulationLinkConfiguration.m_motorConfiguration.m_useMotor)
                {
                    physx::PxArticulationDrive drive;
                    drive.driveType = physx::PxArticulationDriveType::eFORCE;
                    drive.envelope.maxEffort = articulationLinkConfiguration.m_motorConfiguration.m_driveForceLimit;
                    drive.damping = articulationLinkConfiguration.m_motorConfiguration.m_driveDamping;
                    drive.stiffness = articulationLinkConfiguration.m_motorConfiguration.m_driveStiffness;
                    inboundJoint->setDriveParams(physx::PxArticulationAxis::eX, drive);
                }
                if (friction.IsValid())
                {
                    inboundJoint->setFrictionParams(
                        physx::PxArticulationAxis::eX,
                        physx::PxJointFrictionParams(friction.m_staticFrictionEffort, friction.m_dynamicFrictionEffort, friction.m_viscousFrictionCoefficient));
                }
                else
                {
                    AZ_Error("PhysX", false, "Invalid articulation friction parameters.");
                }
                if (articulationLinkConfiguration.m_armature.GetX() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eX, articulationLinkConfiguration.m_armature.GetX());
                }
                if (articulationLinkConfiguration.m_armature.GetY() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eY, articulationLinkConfiguration.m_armature.GetY());
                }
                if (articulationLinkConfiguration.m_armature.GetZ() > AZ::Constants::FloatEpsilon)
                {
                    inboundJoint->setArmature(physx::PxArticulationAxis::eZ, articulationLinkConfiguration.m_armature.GetZ());
                }
                break;
            default:
                AZ_Error("ArticulationLinkComponent", false, "Unexpected articulation joint type.");
                break;
            }
        }

        articulationLinkConfiguration.WarnLegacySettings(articulationLinkConfiguration.m_debugName, articulationLinkConfiguration.m_entityId);

        m_articulationLinksByEntityId.insert(EntityIdArticulationLinkPair{ articulationLinkConfiguration.m_entityId, thisPxLink });

        for (const auto& childLink : thisLinkData.m_childLinks)
        {
            CreateChildArticulationLinks(thisPxLink, *childLink);
        }
    }

    void ArticulationLinkComponent::DestroyArticulation()
    {
        auto* scene = static_cast<PhysXScene*>(AZ::Interface<AzPhysics::SceneInterface>::Get()->GetScene(m_attachedSceneHandle));
        if (scene == nullptr)
        {
            // The scene can be removed before articulation is destroyed.
            // If the scene was removed. Articulations were also removed.
            return;
        }
        scene->RemoveSimulatedBodies(m_articulationLinks);
        m_articulationLinks.clear();

        physx::PxScene* pxScene = static_cast<physx::PxScene*>(scene->GetNativePointer());
        PHYSX_SCENE_WRITE_LOCK(pxScene);
        if (m_articulationCache)
        {
            scene->UnregisterArticulationRoot(*this);
            m_articulationCache->release();
            m_articulationCache = nullptr;
        }
        m_articulation->release();
        m_articulation = nullptr;
        m_articulationLinksByEntityId.clear();
        m_link = nullptr;
        m_driveJoint = nullptr;
    }

    void ArticulationLinkComponent::InitPhysicsTickHandler()
    {
        m_sceneFinishSimHandler = AzPhysics::SceneEvents::OnSceneSimulationFinishHandler(
            [this]([[maybe_unused]] AzPhysics::SceneHandle sceneHandle, float fixedDeltatime)
            {
                PostPhysicsTick(fixedDeltatime);
            },
            aznumeric_cast<int32_t>(AzPhysics::SceneEvents::PhysicsStartFinishSimulationPriority::Physics));
    }

    void ArticulationLinkComponent::CaptureArticulationForces()
    {
        m_articulation->copyInternalStateToCache(*m_articulationCache, physx::PxArticulationCacheFlag::eLINK_INCOMING_JOINT_FORCE);
    }

    void ArticulationLinkComponent::PostPhysicsTick([[maybe_unused]] float fixedDeltaTime)
    {
        AzPhysics::Scene* scene = AZ::Interface<AzPhysics::SceneInterface>::Get()->GetScene(m_attachedSceneHandle);
        physx::PxScene* pxScene = static_cast<physx::PxScene*>(scene->GetNativePointer());

        // The link poses are read under the scene lock, but the entity transforms are set after it is released:
        // transform change handlers may lock the scene for write, and PhysX can't upgrade a read lock.
        AZStd::fixed_vector<AZStd::pair<AZ::EntityId, AZ::Transform>, MaxArticulationLinks> linkTransforms;
        {
            PHYSX_SCENE_WRITE_LOCK(pxScene);

            if (!m_articulation)
            {
                return;
            }
            if (m_articulation->isSleeping())
            {
                return;
            }

            physx::PxArticulationLink* links[MaxArticulationLinks] = { nullptr };
            m_articulation->getLinks(links, MaxArticulationLinks);

            const physx::PxU32 linksNum = m_articulation->getNbLinks();
            AZ_Assert(
                linksNum <= MaxArticulationLinks,
                "Error. Number of articulation links %d is greater than the maximum supported %d",
                linksNum,
                MaxArticulationLinks);

            for (physx::PxU32 linkIndex = 0; linkIndex < linksNum; ++linkIndex)
            {
                physx::PxArticulationLink* link = links[linkIndex];
                ActorData* linkActorData = Utils::GetUserData(link);
                if (linkActorData)
                {
                    linkTransforms.emplace_back(linkActorData->GetEntityId(), PxMathConvert(link->getGlobalPose()));
                }
            }
        }

        for (const auto& [linkEntityId, globalTransform] : linkTransforms)
        {
            AZ::TransformBus::Event(linkEntityId, &AZ::TransformBus::Events::SetWorldTM, globalTransform);
        }
    }

    physx::PxArticulationLink* ArticulationLinkComponent::GetArticulationLink(const AZ::EntityId entityId)
    {
        if (const auto iterator = m_articulationLinksByEntityId.find(entityId); iterator != m_articulationLinksByEntityId.end())
        {
            return iterator->second;
        }
        else
        {
            return nullptr;
        }
    }

    const physx::PxArticulationJointReducedCoordinate* ArticulationLinkComponent::GetDriveJoint() const
    {
        [[maybe_unused]] const bool isRootArticulation = IsRootArticulation();
        AZ_ErrorOnce("Articulation Link Component", !isRootArticulation, "Articulation root does not have an inbound joint.");
        AZ_ErrorOnce("Articulation Link Component", m_driveJoint || IsRootArticulation(), "Invalid articulation joint pointer");
        return m_driveJoint;
    }

    physx::PxArticulationJointReducedCoordinate* ArticulationLinkComponent::GetDriveJoint()
    {
        PHYSX_SCENE_READ_LOCK(m_link->getScene());
        return const_cast<physx::PxArticulationJointReducedCoordinate*>(
            static_cast<const ArticulationLinkComponent&>(*this).GetDriveJoint());
    }

    void ArticulationLinkComponent::SetMotion(ArticulationJointAxis jointAxis, ArticulationJointMotionType jointMotionType)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            joint->setMotion(GetPxArticulationAxis(jointAxis), GetPxArticulationMotion(jointMotionType));
        }
    }

    ArticulationJointMotionType ArticulationLinkComponent::GetMotion(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return GetArticulationJointMotionType(joint->getMotion(GetPxArticulationAxis(jointAxis)));
        }
        return ArticulationJointMotionType::Locked;
    }

    void ArticulationLinkComponent::SetLimit(ArticulationJointAxis jointAxis, AZStd::pair<float, float> limitPair)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            const physx::PxArticulationLimit limit(limitPair.first - m_offsetInCorrectUnits, limitPair.second  - m_offsetInCorrectUnits);
            joint->setLimitParams(GetPxArticulationAxis(jointAxis), limit);
        }
    }

    AZStd::pair<float, float> ArticulationLinkComponent::GetLimit(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            const auto limit = joint->getLimitParams(GetPxArticulationAxis(jointAxis));
            return { limit.low + m_offsetInCorrectUnits, limit.high + m_offsetInCorrectUnits };
        }
        return { -AZ::Constants::FloatMax, AZ::Constants::FloatMax };
    }

    void ArticulationLinkComponent::SetDriveStiffness(ArticulationJointAxis jointAxis, float stiffness)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            const auto articulationAxis = GetPxArticulationAxis(jointAxis);
            auto driveParams = joint->getDriveParams(articulationAxis);
            driveParams.stiffness = stiffness;
            joint->setDriveParams(articulationAxis, driveParams);
        }
    }

    float ArticulationLinkComponent::GetDriveStiffness(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            auto driveParams = joint->getDriveParams(GetPxArticulationAxis(jointAxis));
            return driveParams.stiffness;
        }
        return AZ::Constants::FloatMax;
    }

    void ArticulationLinkComponent::SetDriveDamping(ArticulationJointAxis jointAxis, float damping)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            const auto articulationAxis = GetPxArticulationAxis(jointAxis);
            auto driveParams = joint->getDriveParams(articulationAxis);
            driveParams.damping = damping;
            joint->setDriveParams(articulationAxis, driveParams);
        }
    }

    float ArticulationLinkComponent::GetDriveDamping(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            auto driveParams = joint->getDriveParams(GetPxArticulationAxis(jointAxis));
            return driveParams.damping;
        }
        return AZ::Constants::FloatMax;
    }

    void ArticulationLinkComponent::SetMaxForce(ArticulationJointAxis jointAxis, float maxForce)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            const auto articulationAxis = GetPxArticulationAxis(jointAxis);
            auto driveParams = joint->getDriveParams(articulationAxis);
            driveParams.envelope.maxEffort = maxForce;
            joint->setDriveParams(articulationAxis, driveParams);
        }
    }

    float ArticulationLinkComponent::GetMaxForce(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            auto driveParams = joint->getDriveParams(GetPxArticulationAxis(jointAxis));
            return driveParams.envelope.maxEffort;
        }
        return AZ::Constants::FloatMax;
    }

    void ArticulationLinkComponent::SetIsAccelerationDrive(ArticulationJointAxis jointAxis, bool isAccelerationDrive)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            const auto articulationAxis = GetPxArticulationAxis(jointAxis);
            auto driveParams = joint->getDriveParams(articulationAxis);
            driveParams.driveType =
                isAccelerationDrive ? physx::PxArticulationDriveType::eACCELERATION : physx::PxArticulationDriveType::eFORCE;
            joint->setDriveParams(articulationAxis, driveParams);
        }
    }

    bool ArticulationLinkComponent::IsAccelerationDrive(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            auto driveParams = joint->getDriveParams(GetPxArticulationAxis(jointAxis));
            return driveParams.driveType == physx::PxArticulationDriveType::eACCELERATION;
        }
        return false;
    }

    void ArticulationLinkComponent::SetDriveTarget(ArticulationJointAxis jointAxis, float target)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            joint->setDriveTarget(GetPxArticulationAxis(jointAxis), target - m_offsetInCorrectUnits);
        }
    }

    float ArticulationLinkComponent::GetDriveTarget(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return joint->getDriveTarget(GetPxArticulationAxis(jointAxis) ) + m_offsetInCorrectUnits;
        }
        return 0.0f;
    }

    void ArticulationLinkComponent::SetDriveTargetVelocity(ArticulationJointAxis jointAxis, float targetVelocity)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            joint->setDriveVelocity(GetPxArticulationAxis(jointAxis), targetVelocity);
        }
    }

    float ArticulationLinkComponent::GetDriveTargetVelocity(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return joint->getDriveVelocity(GetPxArticulationAxis(jointAxis));
        }
        return 0.0f;
    }

    float ArticulationLinkComponent::GetJointPosition(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return joint->getJointPosition(GetPxArticulationAxis(jointAxis)) + m_offsetInCorrectUnits ;
        }
        return 0.0f;
    }

    float ArticulationLinkComponent::GetJointVelocity(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return joint->getJointVelocity(GetPxArticulationAxis(jointAxis));
        }
        return 0.0f;
    }

    void ArticulationLinkComponent::SetFrictionParameters(
        ArticulationJointAxis jointAxis,
        const ArticulationJointFrictionProperties& friction)
    {
        if (!friction.IsValid())
        {
            AZ_Error("PhysX", false, "Articulation friction parameters must be finite and nonnegative, with static effort >= dynamic effort.");
            return;
        }
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            joint->setFrictionParams(
                GetPxArticulationAxis(jointAxis),
                physx::PxJointFrictionParams(friction.m_staticFrictionEffort, friction.m_dynamicFrictionEffort, friction.m_viscousFrictionCoefficient));
        }
    }

    ArticulationJointFrictionProperties ArticulationLinkComponent::GetFrictionParameters(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            const auto friction = joint->getFrictionParams(GetPxArticulationAxis(jointAxis));
            return { friction.staticFrictionEffort, friction.dynamicFrictionEffort, friction.viscousFrictionCoefficient };
        }
        return {};
    }

    void ArticulationLinkComponent::SetMaxJointVelocity(ArticulationJointAxis jointAxis, float maxJointVelocity)
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
            joint->setMaxJointVelocity(GetPxArticulationAxis(jointAxis), maxJointVelocity);
        }
    }

    float ArticulationLinkComponent::GetMaxJointVelocity(ArticulationJointAxis jointAxis) const
    {
        if (auto* joint = GetDriveJoint())
        {
            PHYSX_SCENE_READ_LOCK(m_link->getScene());
            return joint->getMaxJointVelocity(GetPxArticulationAxis(jointAxis));
        }
        return 0.0f;
    }

    bool ArticulationLinkComponent::IsRootArticulation() const
    {
        if (m_articulationRootEntityId.IsValid())
        {
            return m_articulationRootEntityId == GetEntityId();
        }
        return IsRootArticulationEntity<ArticulationLinkComponent>(GetEntity());
    }

    physx::PxSpatialForce ArticulationLinkComponent::GetIncomingJointSpatialForce() const
    {
        const auto* rootEntity = GetArticulationRootEntity();
        const auto* root = rootEntity->FindComponent<ArticulationLinkComponent>();
        if (root && root->m_articulation && root->m_articulationCache)
        {
            PHYSX_SCENE_READ_LOCK(root->m_articulation->getScene());
            const auto link = root->m_articulationLinksByEntityId.find(GetEntityId());
            if (link != root->m_articulationLinksByEntityId.end())
            {
                return root->m_articulationCache->linkIncomingJointForce[link->second->getLinkIndex()];
            }
        }
        return { physx::PxVec3(0.0f), 0.0f, physx::PxVec3(0.0f), 0.0f };
    }

    AZ::Vector3 ArticulationLinkComponent::GetIncomingJointForce() const
    {
        return PxMathConvert(GetIncomingJointSpatialForce().force);
    }

    AZ::Vector3 ArticulationLinkComponent::GetIncomingJointTorque() const
    {
        return PxMathConvert(GetIncomingJointSpatialForce().torque);
    }

    const AzPhysics::SimulatedBody* ArticulationLinkComponent::GetSimulatedBodyConst() const
    {
        const AZ::Entity* rootEntity = GetArticulationRootEntity();
        const auto rootComponent = rootEntity->FindComponent<ArticulationLinkComponent>();

        return AZ::Interface<AzPhysics::SceneInterface>::Get()->GetSimulatedBodyFromHandle(
            rootComponent->m_attachedSceneHandle, GetSimulatedBodyHandle());
    }

    AzPhysics::SimulatedBody* ArticulationLinkComponent::GetSimulatedBody()
    {
        return const_cast<AzPhysics::SimulatedBody*>(GetSimulatedBodyConst());
    }

    AzPhysics::SimulatedBodyHandle ArticulationLinkComponent::GetSimulatedBodyHandle() const
    {
        return m_bodyHandle;
    }

    void ArticulationLinkComponent::FillSimulatedBodyHandle()
    {
        const AZ::Entity* rootEntity = GetArticulationRootEntity();
        AZ_Assert(rootEntity, "Articulation root entity is null");
        const auto rootComponent = rootEntity->FindComponent<ArticulationLinkComponent>();
        AZ_Assert(rootComponent, "Articulation root entity has not ArticulationLinkComponent");

        for (auto articulationHandle : rootComponent->GetSimulatedBodyHandles())
        {
            auto simulatedBody = AZ::Interface<AzPhysics::SceneInterface>::Get()->GetSimulatedBodyFromHandle(
                rootComponent->m_attachedSceneHandle, articulationHandle);
            if (simulatedBody)
            {
                if (simulatedBody->GetEntityId() == GetEntityId())
                {
                    m_bodyHandle = articulationHandle;
                    return;
                }
            }
            else
            {
                AZ_Error("ArticulationLinkComponent", false, "Failed to get simulated body from simulated body handle");
            }
        }

        AZ_Error("ArticulationLinkComponent", false, "No simulated body handle found");
    }

    void ArticulationLinkComponent::EnablePhysics()
    {
        if (m_enabled == true)
        {
            return;
        }
        m_enabled = true;
        PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
        m_link->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, false);
    }

    void ArticulationLinkComponent::DisablePhysics()
    {
        if (m_enabled == false)
        {
            return;
        }
        m_enabled = false;
        PHYSX_SCENE_WRITE_LOCK(m_link->getScene());
        m_link->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
    }

    bool ArticulationLinkComponent::IsPhysicsEnabled() const
    {
        return m_enabled;
    }

    AZ::Aabb ArticulationLinkComponent::GetAabb() const
    {
        return GetSimulatedBodyConst()->GetAabb();
    }

    AzPhysics::SceneQueryHit ArticulationLinkComponent::RayCast(const AzPhysics::RayCastRequest& request)
    {
        return GetSimulatedBody()->RayCast(request);
    }

} // namespace PhysX
