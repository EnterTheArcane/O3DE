/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/HairBus.h>
#include <Jolt/HairComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class JOLT_API HairComponent final
        : public AZ::Component
        , public HairRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(HairComponent, HairComponentTypeId);

        HairComponent();
        explicit HairComponent(HairComponentConfiguration configuration);
        ~HairComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        HairHandle GetHairHandle() const override;

        [[nodiscard]]
        HairDefinitionState GetDefinitionState() const override;

        [[nodiscard]]
        HairState GetState() const override;

        bool SetScalpToHeadTransform(const AZ::Transform& scalpToHeadTransform) override;

        bool QueryReadback(
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const override;

        QueryResult QueryVertexStates(AZStd::span<HairVertexState> states) const override;

        QueryResult QueryRenderPositions(AZStd::span<AZ::Vector3> positions) const override;

        QueryResult QueryScalpPositions(AZStd::span<AZ::Vector3> positions) const override;

        QueryResult QueryGridCellStates(AZStd::span<HairGridCellState> states) const override;

        QueryResult QueryNeutralDensity(AZStd::span<float> density) const override;

        bool SkinScalpVertices(
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const override;

        [[nodiscard]]
        AZStd::vector<HairVertexState> CopyVertexStates() const override;

        [[nodiscard]]
        AZStd::vector<AZ::Vector3> CopyRenderPositions() const override;

        [[nodiscard]]
        AZStd::vector<AZ::Vector3> CopyScalpPositions() const override;

        [[nodiscard]]
        AZStd::vector<HairGridCellState> CopyGridCellStates() const override;

        [[nodiscard]]
        AZStd::vector<float> CopyNeutralDensity() const override;

        [[nodiscard]]
        AZStd::vector<AZ::Vector3> CopySkinnedScalpVertices(
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) const override;

        bool Update(
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) override;

        bool UpdateFromTransforms(
            float deltaTime,
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) override;

        bool EnableAutoUpdate(
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) override;

        bool EnableAutoUpdateFromTransforms(
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) override;

        bool DisableAutoUpdate() override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        void ReleaseDefinition();

        AZStd::unique_ptr<HairComponentConfiguration> m_configuration;

        RuntimeImplementation* m_system = nullptr;
        WorldHandle m_worldHandle;
        HairDefinitionHandle m_definitionHandle;
        HairHandle m_hairHandle;
        float m_uniformScale = 1.0f;
    };
} // namespace Jolt
