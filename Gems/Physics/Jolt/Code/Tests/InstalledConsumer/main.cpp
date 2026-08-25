/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Capabilities/Bodies.h>
#include <Jolt/Capabilities/Characters.h>
#include <Jolt/Capabilities/Paths.h>
#include <Jolt/Capabilities/Shapes.h>
#include <Jolt/Capabilities/WorldSimulation.h>
#include <Jolt/Capabilities/Worlds.h>
#include <Jolt/Character.h>
#include <Jolt/DebugDraw.h>
#include <Jolt/Diagnostics.h>
#include <Jolt/Event.h>
#include <Jolt/Path.h>
#include <Jolt/Query.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/System.h>
#include <Jolt/WorldBus.h>
#include <Jolt/WorldDiagnosticsBus.h>
#include <Jolt/WorldQueryBus.h>
#include <Jolt/WorldRollbackBus.h>
#include <Jolt/WorldSimulationBus.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace
{
    int VerifyExportedCallables()
    {
        Jolt::ReflectDebugDraw(nullptr);
        Jolt::ReflectDiagnostics(nullptr);
        Jolt::ReflectEvents(nullptr);
        Jolt::ReflectQueries(nullptr);
        Jolt::ReflectWorlds(nullptr);
        Jolt::ReflectWorldDiagnostics(nullptr);
        Jolt::ReflectWorldQueries(nullptr);
        Jolt::ReflectWorldRollback(nullptr);
        Jolt::ReflectWorldSimulation(nullptr);

        const Jolt::ShapeQueryFaceBuffers faceBuffers;
        if (!faceBuffers.GetQueryFace(0).empty() || !faceBuffers.GetTargetFace(0).empty())
        {
            return 1;
        }

        const Jolt::EventBatch emptyEventBatch;
        if (emptyEventBatch.GetId() != 0)
        {
            return 1;
        }

        return 0;
    }

    int RunSimulation()
    {
        if (VerifyExportedCallables() != 0)
        {
            return 1;
        }

        Jolt::SystemConfiguration systemConfiguration;
        systemConfiguration.m_defaultWorld.m_workerCount = 1;
        Jolt::System system(AZStd::move(systemConfiguration));
        if (!system)
        {
            return 2;
        }

        Jolt::Worlds* worlds = Jolt::Worlds::Get();
        Jolt::Shapes* shapes = Jolt::Shapes::Get();
        Jolt::Bodies* bodies = Jolt::Bodies::Get();
        Jolt::Characters* characters = Jolt::Characters::Get();
        Jolt::Paths* paths = Jolt::Paths::Get();
        Jolt::WorldSimulation* simulation = Jolt::WorldSimulation::Get();
        if (!worlds || !shapes || !bodies || !characters || !paths || !simulation)
        {
            return 3;
        }

        const Jolt::WorldHandle worldHandle = worlds->GetDefaultWorldHandle();
        if (!worldHandle)
        {
            return 4;
        }

        Jolt::ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = Jolt::SphereShapeConfiguration{.m_radius = 0.5f};
        const Jolt::ShapeHandle shapeHandle = shapes->CreateShape(worldHandle, shapeConfiguration);
        if (!shapeHandle)
        {
            return 5;
        }

        Jolt::BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const Jolt::BodyHandle bodyHandle = bodies->CreateBody(worldHandle, bodyConfiguration);
        if (!bodyHandle)
        {
            shapes->DestroyShape(worldHandle, shapeHandle);
            return 6;
        }

        if (!simulation->StepWorld(worldHandle, 1.0f / 60.0f))
        {
            bodies->DestroyBody(worldHandle, bodyHandle);
            shapes->DestroyShape(worldHandle, shapeHandle);
            return 7;
        }

        Jolt::BodyState bodyState;
        const bool moved = bodies->GetBodyState(worldHandle, bodyHandle, bodyState)
            && bodyState.m_transform.m_position.m_z < 2.0;
        const bool destroyedBody = bodies->DestroyBody(worldHandle, bodyHandle);
        if (!moved || !destroyedBody)
        {
            return 8;
        }

        Jolt::CharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = shapeHandle;
        characterConfiguration.m_transform.m_position.m_z = 2.0;
        const Jolt::CharacterHandle characterHandle = characters->CreateCharacter(worldHandle, characterConfiguration);
        Jolt::CharacterState characterState;
        if (!characterHandle
            || !characters->IsCharacterInSimulation(worldHandle, characterHandle)
            || !characters->GetCharacterState(worldHandle, characterHandle, characterState)
            || !characterState.m_isInSimulation
            || !characters->RemoveCharacterFromSimulation(worldHandle, characterHandle)
            || characters->IsCharacterInSimulation(worldHandle, characterHandle)
            || !characters->AddCharacterToSimulation(worldHandle, characterHandle, false)
            || !characters->IsCharacterInSimulation(worldHandle, characterHandle)
            || !characters->DestroyCharacter(worldHandle, characterHandle))
        {
            return 9;
        }

        if (!shapes->DestroyShape(worldHandle, shapeHandle))
        {
            return 10;
        }

        Jolt::HermitePathConfiguration pathConfiguration;
        pathConfiguration.m_points = {
            {.m_position = AZ::Vector3::CreateZero()},
            {.m_position = AZ::Vector3::CreateAxisX(4.0f)},
        };
        const AZ::Transform initialPathTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisY());
        const Jolt::PathHandle pathHandle = paths->CreatePath(pathConfiguration, initialPathTransform);
        if (!pathHandle)
        {
            return 11;
        }

        const AZ::Transform updatedPathTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(2.0f));
        Jolt::PathState pathState;
        if (!paths->QueuePathTransformUpdate(pathHandle, updatedPathTransform)
            || !simulation->StepWorld(worldHandle, 1.0f / 60.0f)
            || !paths->GetPathState(pathHandle, pathState)
            || !pathState.m_transform.IsClose(updatedPathTransform)
            || !paths->DestroyPath(pathHandle))
        {
            return 12;
        }

        return 0;
    }
} // namespace

int main()
{
    AZ::SystemAllocator systemAllocator;
    return RunSimulation();
}
