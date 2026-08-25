/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/DebugDraw.h>
#include <Jolt/Diagnostics.h>

#include <AzCore/EBus/EBus.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    JOLT_API void ReflectWorldDiagnostics(AZ::ReflectContext* context);

    class IWorldDiagnosticsRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual ~IWorldDiagnosticsRequests() = default;

        [[nodiscard]]
        virtual bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const = 0;

        virtual bool ConfigurePerformanceStatistics(
            WorldHandle worldHandle,
            PerformanceStatisticsFlags flags) = 0;

        [[nodiscard]]
        virtual bool GetPerformanceStatistics(
            WorldHandle worldHandle,
            WorldPerformanceStatistics& statistics,
            bool reset) = 0;

        virtual bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const = 0;
    };

    using WorldDiagnosticsRequestBus = AZ::EBus<IWorldDiagnosticsRequests>;
} // namespace Jolt
