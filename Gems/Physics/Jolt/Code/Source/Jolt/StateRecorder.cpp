/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/StateRecorder.h>

#include <AzCore/std/utility/move.h>
#include <AzCore/std/algorithm.h>

#include <cstring>

namespace Jolt
{
    NativeStateRecorder::NativeStateRecorder(
        const AZStd::span<const AZ::u8> data)
        : m_readData(data)
    {
    }

    NativeStateRecorder::NativeStateRecorder(
        AZStd::vector<AZ::u8>& data)
        : m_externalWriteData(&data)
    {
        data.clear();
    }

    void NativeStateRecorder::WriteBytes(
        const void* data,
        const size_t byteCount)
    {
        if (!data && byteCount > 0)
        {
            m_failed = true;
            return;
        }

        AZStd::vector<AZ::u8>* writeData = &m_writeData;
        if (m_externalWriteData)
        {
            writeData = m_externalWriteData;
        }
        const size_t offset = writeData->size();
        if (byteCount > writeData->max_size() - offset)
        {
            m_failed = true;
            return;
        }
        writeData->resize(offset + byteCount);
        if (byteCount > 0)
        {
            std::memcpy(writeData->data() + offset, data, byteCount);
        }
    }

    void NativeStateRecorder::ReadBytes(
        void* data,
        const size_t byteCount)
    {
        if ((!data && byteCount > 0) || byteCount > m_readData.size() - AZStd::min(m_readOffset, m_readData.size()))
        {
            m_failed = true;
            return;
        }
        if (byteCount > 0)
        {
            if (IsValidating())
            {
                const auto* currentBytes = static_cast<const AZ::u8*>(data);
                for (size_t byteIndex = 0; byteIndex < byteCount; ++byteIndex)
                {
                    if (currentBytes[byteIndex] != m_readData[m_readOffset + byteIndex])
                    {
                        if (!m_validationMismatch)
                        {
                            m_firstMismatchByte = m_readOffset + byteIndex;
                        }
                        m_validationMismatch = true;
                        break;
                    }
                }
            }
            std::memcpy(data, m_readData.data() + m_readOffset, byteCount);
            m_readOffset += byteCount;
        }
    }

    bool NativeStateRecorder::IsEOF() const
    {
        return m_readOffset >= m_readData.size();
    }

    bool NativeStateRecorder::IsFailed() const
    {
        return m_failed;
    }

    bool NativeStateRecorder::HasValidationMismatch() const
    {
        return m_validationMismatch;
    }

    bool NativeStateRecorder::HasReadAllData() const
    {
        return m_readOffset == m_readData.size();
    }

    size_t NativeStateRecorder::GetFirstMismatchByte() const
    {
        return m_firstMismatchByte;
    }

    AZStd::vector<AZ::u8> NativeStateRecorder::TakeData()
    {
        if (m_externalWriteData)
        {
            return {};
        }
        return AZStd::move(m_writeData);
    }
} // namespace Jolt
