/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/TypeIds.h>
#include <Jolt/Vehicle.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>

namespace Jolt
{
    struct JOLT_API WheeledVehicleComponentConfiguration final
    {
        AZ_TYPE_INFO(WheeledVehicleComponentConfiguration, WheeledVehicleComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(WheeledVehicleComponentConfiguration, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static WheeledVehicleComponentConfiguration CreateDefault();

        WheeledVehicleConfiguration m_vehicle;
        bool m_enabled = true;
    };

    struct JOLT_API MotorcycleComponentConfiguration final
    {
        AZ_TYPE_INFO(MotorcycleComponentConfiguration, MotorcycleComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(MotorcycleComponentConfiguration, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static MotorcycleComponentConfiguration CreateDefault();

        MotorcycleConfiguration m_motorcycle;
        bool m_enabled = true;
    };

    struct JOLT_API TrackedVehicleComponentConfiguration final
    {
        AZ_TYPE_INFO(TrackedVehicleComponentConfiguration, TrackedVehicleComponentConfigurationTypeId);
        AZ_CLASS_ALLOCATOR(TrackedVehicleComponentConfiguration, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        static TrackedVehicleComponentConfiguration CreateDefault();

        TrackedVehicleConfiguration m_vehicle;
        bool m_enabled = true;
    };
} // namespace Jolt
