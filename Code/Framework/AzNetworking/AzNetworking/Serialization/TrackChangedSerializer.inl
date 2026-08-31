/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <cstring>

namespace AzNetworking
{
    template <typename BASE_TYPE>
    TrackChangedSerializer<BASE_TYPE>::TrackChangedSerializer(const uint8_t* buffer, uint32_t bufferCapacity)
        : BASE_TYPE(buffer, bufferCapacity)
        , m_hasChanged(false)
    {
        ;
    }

    template <typename BASE_TYPE>
    SerializerMode TrackChangedSerializer<BASE_TYPE>::GetSerializerMode() const
    {
        return BASE_TYPE::GetSerializerMode();
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(bool& value, const char* name)
    {
        const bool cached = value;
        const bool result = BASE_TYPE::Serialize(value, name);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(int8_t& value, const char* name, int8_t minValue, int8_t maxValue)
    {
        const int8_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(int16_t& value, const char* name, int16_t minValue, int16_t maxValue)
    {
        const int16_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(int32_t& value, const char* name, int32_t minValue, int32_t maxValue)
    {
        const int32_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(long& value, const char* name, long minValue, long maxValue)
    {
        const long cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(AZ::s64& value, const char* name, AZ::s64 minValue, AZ::s64 maxValue)
    {
        const AZ::s64 cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(uint8_t& value, const char* name, uint8_t minValue, uint8_t maxValue)
    {
        const uint8_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(uint16_t& value, const char* name, uint16_t minValue, uint16_t maxValue)
    {
        const uint16_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(uint32_t& value, const char* name, uint32_t minValue, uint32_t maxValue)
    {
        const uint32_t cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(unsigned long& value, const char* name, unsigned long minValue, unsigned long maxValue)
    {
        const unsigned long cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(AZ::u64& value, const char* name, AZ::u64 minValue, AZ::u64 maxValue)
    {
        const AZ::u64 cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (cached != value)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(float& value, const char* name, float minValue, float maxValue)
    {
        const float cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (std::memcmp(&cached, &value, sizeof(value)) != 0)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::Serialize(double& value, const char* name, double minValue, double maxValue)
    {
        const double cached = value;
        const bool result = BASE_TYPE::Serialize(value, name, minValue, maxValue);
        if (std::memcmp(&cached, &value, sizeof(value)) != 0)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::SerializeBytes(uint8_t* buffer, uint32_t bufferCapacity, bool isString, uint32_t& outSize, const char* name)
    {
        if (outSize > bufferCapacity || (outSize > 0 && buffer == nullptr))
        {
            this->Invalidate();
            return false;
        }

        const uint32_t cachedSize = outSize;
        if (!CacheBytes(buffer, cachedSize))
        {
            this->Invalidate();
            return false;
        }

        const bool result = BASE_TYPE::SerializeBytes(buffer, bufferCapacity, isString, outSize, name);
        if (outSize > bufferCapacity || (outSize > 0 && buffer == nullptr))
        {
            if (cachedSize != outSize)
            {
                m_hasChanged = true;
            }
            this->Invalidate();
            return false;
        }

        bool changed = cachedSize != outSize;
        if (!changed && outSize > 0)
        {
            changed = std::memcmp(GetCachedBytes(cachedSize), buffer, outSize) != 0;
        }
        if (changed)
        {
            m_hasChanged = true;
        }
        return result;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::CacheBytes(const uint8_t* buffer, uint32_t size)
    {
        if (size == 0)
        {
            return true;
        }

        if (buffer == nullptr)
        {
            return false;
        }

        if (size <= m_inlineByteCache.size())
        {
            std::memcpy(m_inlineByteCache.data(), buffer, size);
            return true;
        }

        if (size > m_overflowByteCache.max_size())
        {
            return false;
        }

        m_overflowByteCache.resize_no_construct(size);
        std::memcpy(m_overflowByteCache.data(), buffer, size);
        return true;
    }

    template <typename BASE_TYPE>
    const uint8_t* TrackChangedSerializer<BASE_TYPE>::GetCachedBytes(uint32_t size) const
    {
        if (size <= m_inlineByteCache.size())
        {
            return m_inlineByteCache.data();
        }
        return m_overflowByteCache.data();
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::BeginObject(const char* name)
    {
        return BASE_TYPE::BeginObject(name);
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::EndObject(const char* name)
    {
        return BASE_TYPE::EndObject(name);
    }

    template <typename BASE_TYPE>
    const uint8_t* TrackChangedSerializer<BASE_TYPE>::GetBuffer() const
    {
        return BASE_TYPE::GetBuffer();
    }

    template <typename BASE_TYPE>
    uint32_t TrackChangedSerializer<BASE_TYPE>::GetCapacity() const
    {
        return BASE_TYPE::GetCapacity();
    }

    template <typename BASE_TYPE>
    uint32_t TrackChangedSerializer<BASE_TYPE>::GetSize() const
    {
        return BASE_TYPE::GetSize();
    }

    template <typename BASE_TYPE>
    void TrackChangedSerializer<BASE_TYPE>::ClearTrackedChangesFlag()
    {
        m_hasChanged = false;
    }

    template <typename BASE_TYPE>
    bool TrackChangedSerializer<BASE_TYPE>::GetTrackedChangesFlag() const
    {
        return m_hasChanged;
    }
}
