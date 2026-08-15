/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/PathComponent.h>

#include <Jolt/System.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    class PathNotificationBusBehaviorHandler final
        : public PathNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            PathNotificationBusBehaviorHandler,
            "{E869771C-A9F2-491F-8C01-99E1D5DA0060}",
            AZ::SystemAllocator,
            OnPathCreated,
            OnPathDestroying,
            OnPathDestroyed);

        void OnPathCreated(
            const PathHandle pathHandle) override
        {
            Call(FN_OnPathCreated, pathHandle);
        }

        void OnPathDestroying(
            const PathHandle pathHandle) override
        {
            Call(FN_OnPathDestroying, pathHandle);
        }

        void OnPathDestroyed(
            const PathHandle pathHandle) override
        {
            Call(FN_OnPathDestroyed, pathHandle);
        }
    };

    PathComponent::PathComponent(
        HermitePathConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void PathComponent::Reflect(
        AZ::ReflectContext* context)
    {
        HermitePathConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<PathComponent, AZ::Component>()
                ->Field("Configuration", &PathComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<PathComponent>("Jolt Path", "Defines a Hermite path for Jolt constraints.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &PathComponent::m_configuration,
                        "Path",
                        "Ordered Hermite control points and loop state.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<HermitePathPoint>("HermitePathPoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "normal",
                    BehaviorValueGetter(&HermitePathPoint::m_normal),
                    nullptr)
                ->Property(
                    "position",
                    BehaviorValueGetter(&HermitePathPoint::m_position),
                    nullptr)
                ->Property(
                    "tangent",
                    BehaviorValueGetter(&HermitePathPoint::m_tangent),
                    nullptr);

            behaviorContext->Class<PathSample>("PathSample")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "position",
                    BehaviorValueGetter(&PathSample::m_position),
                    nullptr)
                ->Property(
                    "tangent",
                    BehaviorValueGetter(&PathSample::m_tangent),
                    nullptr)
                ->Property(
                    "normal",
                    BehaviorValueGetter(&PathSample::m_normal),
                    nullptr)
                ->Property(
                    "binormal",
                    BehaviorValueGetter(&PathSample::m_binormal),
                    nullptr)
                ->Property(
                    "fraction",
                    BehaviorValueGetter(&PathSample::m_fraction),
                    nullptr)
                ->Property("valid", BehaviorValueGetter(&PathSample::m_valid), nullptr);

            behaviorContext->Class<PathState>("PathState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property(
                    "maximumFraction",
                    BehaviorValueGetter(&PathState::m_maximumFraction),
                    nullptr)
                ->Property("looping", BehaviorValueGetter(&PathState::m_isLooping), nullptr);

            behaviorContext->EBus<PathRequestBus>("JoltPathRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("GetPathHandle", &IPathRequests::GetPathHandle)
                ->Event("CopyPoints", &IPathRequests::CopyPoints)
                ->Event("GetState", &IPathRequests::GetState)
                ->Event("Sample", &IPathRequests::Sample)
                ->Event("FindClosestPoint", &IPathRequests::FindClosestPoint);

            behaviorContext->EBus<PathNotificationBus>("JoltPathNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<PathNotificationBusBehaviorHandler>();

            behaviorContext->Class<PathComponent>("Jolt::PathComponent")
                ->RequestBus("JoltPathRequestBus");
        }
    }

    void PathComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltPathService"));
    }

    void PathComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltPathService"));
    }

    PathHandle PathComponent::GetPathHandle() const
    {
        return m_pathHandle;
    }

    const HermitePathConfiguration& PathComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    AZStd::vector<HermitePathPoint> PathComponent::CopyPoints() const
    {
        return m_configuration.m_points;
    }

    PathState PathComponent::GetState() const
    {
        PathState state;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded = m_system->GetPathState(m_pathHandle, state);
        }
        return state;
    }

    PathSample PathComponent::Sample(
        const float fraction) const
    {
        PathSample sample;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded = m_system->SamplePath(m_pathHandle, fraction, sample);
        }
        return sample;
    }

    PathSample PathComponent::FindClosestPoint(
        const AZ::Vector3& position,
        const float fractionHint) const
    {
        PathSample sample;
        if (m_system)
        {
            [[maybe_unused]] const bool succeeded =
                m_system->FindClosestPathPoint(m_pathHandle, position, fractionHint, sample);
        }
        return sample;
    }

    void PathComponent::Activate()
    {
        if (m_configuration.m_points.empty())
        {
            m_configuration = HermitePathConfiguration::CreateDefault();
        }

        m_system = AZ::Interface<ISystem>::Get();
        PathRequestBus::Handler::BusConnect(GetEntityId());
        if (!m_system)
        {
            return;
        }

        m_pathHandle = m_system->CreatePath(m_configuration);
        if (!m_pathHandle)
        {
            AZ_Warning(
                "Jolt",
                false,
                "Path component '%s' could not create a path from %zu points.",
                GetEntity()->GetName().c_str(),
                m_configuration.m_points.size());
            return;
        }

        PathNotificationBus::Event(
            GetEntityId(),
            &IPathNotifications::OnPathCreated,
            m_pathHandle);
    }

    void PathComponent::Deactivate()
    {
        if (m_system && m_pathHandle)
        {
            PathNotificationBus::Event(
                GetEntityId(),
                &IPathNotifications::OnPathDestroying,
                m_pathHandle);
            if (m_system->DestroyPath(m_pathHandle))
            {
                const PathHandle pathHandle = m_pathHandle;
                m_pathHandle = {};
                PathNotificationBus::Event(
                    GetEntityId(),
                    &IPathNotifications::OnPathDestroyed,
                    pathHandle);
            }
        }
        PathRequestBus::Handler::BusDisconnect();
        m_system = nullptr;
    }
} // namespace Jolt
