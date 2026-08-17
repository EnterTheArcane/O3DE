/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/FloatEnvironment.h>

#include <AzCore/std/chrono/chrono.h>

#if defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#define JOLT_HAS_X86_FLOAT_CONTROL 1
#define JOLT_HAS_ARM64_FLOAT_CONTROL 0
#elif defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#define JOLT_HAS_X86_FLOAT_CONTROL 1
#define JOLT_HAS_ARM64_FLOAT_CONTROL 0
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
#include <arm64intr.h>
#include <intrin.h>
#define JOLT_HAS_X86_FLOAT_CONTROL 0
#define JOLT_HAS_ARM64_FLOAT_CONTROL 1
#elif defined(__aarch64__) && defined(__clang__)
#include <arm_acle.h>
#define JOLT_HAS_X86_FLOAT_CONTROL 0
#define JOLT_HAS_ARM64_FLOAT_CONTROL 1
#elif defined(__aarch64__)
#define JOLT_HAS_X86_FLOAT_CONTROL 0
#define JOLT_HAS_ARM64_FLOAT_CONTROL 1
#else
#define JOLT_HAS_X86_FLOAT_CONTROL 0
#define JOLT_HAS_ARM64_FLOAT_CONTROL 0
#endif

#define JOLT_HAS_NATIVE_FLOAT_CONTROL (JOLT_HAS_X86_FLOAT_CONTROL || JOLT_HAS_ARM64_FLOAT_CONTROL)

namespace Jolt
{
    namespace
    {
#if JOLT_HAS_X86_FLOAT_CONTROL
        constexpr AZ::u64 DefaultFloatControl = 0x1f80;
        constexpr AZ::u64 FloatControlModeMask = 0xffc0;
        constexpr AZ::u64 FloatStatusMask = 0x3f;

#if defined(__i386__) || defined(__x86_64__)
        [[nodiscard]]
        AZ::u16 ReadX87Status()
        {
            AZ::u16 status = 0;
            __asm__ volatile("fnstsw %0" : "=am"(status));
            return status;
        }

        void ClearX87Status()
        {
            __asm__ volatile("fnclex");
        }
#endif

        [[nodiscard]]
        AZ::u64 ReadFloatControl()
        {
            return _mm_getcsr();
        }

        void WriteFloatControl(
            const AZ::u64 control)
        {
            _mm_setcsr(static_cast<AZ::u32>(control));
        }
#elif JOLT_HAS_ARM64_FLOAT_CONTROL
        constexpr AZ::u64 DefaultFloatControl = 0;
        constexpr AZ::u64 Arm64FormatAndRoundingMask = AZ::u64{0x7ff} << 16;
        constexpr AZ::u64 Arm64ExceptionAndBFloatMask = (AZ::u64{1} << 15) | (AZ::u64{1} << 13) | (AZ::u64{0x1f} << 8);
        constexpr AZ::u64 Arm64AlternativeBehaviorMask = AZ::u64{0x7};
        constexpr AZ::u64 FloatControlModeMask =
            Arm64FormatAndRoundingMask | Arm64ExceptionAndBFloatMask | Arm64AlternativeBehaviorMask;

        [[nodiscard]]
        AZ::u64 ReadFloatControl()
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            return static_cast<AZ::u64>(_ReadStatusReg(ARM64_FPCR));
#elif defined(__clang__)
            return __arm_rsr64("fpcr");
#else
            AZ::u64 control = 0;
            __asm__ volatile("mrs %0, fpcr" : "=r"(control));
            return control;
#endif
        }

        [[nodiscard]]
        AZ::u64 ReadFloatStatus()
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            return static_cast<AZ::u64>(_ReadStatusReg(ARM64_FPSR));
#elif defined(__clang__)
            return __arm_rsr64("fpsr");
#else
            AZ::u64 status = 0;
            __asm__ volatile("mrs %0, fpsr" : "=r"(status));
            return status;
#endif
        }

        void WriteFloatControl(
            AZ::u64 control)
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            _WriteStatusReg(ARM64_FPCR, static_cast<__int64>(control));
#elif defined(__clang__)
            __arm_wsr64("fpcr", control);
#else
            __asm__ volatile("msr fpcr, %0" : : "r"(control));
#endif
        }

        void WriteFloatStatus(
            const AZ::u64 status)
        {
#if defined(_M_ARM64) || defined(_M_ARM64EC)
            _WriteStatusReg(ARM64_FPSR, static_cast<__int64>(status));
#elif defined(__clang__)
            __arm_wsr64("fpsr", status);
#else
            __asm__ volatile("msr fpsr, %0" : : "r"(status));
#endif
        }
#endif
    } // namespace

    void FloatEnvironment::Enter()
    {
#if JOLT_HAS_NATIVE_FLOAT_CONTROL
        const AZ::u64 currentControl = ReadFloatControl();
        bool controlIsCanonical = false;
#if defined(_M_X64) || defined(__x86_64__)
        controlIsCanonical =
            (currentControl & FloatControlModeMask) == DefaultFloatControl;
#if defined(__x86_64__)
        controlIsCanonical = controlIsCanonical
            && (ReadX87Status() & FloatStatusMask) == 0;
#endif
#elif JOLT_HAS_ARM64_FLOAT_CONTROL
        controlIsCanonical = std::fegetround() == FE_TONEAREST
            && (currentControl & FloatControlModeMask) == DefaultFloatControl;
#endif
        if (controlIsCanonical)
        {
            m_previousControl = currentControl;
#if defined(_M_X64) || defined(__x86_64__)
            if ((currentControl & FloatStatusMask) != 0)
            {
                WriteFloatControl(DefaultFloatControl);
            }
#endif
#if JOLT_HAS_ARM64_FLOAT_CONTROL
            m_previousStatus = ReadFloatStatus();
#endif
            m_active = true;
            return;
        }

        m_previousControl = currentControl;
#endif
        m_environmentCaptured = std::fegetenv(&m_previousEnvironment) == 0;
        std::fesetenv(FE_DFL_ENV);
        std::fesetround(FE_TONEAREST);
#if JOLT_HAS_X86_FLOAT_CONTROL
        WriteFloatControl(DefaultFloatControl);
#elif JOLT_HAS_ARM64_FLOAT_CONTROL
        WriteFloatControl(ReadFloatControl() & ~FloatControlModeMask);
#endif
        m_active = true;
    }

    void FloatEnvironment::Leave()
    {
        if (!m_active)
        {
            return;
        }

        if (m_environmentCaptured)
        {
            std::fesetenv(&m_previousEnvironment);
        }
#if JOLT_HAS_NATIVE_FLOAT_CONTROL
        if (ReadFloatControl() != m_previousControl)
        {
            WriteFloatControl(m_previousControl);
        }
#if defined(__i386__) || defined(__x86_64__)
        if (!m_environmentCaptured
            && (ReadX87Status() & FloatStatusMask) != 0)
        {
            ClearX87Status();
        }
#endif
#if JOLT_HAS_ARM64_FLOAT_CONTROL
        if (ReadFloatStatus() != m_previousStatus)
        {
            WriteFloatStatus(m_previousStatus);
        }
#endif
#endif
        m_environmentCaptured = false;
        m_active = false;
    }

    DeterministicFloatScope::DeterministicFloatScope()
    {
        m_environment.Enter();
    }

    DeterministicFloatScope::~DeterministicFloatScope()
    {
        m_environment.Leave();
    }

    void DeterministicWorldMutex::lock()
    {
        lock_simulation();
        if (m_stateWriteDepth == 0)
        {
            m_waitingWriterCount.fetch_add(1, AZStd::memory_order_acq_rel);
            if (m_collectStatistics.load(AZStd::memory_order_relaxed))
            {
                const auto startTime = AZStd::chrono::steady_clock::now();
                const bool acquiredWithoutWaiting = m_stateMutex.try_lock();
                if (!acquiredWithoutWaiting)
                {
                    m_stateMutex.lock();
                }
                const AZ::u64 waitNanoseconds = static_cast<AZ::u64>(
                    AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(
                        AZStd::chrono::steady_clock::now() - startTime)
                        .count());
                RecordLock(waitNanoseconds, !acquiredWithoutWaiting);
            }
            else
            {
                m_stateMutex.lock();
            }
            m_waitingWriterCount.fetch_sub(1, AZStd::memory_order_release);
        }
        ++m_stateWriteDepth;
    }

    bool DeterministicWorldMutex::try_lock()
    {
        if (!m_operationMutex.try_lock())
        {
            return false;
        }

        if (m_operationDepth == 0)
        {
            m_environment.Enter();
        }
        ++m_operationDepth;

        if (m_stateWriteDepth == 0)
        {
            m_waitingWriterCount.fetch_add(1, AZStd::memory_order_acq_rel);
            const bool locked = m_stateMutex.try_lock();
            m_waitingWriterCount.fetch_sub(1, AZStd::memory_order_release);
            if (!locked)
            {
                unlock_simulation();
                return false;
            }
        }
        ++m_stateWriteDepth;
        return true;
    }

    void DeterministicWorldMutex::unlock()
    {
        --m_stateWriteDepth;
        if (m_stateWriteDepth == 0)
        {
            m_stateMutex.unlock();
        }
        unlock_simulation();
    }

    void DeterministicWorldMutex::lock_shared()
    {
        const bool collectStatistics = m_collectStatistics.load(AZStd::memory_order_relaxed);
        AZStd::chrono::steady_clock::time_point startTime;
        if (collectStatistics)
        {
            startTime = AZStd::chrono::steady_clock::now();
        }
        bool contended = false;
        AZStd::exponential_backoff backoff;
        while (true)
        {
            while (m_waitingWriterCount.load(AZStd::memory_order_acquire) > 0)
            {
                contended = true;
                backoff.wait();
            }

            if (!m_stateMutex.try_lock_shared())
            {
                contended = true;
                m_stateMutex.lock_shared();
            }
            if (m_waitingWriterCount.load(AZStd::memory_order_acquire) == 0)
            {
                if (collectStatistics)
                {
                    const AZ::u64 waitNanoseconds = static_cast<AZ::u64>(
                        AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(
                            AZStd::chrono::steady_clock::now() - startTime)
                            .count());
                    RecordLock(waitNanoseconds, contended);
                }
                return;
            }
            contended = true;
            m_stateMutex.unlock_shared();
            backoff.wait();
        }
    }

    bool DeterministicWorldMutex::try_lock_shared()
    {
        if (m_waitingWriterCount.load(AZStd::memory_order_acquire) > 0
            || !m_stateMutex.try_lock_shared())
        {
            return false;
        }
        if (m_waitingWriterCount.load(AZStd::memory_order_acquire) == 0)
        {
            return true;
        }

        m_stateMutex.unlock_shared();
        return false;
    }

    void DeterministicWorldMutex::unlock_shared()
    {
        m_stateMutex.unlock_shared();
    }

    void DeterministicWorldMutex::lock_simulation()
    {
        if (m_collectStatistics.load(AZStd::memory_order_relaxed))
        {
            const auto startTime = AZStd::chrono::steady_clock::now();
            const bool acquiredWithoutWaiting = m_operationMutex.try_lock();
            if (!acquiredWithoutWaiting)
            {
                m_operationMutex.lock();
            }
            const AZ::u64 waitNanoseconds = static_cast<AZ::u64>(
                AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(
                    AZStd::chrono::steady_clock::now() - startTime)
                    .count());
            RecordLock(waitNanoseconds, !acquiredWithoutWaiting);
        }
        else
        {
            m_operationMutex.lock();
        }
        if (m_operationDepth == 0)
        {
            m_environment.Enter();
        }
        ++m_operationDepth;
    }

    void DeterministicWorldMutex::unlock_simulation()
    {
        --m_operationDepth;
        if (m_operationDepth == 0)
        {
            m_environment.Leave();
        }
        m_operationMutex.unlock();
    }

    void DeterministicWorldMutex::ConfigureStatistics(
        const bool enabled)
    {
        m_collectStatistics.store(enabled, AZStd::memory_order_release);
        if (enabled)
        {
            [[maybe_unused]] const WorldLockStatistics resetStatistics = GetStatistics(true);
        }
    }

    WorldLockStatistics DeterministicWorldMutex::GetStatistics(
        const bool reset)
    {
        if (reset)
        {
            return {
                .m_contentionCount = m_contentionCount.exchange(0, AZStd::memory_order_relaxed),
                .m_lockCount = m_lockCount.exchange(0, AZStd::memory_order_relaxed),
                .m_maximumWaitNanoseconds = m_maximumWaitNanoseconds.exchange(0, AZStd::memory_order_relaxed),
                .m_waitNanoseconds = m_waitNanoseconds.exchange(0, AZStd::memory_order_relaxed),
            };
        }

        return {
            .m_contentionCount = m_contentionCount.load(AZStd::memory_order_relaxed),
            .m_lockCount = m_lockCount.load(AZStd::memory_order_relaxed),
            .m_maximumWaitNanoseconds = m_maximumWaitNanoseconds.load(AZStd::memory_order_relaxed),
            .m_waitNanoseconds = m_waitNanoseconds.load(AZStd::memory_order_relaxed),
        };
    }

    void DeterministicWorldMutex::RecordLock(
        const AZ::u64 waitNanoseconds,
        const bool contended)
    {
        m_lockCount.fetch_add(1, AZStd::memory_order_relaxed);
        m_waitNanoseconds.fetch_add(waitNanoseconds, AZStd::memory_order_relaxed);
        if (contended)
        {
            m_contentionCount.fetch_add(1, AZStd::memory_order_relaxed);
        }

        auto maximumWaitNanoseconds = m_maximumWaitNanoseconds.load(AZStd::memory_order_relaxed);
        while (maximumWaitNanoseconds < waitNanoseconds
            && !m_maximumWaitNanoseconds.compare_exchange_weak(
                maximumWaitNanoseconds,
                waitNanoseconds,
                AZStd::memory_order_relaxed))
        {
        }
    }

    DeterministicWorldQueryLock::DeterministicWorldQueryLock(DeterministicWorldMutex& mutex)
        : m_lock(mutex)
    {
    }

    DeterministicSimulationLock::DeterministicSimulationLock(DeterministicWorldMutex& mutex)
        : m_mutex(mutex)
    {
        m_mutex.lock_simulation();
    }

    DeterministicSimulationLock::~DeterministicSimulationLock()
    {
        m_mutex.unlock_simulation();
    }
} // namespace Jolt

#undef JOLT_HAS_X86_FLOAT_CONTROL
#undef JOLT_HAS_ARM64_FLOAT_CONTROL
#undef JOLT_HAS_NATIVE_FLOAT_CONTROL
