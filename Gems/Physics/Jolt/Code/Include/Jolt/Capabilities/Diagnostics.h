/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/DebugDraw.h>
#include <Jolt/Diagnostics.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Diagnostics
    {
    public:
        [[nodiscard]]
        static Diagnostics* Get();

        [[nodiscard]]
        bool GetWorldStatistics(
            WorldHandle worldHandle,
            WorldStatistics& statistics) const;

        //! Enables selected counters. Disabled categories add no clocks or counter updates to their hot paths.
        bool ConfigurePerformanceStatistics(
            WorldHandle worldHandle,
            PerformanceStatisticsFlags flags);

        //! Copies counters into caller-owned storage. Reset retains all allocated instrumentation storage.
        [[nodiscard]]
        bool GetPerformanceStatistics(
            WorldHandle worldHandle,
            WorldPerformanceStatistics& statistics,
            bool reset);

        //! Reads allocation-free native broadphase counters. Reset preserves native map capacity.
        [[nodiscard]]
        DiagnosticStatisticsResult GetBroadPhaseStatistics(
            WorldHandle worldHandle,
            AZStd::span<BroadPhaseStatistics> statistics,
            bool reset);

        //! Reads process-wide native narrowphase counters.
        [[nodiscard]]
        DiagnosticStatisticsResult GetNarrowPhaseStatistics(
            AZStd::span<NarrowPhaseStatistics> statistics,
            bool reset);

        bool DrawDebug(
            WorldHandle worldHandle,
            const DebugDrawSettings& settings,
            IDebugRenderer& renderer,
            const IDebugFilter* filter = nullptr);

        bool ConfigureDebugCapture(
            WorldHandle worldHandle,
            const DebugCaptureConfiguration& configuration);

        [[nodiscard]]
        bool GetDebugCaptureStatistics(
            WorldHandle worldHandle,
            DebugCaptureStatistics& statistics) const;

    private:
        friend class Runtime;

        Diagnostics() = default;
        ~Diagnostics() = default;

        static AZStd::atomic<Diagnostics*> s_instance;
    };
} // namespace Jolt
