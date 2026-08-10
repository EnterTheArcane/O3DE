/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/FloatEnvironment.h>

#if defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#define BOX3D_HAS_X86_FLOAT_CONTROL 1
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 0
#elif defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#define BOX3D_HAS_X86_FLOAT_CONTROL 1
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 0
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
#include <arm64intr.h>
#include <intrin.h>
#define BOX3D_HAS_X86_FLOAT_CONTROL 0
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 1
#elif defined(__aarch64__) && defined(__clang__)
#include <arm_acle.h>
#define BOX3D_HAS_X86_FLOAT_CONTROL 0
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 1
#elif defined(__aarch64__)
#define BOX3D_HAS_X86_FLOAT_CONTROL 0
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 1
#else
#define BOX3D_HAS_X86_FLOAT_CONTROL 0
#define BOX3D_HAS_ARM64_FLOAT_CONTROL 0
#endif

#define BOX3D_HAS_NATIVE_FLOAT_CONTROL (BOX3D_HAS_X86_FLOAT_CONTROL || BOX3D_HAS_ARM64_FLOAT_CONTROL)

namespace Box3D
{
    namespace
    {
#if BOX3D_HAS_X86_FLOAT_CONTROL
        constexpr AZ::u64 DefaultFloatControl = 0x1F80;
        constexpr AZ::u64 FloatControlModeMask = 0xFFC0;

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
#elif BOX3D_HAS_ARM64_FLOAT_CONTROL
        constexpr AZ::u64 DefaultFloatControl = 0;
        constexpr AZ::u64 Arm64FormatAndRoundingMask = AZ::u64{0x7FF} << 16;
        constexpr AZ::u64 Arm64ExceptionAndBFloatMask = (AZ::u64{1} << 15) | (AZ::u64{1} << 13) | (AZ::u64{0x1F} << 8);
        constexpr AZ::u64 Arm64AlternativeBehaviorMask = AZ::u64{0x7};
        constexpr AZ::u64 FloatControlModeMask = Arm64FormatAndRoundingMask | Arm64ExceptionAndBFloatMask | Arm64AlternativeBehaviorMask;

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
#endif
    } // namespace

    void FloatEnvironment::Enter()
    {
#if BOX3D_HAS_NATIVE_FLOAT_CONTROL
        const AZ::u64 currentControl = ReadFloatControl();
#if defined(_M_X64) || defined(__x86_64__)
        if ((currentControl & FloatControlModeMask) == DefaultFloatControl)
#else
        if (std::fegetround() == FE_TONEAREST && (currentControl & FloatControlModeMask) == DefaultFloatControl)
#endif
        {
            return;
        }

        m_previousControl = currentControl;
#endif
        m_environmentCaptured = std::fegetenv(&m_previousEnvironment) == 0;
        std::fesetenv(FE_DFL_ENV);
        std::fesetround(FE_TONEAREST);
#if BOX3D_HAS_X86_FLOAT_CONTROL
        WriteFloatControl(DefaultFloatControl);
#elif BOX3D_HAS_ARM64_FLOAT_CONTROL
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
#if BOX3D_HAS_NATIVE_FLOAT_CONTROL
        WriteFloatControl(m_previousControl);
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

    void DeterministicRecursiveMutex::lock()
    {
        m_mutex.lock();
        if (m_depth == 0)
        {
            m_environment.Enter();
        }
        ++m_depth;
    }

    bool DeterministicRecursiveMutex::try_lock()
    {
        if (!m_mutex.try_lock())
        {
            return false;
        }

        if (m_depth == 0)
        {
            m_environment.Enter();
        }
        ++m_depth;
        return true;
    }

    void DeterministicRecursiveMutex::unlock()
    {
        --m_depth;
        if (m_depth == 0)
        {
            m_environment.Leave();
        }
        m_mutex.unlock();
    }
} // namespace Box3D

#undef BOX3D_HAS_X86_FLOAT_CONTROL
#undef BOX3D_HAS_ARM64_FLOAT_CONTROL
#undef BOX3D_HAS_NATIVE_FLOAT_CONTROL
