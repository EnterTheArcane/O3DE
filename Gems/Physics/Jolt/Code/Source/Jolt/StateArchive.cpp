/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/StateArchive.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/std/limits.h>

namespace Jolt::Internal
{
    void StateArchiveWriter::WriteBytes(
        const void* data,
        const size_t byteCount)
    {
        if (!m_valid
            || byteCount > MaximumStateArchiveSize - m_data.size())
        {
            m_valid = false;
            return;
        }

        const size_t offset = m_data.size();
        m_data.resize(offset + byteCount);
        if (byteCount > 0)
        {
            std::memcpy(m_data.data() + offset, data, byteCount);
        }
    }

    void StateArchiveWriter::WriteSize(const size_t size)
    {
        if (size > AZStd::numeric_limits<AZ::u32>::max())
        {
            m_valid = false;
            return;
        }

        Write(aznumeric_cast<AZ::u32>(size));
    }

    bool StateArchiveWriter::IsValid() const
    {
        return m_valid;
    }

    bool StateArchiveReader::ReadBytes(
        void* data,
        const size_t byteCount)
    {
        if (byteCount > RemainingByteCount())
        {
            return false;
        }

        if (byteCount > 0)
        {
            std::memcpy(data, m_data.data() + m_offset, byteCount);
        }
        m_offset += byteCount;
        return true;
    }

    bool StateArchiveReader::ReadSize(size_t& size)
    {
        AZ::u32 storedSize = 0;
        if (!Read(storedSize))
        {
            return false;
        }
        size = storedSize;
        return true;
    }

    bool StateArchiveReader::HasReadAllData() const
    {
        return m_offset == m_data.size();
    }

    size_t StateArchiveReader::RemainingByteCount() const
    {
        return m_data.size() - m_offset;
    }
} // namespace Jolt::Internal
