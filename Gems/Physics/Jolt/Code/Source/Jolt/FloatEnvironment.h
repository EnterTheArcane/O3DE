/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Architecture.h>
#include <Jolt/Configuration.h>

#include <AzCore/base.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/exponential_backoff.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/shared_mutex.h>

#include <cfenv>
#include <cstddef>

namespace Jolt
{
    struct WorldLockStatistics final
    {
        AZ::u64 m_contentionCount = 0;
        AZ::u64 m_lockCount = 0;
        AZ::u64 m_maximumWaitNanoseconds = 0;
        AZ::u64 m_waitNanoseconds = 0;
    };

    class JOLT_API FloatEnvironment final
    {
    public:
        FloatEnvironment() = default;

        AZ_DISABLE_COPY_MOVE(FloatEnvironment);

        void Enter();

        void Leave();

    private:
        std::fenv_t m_previousEnvironment;
#if JOLT_ARCH_FAMILY_X86
        AZ::u32 m_previousControl;
#elif JOLT_ARCH_ARM64
        AZ::u64 m_previousControl;
#endif
#if JOLT_ARCH_ARM64
        AZ::u64 m_previousStatus;
#endif

        bool m_environmentCaptured;
        bool m_active = false;
    };

    class JOLT_API DeterministicFloatScope final
    {
    public:
        DeterministicFloatScope();
        ~DeterministicFloatScope();

        AZ_DISABLE_COPY_MOVE(DeterministicFloatScope);

    private:
        FloatEnvironment m_environment;
    };

    class JOLT_API DeterministicWorldMutex final
    {
    public:
        DeterministicWorldMutex() = default;

        AZ_DISABLE_COPY_MOVE(DeterministicWorldMutex);

        void lock();

        [[nodiscard]]
        bool try_lock();

        void unlock();

        void lock_shared();

        [[nodiscard]]
        bool try_lock_shared();

        void unlock_shared();

        void lock_simulation();

        void unlock_simulation();

        void ConfigureStatistics(bool enabled);

        [[nodiscard]]
        WorldLockStatistics GetStatistics(bool reset);

    private:
        void RecordLock(
            AZ::u64 waitNanoseconds,
            bool contended);

        AZStd::recursive_mutex m_operationMutex;
        AZStd::shared_mutex m_stateMutex;
        AZStd::atomic_uint32_t m_waitingWriterCount{0};
        AZStd::atomic_uint64_t m_contentionCount{0};
        AZStd::atomic_uint64_t m_lockCount{0};
        AZStd::atomic_uint64_t m_maximumWaitNanoseconds{0};
        AZStd::atomic_uint64_t m_waitNanoseconds{0};
        AZStd::atomic_bool m_collectStatistics{false};
        FloatEnvironment m_environment;
        size_t m_operationDepth = 0;
        size_t m_stateWriteDepth = 0;
    };

    class JOLT_API DeterministicWorldQueryLock final
    {
    public:
        explicit DeterministicWorldQueryLock(DeterministicWorldMutex& mutex);

        AZ_DISABLE_COPY_MOVE(DeterministicWorldQueryLock);

    private:
        DeterministicFloatScope m_floatScope;
        AZStd::shared_lock<DeterministicWorldMutex> m_lock;
    };

    class JOLT_API DeterministicSimulationLock final
    {
    public:
        explicit DeterministicSimulationLock(DeterministicWorldMutex& mutex);
        ~DeterministicSimulationLock();

        AZ_DISABLE_COPY_MOVE(DeterministicSimulationLock);

    private:
        DeterministicWorldMutex& m_mutex;
    };
} // namespace Jolt
