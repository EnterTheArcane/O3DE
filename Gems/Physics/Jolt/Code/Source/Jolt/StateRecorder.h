/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/span.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/StateRecorder.h>

namespace Jolt
{
    class NativeStateRecorder final
        : public JPH::StateRecorder
    {
    public:
        NativeStateRecorder() = default;
        explicit NativeStateRecorder(AZStd::span<const AZ::u8> data);
        explicit NativeStateRecorder(AZStd::vector<AZ::u8>& data);

        AZ_DISABLE_COPY_MOVE(NativeStateRecorder);

        void WriteBytes(
            const void* data,
            size_t byteCount) override;

        void ReadBytes(
            void* data,
            size_t byteCount) override;

        [[nodiscard]]
        bool IsEOF() const override;

        [[nodiscard]]
        bool IsFailed() const override;

        [[nodiscard]]
        bool HasValidationMismatch() const;

        [[nodiscard]]
        bool HasReadAllData() const;

        [[nodiscard]]
        size_t GetFirstMismatchByte() const;

        [[nodiscard]]
        AZStd::vector<AZ::u8> TakeData();

    private:
        AZStd::vector<AZ::u8> m_writeData;
        AZStd::vector<AZ::u8>* m_externalWriteData = nullptr;
        AZStd::span<const AZ::u8> m_readData;
        size_t m_readOffset = 0;
        size_t m_firstMismatchByte = 0;
        bool m_failed = false;
        bool m_validationMismatch = false;
    };
} // namespace Jolt
