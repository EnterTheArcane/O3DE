/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <cstring>

namespace Jolt::Internal
{
    inline constexpr size_t MaximumStateArchiveSize = size_t{1} << 30;

    class StateArchiveWriter final
    {
    public:
        explicit StateArchiveWriter(AZStd::vector<AZ::u8>& data)
            : m_data(data)
        {
            m_data.clear();
        }

        template<typename Type>
        void Write(const Type& value)
        {
            static_assert(AZStd::is_trivially_copyable_v<Type>);
            WriteBytes(&value, sizeof(value));
        }

        template<typename Type>
        void WriteVector(const AZStd::vector<Type>& values)
        {
            static_assert(AZStd::is_trivially_copyable_v<Type>);
            WriteSize(values.size());
            if (!values.empty())
            {
                WriteBytes(values.data(), values.size() * sizeof(Type));
            }
        }

        void WriteBytes(
            const void* data,
            size_t byteCount);

        void WriteSize(size_t size);

        [[nodiscard]]
        bool IsValid() const;

    private:
        AZStd::vector<AZ::u8>& m_data;
        bool m_valid = true;
    };

    class StateArchiveReader final
    {
    public:
        explicit StateArchiveReader(AZStd::span<const AZ::u8> data)
            : m_data(data)
        {
        }

        template<typename Type>
        bool Read(Type& value)
        {
            static_assert(AZStd::is_trivially_copyable_v<Type>);
            return ReadBytes(&value, sizeof(value));
        }

        template<typename Type>
        bool ReadVector(AZStd::vector<Type>& values)
        {
            static_assert(AZStd::is_trivially_copyable_v<Type>);
            size_t size = 0;
            if (!ReadSize(size)
                || size > MaximumStateArchiveSize / sizeof(Type))
            {
                return false;
            }

            const size_t byteCount = size * sizeof(Type);
            if (byteCount > RemainingByteCount())
            {
                return false;
            }

            values.resize(size);
            if (!values.empty())
            {
                return ReadBytes(values.data(), byteCount);
            }
            return true;
        }

        bool ReadBytes(
            void* data,
            size_t byteCount);

        bool ReadSize(size_t& size);

        [[nodiscard]]
        bool HasReadAllData() const;

        [[nodiscard]]
        size_t RemainingByteCount() const;

    private:
        AZStd::span<const AZ::u8> m_data;
        size_t m_offset = 0;
    };
} // namespace Jolt::Internal
