/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/HairComputeProvider.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

#include <AzTest/AzTest.h>

namespace Jolt::Tests
{
    class NameDictionaryScope final
    {
    public:
        NameDictionaryScope()
        {
            if (!AZ::Interface<AZ::NameDictionary>::Get())
            {
                AZ::NameDictionary::Create();
                m_created = true;
            }
        }

        ~NameDictionaryScope()
        {
            if (m_created)
            {
                AZ::NameDictionary::Destroy();
            }
        }

        AZ_DISABLE_COPY_MOVE(NameDictionaryScope);

    private:
        bool m_created = false;
    };

    class HairComputeProviderScope final
    {
    public:
        explicit HairComputeProviderScope(
            IHairComputeProvider& provider)
            : m_provider(provider)
        {
            AZ::Interface<IHairComputeProvider>::Register(&m_provider);
        }

        ~HairComputeProviderScope()
        {
            AZ::Interface<IHairComputeProvider>::Unregister(&m_provider);
        }

        AZ_DISABLE_COPY_MOVE(HairComputeProviderScope);

    private:
        IHairComputeProvider& m_provider;
    };

    [[nodiscard]]
    inline HairDefinitionHandle CreateMinimalHairDefinition(
        ISystem& system)
    {
        HairDefinitionConfiguration configuration;
        configuration.m_vertices = {
            {.m_position = AZ::Vector3::CreateAxisZ(), .m_inverseMass = 0.0f},
            {.m_position = AZ::Vector3::CreateAxisZ(0.5f)},
            {.m_position = AZ::Vector3::CreateZero()},
        };
        configuration.m_strands = {
            {.m_beginVertex = 0, .m_endVertex = 3, .m_materialIndex = 0},
        };
        configuration.m_materials.resize(1);
        configuration.m_materials[0].m_simulationStrandFraction = 1.0f;
        configuration.m_scalpVertices = {
            AZ::Vector3(-1.0f, -1.0f, 1.0f),
            AZ::Vector3(1.0f, -1.0f, 1.0f),
            AZ::Vector3(0.0f, 1.0f, 1.0f),
        };
        configuration.m_scalpTriangles = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        configuration.m_scalpInverseBindPoses = {
            AZ::Transform::CreateIdentity(),
        };
        configuration.m_scalpSkinWeights = {
            {.m_jointIndex = 0, .m_weight = 1.0f},
            {.m_jointIndex = 0, .m_weight = 1.0f},
            {.m_jointIndex = 0, .m_weight = 1.0f},
        };
        configuration.m_scalpSkinWeightsPerVertex = 1;
        configuration.m_gridSizeX = 2;
        configuration.m_gridSizeY = 2;
        configuration.m_gridSizeZ = 2;
        return system.CreateHairDefinition(configuration);
    }

    inline void SimulateAndValidateGpuHair(
        IHairComputeProvider& provider)
    {
        NameDictionaryScope nameDictionaryScope;
        HairComputeProviderScope providerScope(provider);
        SystemConfiguration configuration;
        configuration.m_defaultWorld.m_workerCount = 1;
        configuration.m_hairComputeBackend = HairComputeBackend::PlatformGpu;
        configuration.m_allowNondeterministicHair = true;
        System system(configuration, nullptr);
        ASSERT_TRUE(system);

        const HairDefinitionHandle definitionHandle = CreateMinimalHairDefinition(system);
        ASSERT_TRUE(definitionHandle);
        HairDefinitionState definitionState;
        ASSERT_TRUE(system.GetHairDefinitionState(definitionHandle, definitionState));

        HairConfiguration hairConfiguration;
        hairConfiguration.m_definitionHandle = definitionHandle;
        hairConfiguration.m_objectLayer = DefaultLayers::Moving;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const HairHandle hairHandle = system.CreateHair(worldHandle, hairConfiguration);
        ASSERT_TRUE(hairHandle);

        const AZStd::array jointModelTransforms{
            AZ::Transform::CreateIdentity(),
        };
        ASSERT_TRUE(system.UpdateHair(
            worldHandle,
            hairHandle,
            1.0f / 60.0f,
            AZ::Transform::CreateIdentity(),
            jointModelTransforms));

        AZStd::vector<HairVertexState> vertexStates(definitionState.m_simulationVertexCount);
        AZStd::vector<AZ::Vector3> renderPositions(definitionState.m_renderVertexCount);
        AZStd::vector<AZ::Vector3> scalpPositions(definitionState.m_scalpVertexCount);
        AZStd::vector<HairGridCellState> gridCells(definitionState.m_gridCellCount);
        HairReadbackResult readbackResult;
        ASSERT_TRUE(system.GetHairReadback(
            worldHandle,
            hairHandle,
            {
                .m_vertexStates = vertexStates,
                .m_renderPositions = renderPositions,
                .m_scalpPositions = scalpPositions,
                .m_gridCells = gridCells,
            },
            readbackResult));
        EXPECT_TRUE(readbackResult.m_vertexStates.IsComplete());
        EXPECT_TRUE(readbackResult.m_renderPositions.IsComplete());
        EXPECT_TRUE(readbackResult.m_scalpPositions.IsComplete());
        EXPECT_TRUE(readbackResult.m_gridCells.IsComplete());
        for (const HairVertexState& vertexState : vertexStates)
        {
            EXPECT_TRUE(vertexState.m_localPosition.IsFinite());
            EXPECT_TRUE(vertexState.m_localRotation.IsFinite());
            EXPECT_TRUE(vertexState.m_localLinearVelocity.IsFinite());
            EXPECT_TRUE(vertexState.m_localAngularVelocity.IsFinite());
        }

        EXPECT_TRUE(system.DestroyHair(worldHandle, hairHandle));
        EXPECT_TRUE(system.DestroyHairDefinition(definitionHandle));
    }
} // namespace Jolt::Tests
