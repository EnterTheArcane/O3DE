/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>

#include <AzCore/std/limits.h>

namespace Jolt::Internal
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
        static constexpr HandleType FromValue(
            typename HandleType::ValueType value) noexcept
        {
            return HandleType(value);
        }

        template<typename HandleType>
        [[nodiscard]]
        static constexpr typename HandleType::ValueType ToValue(
            HandleType handle) noexcept
        {
            return handle.m_value;
        }
    };

    struct WorldHandleParts final
    {
        AZ::u32 m_index = 0;
        AZ::u32 m_generation = 0;
    };

    struct WorldMemberHandleParts final
    {
        AZ::u32 m_worldIndex = 0;
        AZ::u32 m_index = 0;
        AZ::u32 m_generation = 0;
    };

    struct ResourceHandleParts final
    {
        AZ::u32 m_index = 0;
        AZ::u32 m_generation = 0;
    };

    template<typename HandleType>
    [[nodiscard]]
    constexpr HandleType MakeResourceHandle(
        const AZ::u32 index,
        const AZ::u32 generation) noexcept
    {
        if (index == AZStd::numeric_limits<AZ::u32>::max() || generation == 0)
        {
            return {};
        }

        const AZ::u64 payload = static_cast<AZ::u64>(index) + 1;
        return HandleAccess::FromValue<HandleType>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr bool DecodeResourceHandle(
        const HandleType handle,
        ResourceHandleParts& parts) noexcept
    {
        const AZ::u64 value = HandleAccess::ToValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        const AZ::u32 generation = static_cast<AZ::u32>(value >> 32);
        if (payload == 0 || generation == 0)
        {
            return false;
        }

        parts = {
            .m_index = static_cast<AZ::u32>(payload - 1),
            .m_generation = generation,
        };
        return true;
    }

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
        return HandleAccess::FromValue<WorldHandle>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    [[nodiscard]]
    constexpr bool DecodeWorldHandle(
        const WorldHandle handle,
        WorldHandleParts& parts) noexcept
    {
        const AZ::u64 value = HandleAccess::ToValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        const AZ::u32 generation = static_cast<AZ::u32>(value >> 32);
        if (payload == 0 || payload > MaximumWorldCount || generation == 0)
        {
            return false;
        }

        parts = {
            .m_index = static_cast<AZ::u32>(payload - 1),
            .m_generation = generation,
        };
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
        return HandleAccess::FromValue<HandleType>(static_cast<AZ::u64>(generation) << 32 | payload);
    }

    template<typename HandleType>
    [[nodiscard]]
    constexpr bool DecodeWorldMemberHandle(
        HandleType handle,
        WorldMemberHandleParts& parts) noexcept
    {
        const AZ::u64 value = HandleAccess::ToValue(handle);
        const AZ::u64 payload = value & HandlePayloadMask;
        const AZ::u64 indexPlusOne = payload >> WorldIndexBits;
        const AZ::u32 generation = static_cast<AZ::u32>(value >> 32);
        if (indexPlusOne == 0 || generation == 0)
        {
            return false;
        }

        parts = {
            .m_worldIndex = static_cast<AZ::u32>(payload & WorldIndexMask),
            .m_index = static_cast<AZ::u32>(indexPlusOne - 1),
            .m_generation = generation,
        };
        return true;
    }

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
} // namespace Jolt::Internal
