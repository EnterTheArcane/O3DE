/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/PathBus.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>

namespace Jolt
{
    enum class ResourceDestructionPhase : AZ::u8;

    class RuntimeImplementation;

    class JOLT_API PathComponent final
        : public AZ::Component
        , public PathRequestBus::Handler
    {
    public:
        AZ_COMPONENT(PathComponent, PathComponentTypeId);

        PathComponent() = default;
        explicit PathComponent(HermitePathConfiguration configuration);
        ~PathComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        [[nodiscard]]
        PathHandle GetPathHandle() const override;

        [[nodiscard]]
        const HermitePathConfiguration& GetConfiguration() const override;

        [[nodiscard]]
        AZStd::vector<HermitePathPoint> CopyPoints() const override;

        [[nodiscard]]
        PathState GetState() const override;

        [[nodiscard]]
        PathSample Sample(float fraction) const override;

        [[nodiscard]]
        PathSample FindClosestPoint(
            const AZ::Vector3& position,
            float fractionHint) const override;

    private:
        void Activate() override;

        void Deactivate() override;

        bool DestroyPath(bool mandatory);

        static void NotifyResourceDestruction(
            void* context,
            AZ::EntityId entityId,
            AZ::ComponentId componentId,
            ResourceDestructionPhase phase);

        HermitePathConfiguration m_configuration = HermitePathConfiguration::CreateDefault();

        RuntimeImplementation* m_system = nullptr;
        PathHandle m_pathHandle;
    };
} // namespace Jolt
