/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/parallel/mutex.h>

#include <cfenv>
#include <cstddef>

namespace Box3D
{
    class FloatEnvironment final
    {
    public:
        FloatEnvironment() = default;
        void Enter();
        void Leave();

        AZ_DISABLE_COPY_MOVE(FloatEnvironment);

    private:
        std::fenv_t m_previousEnvironment{};
        AZ::u64 m_previousControl = 0;
        bool m_environmentCaptured = false;
        bool m_active = false;
    };

    class DeterministicFloatScope final
    {
    public:
        DeterministicFloatScope();
        ~DeterministicFloatScope();

        AZ_DISABLE_COPY_MOVE(DeterministicFloatScope);

    private:
        FloatEnvironment m_environment;
    };

    class DeterministicRecursiveMutex final
    {
    public:
        DeterministicRecursiveMutex() = default;
        void lock();
        [[nodiscard]] bool try_lock();
        void unlock();

        AZ_DISABLE_COPY_MOVE(DeterministicRecursiveMutex);

    private:
        AZStd::recursive_mutex m_mutex;
        FloatEnvironment m_environment;
        size_t m_depth = 0;
    };
} // namespace Box3D
