/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Jobs/JobManagerDesc.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ::Test
{
    //! Restricts a benchmark process to one logical processor per physical core, favoring a shared cache domain.
    class ScopedBenchmarkCpuAffinity final
    {
    public:
        explicit ScopedBenchmarkCpuAffinity(AZ::u32 processorCount);
        ~ScopedBenchmarkCpuAffinity();

        AZ_DISABLE_COPY_MOVE(ScopedBenchmarkCpuAffinity);

        [[nodiscard]]
        AZ::u32 GetProcessorCount() const;

        [[nodiscard]]
        bool IsConstrained() const;

        [[nodiscard]]
        bool IsHighQualityOfService() const;

        void ConfigureJobManagerThreads(
            AZ::JobManagerDesc& descriptor,
            AZ::u32 workerCount) const;

    private:
        struct Implementation;
        AZStd::unique_ptr<Implementation> m_implementation;
    };

    //! Returns the process-wide CPU-affinity policy shared by all benchmark repetitions.
    [[nodiscard]]
    const ScopedBenchmarkCpuAffinity& GetBenchmarkCpuAffinity();
} // namespace AZ::Test
