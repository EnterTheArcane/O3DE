/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <AzCore/base.h>
#include <AzCore/std/utility/move.h>
#include <AzCore/std/utils.h>

namespace Jolt
{
    enum class OperationStatus : AZ::u8
    {
        None = 0,
        Pending,
        Running,
        Succeeded,
        Failed,
        Canceled,
    };

    namespace Internal
    {
        class OperationPool;
        class OperationRecord;

        template<class Result, class Work>
        class TypedOperationRecord;

        JOLT_API bool CancelOperation(OperationRecord* record);

        JOLT_API const void* GetOperationResult(const OperationRecord* record);

        JOLT_API OperationStatus GetOperationStatus(const OperationRecord* record);

        JOLT_API void ReleaseOperation(OperationRecord* record);

        JOLT_API OperationStatus WaitForOperation(OperationRecord* record);
    } // namespace Internal

    //! Owns one provider-managed operation result. This type is intentionally not thread safe.
    //! Destruction requests cancellation while work is queued, but never waits after work begins.
    //! Running work retains its dependencies, and a completed result remains readable until Reset or destruction.
    template<class Result>
    class Operation final
    {
    public:
        Operation() = default;

        Operation(Operation&& other) noexcept
            : m_record(AZStd::exchange(other.m_record, nullptr))
        {
        }

        ~Operation()
        {
            Reset();
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        Operation& operator=(Operation&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                m_record = AZStd::exchange(other.m_record, nullptr);
            }
            return *this;
        }

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return m_record;
        }

        //! Requests cancellation only while the work remains queued.
        [[nodiscard]]
        bool Cancel()
        {
            return Internal::CancelOperation(m_record);
        }

        [[nodiscard]]
        OperationStatus GetStatus() const
        {
            return Internal::GetOperationStatus(m_record);
        }

        //! Uses the job system's worker-aware assistance when called from a worker.
        [[nodiscard]]
        OperationStatus Wait()
        {
            return Internal::WaitForOperation(m_record);
        }

        //! Returns the result for succeeded and failed operations, or null while pending, running, or canceled.
        //! The returned result remains valid until this operation is reset or destroyed.
        [[nodiscard]]
        const Result* GetResult() const
        {
            return static_cast<const Result*>(Internal::GetOperationResult(m_record));
        }

        void Reset()
        {
            if (m_record)
            {
                Internal::CancelOperation(m_record);
                Internal::ReleaseOperation(m_record);
                m_record = nullptr;
            }
        }

    private:
        friend class Internal::OperationPool;

        explicit Operation(Internal::OperationRecord* record)
            : m_record(record)
        {
        }

        Internal::OperationRecord* m_record = nullptr;
    };
} // namespace Jolt
