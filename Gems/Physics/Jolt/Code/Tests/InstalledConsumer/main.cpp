/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Capabilities/Bodies.h>
#include <Jolt/Capabilities/Shapes.h>
#include <Jolt/Capabilities/WorldSimulation.h>
#include <Jolt/Capabilities/Worlds.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/System.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace
{
    int RunSimulation()
    {
        Jolt::SystemConfiguration systemConfiguration;
        systemConfiguration.m_defaultWorld.m_workerCount = 1;
        Jolt::System system(AZStd::move(systemConfiguration));
        if (!system)
        {
            return 1;
        }

        Jolt::Worlds* worlds = Jolt::Worlds::Get();
        Jolt::Shapes* shapes = Jolt::Shapes::Get();
        Jolt::Bodies* bodies = Jolt::Bodies::Get();
        Jolt::WorldSimulation* simulation = Jolt::WorldSimulation::Get();
        if (!worlds || !shapes || !bodies || !simulation)
        {
            return 2;
        }

        const Jolt::WorldHandle worldHandle = worlds->GetDefaultWorldHandle();
        if (!worldHandle)
        {
            return 3;
        }

        Jolt::ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = Jolt::SphereShapeConfiguration{.m_radius = 0.5f};
        const Jolt::ShapeHandle shapeHandle = shapes->CreateShape(worldHandle, shapeConfiguration);
        if (!shapeHandle)
        {
            return 4;
        }

        Jolt::BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const Jolt::BodyHandle bodyHandle = bodies->CreateBody(worldHandle, bodyConfiguration);
        if (!bodyHandle)
        {
            shapes->DestroyShape(worldHandle, shapeHandle);
            return 5;
        }

        if (!simulation->StepWorld(worldHandle, 1.0f / 60.0f))
        {
            bodies->DestroyBody(worldHandle, bodyHandle);
            shapes->DestroyShape(worldHandle, shapeHandle);
            return 6;
        }

        Jolt::BodyState bodyState;
        const bool moved = bodies->GetBodyState(worldHandle, bodyHandle, bodyState)
            && bodyState.m_transform.m_position.m_z < 2.0;
        const bool destroyedBody = bodies->DestroyBody(worldHandle, bodyHandle);
        const bool destroyedShape = shapes->DestroyShape(worldHandle, shapeHandle);
        if (!moved || !destroyedBody || !destroyedShape)
        {
            return 7;
        }

        return 0;
    }
} // namespace

int main()
{
    AZ::SystemAllocator systemAllocator;
    return RunSimulation();
}
