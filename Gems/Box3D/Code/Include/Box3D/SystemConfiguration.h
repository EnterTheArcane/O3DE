/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Events.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class JobContext;
    class ReflectContext;
} // namespace AZ

namespace Box3D
{
    using SurfaceTypeId = AZ::u64;
    using MaterialMixCallback = float (*)(float valueA, SurfaceTypeId surfaceTypeA, float valueB, SurfaceTypeId surfaceTypeB);

    //! Native solver settings shared by worlds created by this system.
    struct SystemConfiguration final
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO(SystemConfiguration, SystemConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static constexpr AZ::u32 DefaultWorkerCount = 4;

        AZ::u32 m_subStepCount = 4;
        AZ::u32 m_workerCount = DefaultWorkerCount;
        float m_contactHertz = 30.0f;
        float m_contactDampingRatio = 10.0f;
        float m_contactSpeed = 3.0f;
        float m_contactRecycleDistance = 0.05f;
        float m_restitutionThreshold = 1.0f;
        float m_hitEventThreshold = 1.0f;
        float m_maximumLinearSpeed = 400.0f;
        float m_lengthUnitsPerMeter = 1.0f;
        float m_stallWarningThresholdSeconds = (AZStd::numeric_limits<float>::max)();
        AZ::u32 m_staticShapeCapacity = 0;
        AZ::u32 m_dynamicShapeCapacity = 0;
        AZ::u32 m_staticBodyCapacity = 0;
        AZ::u32 m_dynamicBodyCapacity = 0;
        AZ::u32 m_contactCapacity = 0;
        MaterialMixCallback m_frictionCallback = nullptr;
        MaterialMixCallback m_restitutionCallback = nullptr;
        bool m_enableSleep = true;
        bool m_enableContinuous = true;
        bool m_enableWarmStarting = true;
        bool m_enableSpeculative = true;

        bool operator==(const SystemConfiguration& other) const;
        bool operator!=(const SystemConfiguration& other) const;
    };

    //! Independent world identity, timing, gravity, and scheduling policy.
    struct WorldConfiguration final
    {
        AZ_TYPE_INFO(WorldConfiguration, "{430CC917-BD5E-4EB7-89AB-744BD395B163}");

        AZ::Name m_name;
        AZ::Vector3 m_gravity = AZ::Vector3(0.0f, 0.0f, -9.81f);
        AZ::JobContext* m_jobContext = nullptr;
        float m_fixedTimeStep = 1.0f / 60.0f;
        AZ::u32 m_maximumCatchUpSteps = 4;
        StepEventTypes m_collectedEventTypes = StepEventTypes::All;
        bool m_autoSimulate = true;
        bool m_enabled = true;
    };
} // namespace Box3D
