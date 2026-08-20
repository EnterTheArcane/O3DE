/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Hair.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Hair
    {
    public:
        [[nodiscard]]
        static Hair* Get();

        [[nodiscard]]
        HairDefinitionHandle CreateHairDefinition(
            const HairDefinitionConfiguration& configuration);

        bool DestroyHairDefinition(HairDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(HairDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetHairDefinitionState(
            HairDefinitionHandle definitionHandle,
            HairDefinitionState& state) const;

        [[nodiscard]]
        QueryResult GetHairNeutralDensity(
            HairDefinitionHandle definitionHandle,
            AZStd::span<float> density) const;

        bool SkinHairScalpVertices(
            HairDefinitionHandle definitionHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms,
            AZStd::span<AZ::Transform> preparedJointTransforms,
            AZStd::span<AZ::Vector3> scalpVertices) const;

        [[nodiscard]]
        HairHandle CreateHair(
            WorldHandle worldHandle,
            const HairConfiguration& configuration);

        bool DestroyHair(
            WorldHandle worldHandle,
            HairHandle hairHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            HairHandle hairHandle) const;

        bool SetHairTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const WorldTransform& worldTransform,
            bool teleport);

        bool SetHairScalpToHeadTransform(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& scalpToHeadTransform);

        bool UpdateHair(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            float deltaTime,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool EnableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const AZ::Transform& jointToHair,
            AZStd::span<const AZ::Transform> jointModelTransforms);

        bool DisableHairAutoUpdate(
            WorldHandle worldHandle,
            HairHandle hairHandle);

        [[nodiscard]]
        bool GetHairState(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            HairState& state) const;

        //! Reads all requested output streams with one compute synchronization.
        [[nodiscard]]
        bool GetHairReadback(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            const HairReadbackBuffers& buffers,
            HairReadbackResult& result) const;

        [[nodiscard]]
        QueryResult GetHairVertexStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairVertexState> states) const;

        [[nodiscard]]
        QueryResult GetHairRenderPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairScalpPositions(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<AZ::Vector3> positions) const;

        [[nodiscard]]
        QueryResult GetHairGridCellStates(
            WorldHandle worldHandle,
            HairHandle hairHandle,
            AZStd::span<HairGridCellState> states) const;

    private:
        friend class Runtime;

        Hair() = default;
        ~Hair() = default;

        static AZStd::atomic<Hair*> s_instance;
    };
} // namespace Jolt
