/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Hair.h>
#include <Jolt/Query.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class IHairRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual HairHandle GetHairHandle() const = 0;

        [[nodiscard]]
        virtual HairDefinitionState GetDefinitionState() const = 0;

        [[nodiscard]]
        virtual HairState GetState() const = 0;

        virtual bool SetScalpToHeadTransform(const AZ::Transform& scalpToHeadTransform) = 0;

        virtual bool QueryReadback(
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const = 0;

        virtual QueryResult QueryVertexStates(AZStd::span<HairVertexState> states) const = 0;

        virtual QueryResult QueryRenderPositions(AZStd::span<AZ::Vector3> positions) const = 0;

        virtual QueryResult QueryScalpPositions(AZStd::span<AZ::Vector3> positions) const = 0;

        virtual QueryResult QueryGridCellStates(AZStd::span<HairGridCellState> states) const = 0;

        virtual QueryResult QueryNeutralDensity(AZStd::span<float> density) const = 0;

        virtual bool SkinScalpVertices(
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<HairVertexState> CopyVertexStates() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Vector3> CopyRenderPositions() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Vector3> CopyScalpPositions() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<HairGridCellState> CopyGridCellStates() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<float> CopyNeutralDensity() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Vector3> CopySkinnedScalpVertices(
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) const = 0;

        virtual bool Update(
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) = 0;

        virtual bool UpdateFromTransforms(
            float deltaTime,
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) = 0;

        virtual bool EnableAutoUpdate(
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms) = 0;

        virtual bool EnableAutoUpdateFromTransforms(
            const AZ::Transform& jointToHair,
            const AZStd::vector<AZ::Transform>& jointModelTransforms) = 0;

        virtual bool DisableAutoUpdate() = 0;
    };

    using HairRequestBus = AZ::EBus<IHairRequests>;

    class IHairNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnHairCreated(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] HairHandle hairHandle)
        {
        }

        virtual void OnHairDestroying(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] HairHandle hairHandle)
        {
        }

        virtual void OnHairDestroyed(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] HairHandle hairHandle)
        {
        }
    };

    using HairNotificationBus = AZ::EBus<IHairNotifications>;
} // namespace Jolt
