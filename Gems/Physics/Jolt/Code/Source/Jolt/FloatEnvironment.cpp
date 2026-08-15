/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/FloatEnvironment.h>

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
            (currentControl & (FloatControlModeMask | FloatStatusMask)) == DefaultFloatControl;
#elif JOLT_HAS_ARM64_FLOAT_CONTROL
        controlIsCanonical = std::fegetround() == FE_TONEAREST
            && (currentControl & FloatControlModeMask) == DefaultFloatControl;
#endif
        if (controlIsCanonical)
        {
            m_previousControl = currentControl;
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
            m_stateMutex.lock();
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
        AZStd::exponential_backoff backoff;
        while (true)
        {
            while (m_waitingWriterCount.load(AZStd::memory_order_acquire) > 0)
            {
                backoff.wait();
            }

            m_stateMutex.lock_shared();
            if (m_waitingWriterCount.load(AZStd::memory_order_acquire) == 0)
            {
                return;
            }
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
        m_operationMutex.lock();
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
