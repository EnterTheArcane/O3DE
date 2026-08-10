/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>

#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>

namespace Box3D::Internal
{
    inline constexpr AZ::u32 MaximumWorldCount = 64;
    inline constexpr AZ::u32 WorldIndexBits = 6;
    inline constexpr AZ::u64 WorldIndexMask = (AZ::u64{1} << WorldIndexBits) - 1;
    inline constexpr AZ::u64 HandlePayloadMask = AZStd::numeric_limits<AZ::u32>::max();
    inline constexpr AZ::u32 MaximumWorldMemberIndex = static_cast<AZ::u32>(HandlePayloadMask >> WorldIndexBits) - 1;

    class HandleAccess final
    {
    public:
        template<typename HandleType>
        [[nodiscard]]
        static constexpr HandleType Create(
            typename HandleType::ValueType value) noexcept
        {
            return HandleType(value);
        }

        template<typename Tag>
        [[nodiscard]]
        static constexpr typename Handle<Tag>::ValueType GetValue(
            Handle<Tag> handle) noexcept
        {
            return handle.m_value;
        }
    };

    struct WorldHandleParts final
    {
        AZ::u32 m_index;
        AZ::u32 m_generation;
    };

    struct WorldMemberHandleParts final
    {
        AZ::u32 m_worldIndex;
        AZ::u32 m_index;
        AZ::u32 m_generation;
    };

    class GenerationSource final
    {
    public:
        explicit GenerationSource(
            const AZ::u32 firstGeneration = 1) noexcept
            : m_nextGeneration(firstGeneration)
        {
        }
        AZ_DISABLE_COPY_MOVE(GenerationSource);

        [[nodiscard]]
        AZ::u32 Acquire() noexcept
        {
            AZ::u32 generation = m_nextGeneration.load(AZStd::memory_order_relaxed);
            while (generation != 0)
            {
                AZ::u32 nextGeneration = generation + 1;
                if (generation == AZStd::numeric_limits<AZ::u32>::max())
                {
                    nextGeneration = 0;
                }
                if (m_nextGeneration.compare_exchange_weak(
                        generation, nextGeneration, AZStd::memory_order_relaxed, AZStd::memory_order_relaxed))
                {
                    return generation;
                }
            }
            return 0;
        }

    private:
        AZStd::atomic<AZ::u32> m_nextGeneration;
    };

    [[nodiscard]]
    constexpr WorldHandle MakeWorldHandle(
        const AZ::u32 index,
        const AZ::u32 generation) noexcept
    {
        if (index >= MaximumWorldCount || generation == 0)
        {
            return {};
        }

        const AZ::u64 payload = static_cast<AZ::u64>(index) + 1;
        return HandleAccess::Create<WorldHandle>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    [[nodiscard]]
    constexpr bool DecodeWorldHandle(
        const WorldHandle handle,
        WorldHandleParts& parts) noexcept
    {
        const AZ::u64 value = HandleAccess::GetValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        const AZ::u32 generation = static_cast<AZ::u32>(value >> 32);
        if (payload == 0 || payload > MaximumWorldCount || generation == 0)
        {
            return false;
        }

        parts = {static_cast<AZ::u32>(payload - 1), generation};
        return true;
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr HandleType MakeWorldMemberHandle(
        const AZ::u32 worldIndex,
        const AZ::u32 index,
        const AZ::u32 generation) noexcept
    {
        if (worldIndex >= MaximumWorldCount || index > MaximumWorldMemberIndex || generation == 0)
        {
            return {};
        }

        const AZ::u64 payload = (static_cast<AZ::u64>(index) + 1) << WorldIndexBits | worldIndex;
        return HandleAccess::Create<HandleType>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr bool DecodeWorldMemberHandle(
        HandleType handle,
        WorldMemberHandleParts& parts) noexcept
    {
        const AZ::u64 value = HandleAccess::GetValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        const AZ::u64 indexPlusOne = payload >> WorldIndexBits;
        const AZ::u32 generation = static_cast<AZ::u32>(value >> 32);
        if (indexPlusOne == 0 || generation == 0)
        {
            return false;
        }

        parts = {static_cast<AZ::u32>(payload & WorldIndexMask), static_cast<AZ::u32>(indexPlusOne - 1), generation};
        return true;
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr HandleType MakeRegistryHandle(
        const AZ::u32 index,
        const AZ::u32 generation) noexcept
    {
        if (generation == 0)
        {
            return {};
        }

        const AZ::u64 payload = static_cast<AZ::u64>(index) + 1;
        return HandleAccess::Create<HandleType>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr bool DecodeRegistryHandle(
        HandleType handle,
        AZ::u32& index,
        AZ::u32& generation) noexcept
    {
        const AZ::u64 value = HandleAccess::GetValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        generation = static_cast<AZ::u32>(value >> 32);
        if (payload == 0 || generation == 0)
        {
            return false;
        }

        index = static_cast<AZ::u32>(payload - 1);
        return true;
    }

    //! Returns false when a slot must be retired instead of reviving a stale handle after generation exhaustion.
    [[nodiscard]]
    constexpr bool AdvanceGeneration(
        AZ::u32& generation) noexcept
    {
        if (generation == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        ++generation;
        return true;
    }
} // namespace Box3D::Internal
