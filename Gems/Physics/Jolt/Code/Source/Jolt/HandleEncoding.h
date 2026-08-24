/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Handle.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt::Internal
{
    inline constexpr AZ::u32 MaximumWorldCount = Jolt::MaximumWorldCount;
    inline constexpr AZ::u32 WorldIndexBits = 6;
    inline constexpr AZ::u64 WorldIndexMask = (AZ::u64{1} << WorldIndexBits) - 1;
    inline constexpr AZ::u64 HandlePayloadMask = AZStd::numeric_limits<AZ::u32>::max();
    inline constexpr AZ::u32 MaximumWorldMemberIndex = static_cast<AZ::u32>(HandlePayloadMask >> WorldIndexBits) - 1;

    enum class HandleKind : AZ::u8
    {
        None = 0,
        Body,
        Character,
        Constraint,
        CookedShape,
        Extension,
        GroupFilter,
        Hair,
        HairDefinition,
        Material,
        Path,
        Ragdoll,
        RagdollDefinition,
        SceneDefinition,
        SceneInstance,
        Shape,
        SkeletalAnimation,
        SkeletonDefinition,
        SkeletonMapper,
        SkeletonPose,
        StateSnapshot,
        SoftBodyDefinition,
        Vehicle,
        VirtualCharacter,
        World,
        Count,
    };

    inline constexpr size_t HandleKindCount = static_cast<size_t>(HandleKind::Count) - 1;

    template<typename HandleType>
    struct HandleKindTraits;

    template<>
    struct HandleKindTraits<BodyHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Body;
    };

    template<>
    struct HandleKindTraits<CharacterHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Character;
    };

    template<>
    struct HandleKindTraits<ConstraintHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Constraint;
    };

    template<>
    struct HandleKindTraits<CookedShapeHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::CookedShape;
    };

    template<>
    struct HandleKindTraits<ExtensionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Extension;
    };

    template<>
    struct HandleKindTraits<GroupFilterHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::GroupFilter;
    };

    template<>
    struct HandleKindTraits<HairHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Hair;
    };

    template<>
    struct HandleKindTraits<HairDefinitionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::HairDefinition;
    };

    template<>
    struct HandleKindTraits<MaterialHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Material;
    };

    template<>
    struct HandleKindTraits<PathHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Path;
    };

    template<>
    struct HandleKindTraits<RagdollHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Ragdoll;
    };

    template<>
    struct HandleKindTraits<RagdollDefinitionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::RagdollDefinition;
    };

    template<>
    struct HandleKindTraits<SceneDefinitionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SceneDefinition;
    };

    template<>
    struct HandleKindTraits<SceneInstanceHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SceneInstance;
    };

    template<>
    struct HandleKindTraits<ShapeHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Shape;
    };

    template<>
    struct HandleKindTraits<SkeletalAnimationHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SkeletalAnimation;
    };

    template<>
    struct HandleKindTraits<SkeletonDefinitionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SkeletonDefinition;
    };

    template<>
    struct HandleKindTraits<SkeletonMapperHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SkeletonMapper;
    };

    template<>
    struct HandleKindTraits<SkeletonPoseHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SkeletonPose;
    };

    template<>
    struct HandleKindTraits<StateSnapshotHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::StateSnapshot;
    };

    template<>
    struct HandleKindTraits<SoftBodyDefinitionHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::SoftBodyDefinition;
    };

    template<>
    struct HandleKindTraits<VehicleHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::Vehicle;
    };

    template<>
    struct HandleKindTraits<VirtualCharacterHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::VirtualCharacter;
    };

    template<>
    struct HandleKindTraits<WorldHandle> final
    {
        static constexpr HandleKind Kind = HandleKind::World;
    };

    class AtomicGenerationSource final
    {
    public:
        explicit AtomicGenerationSource(const AZ::u32 firstGeneration = 1) noexcept
            : m_nextGeneration(firstGeneration)
        {
        }

        AZ_DISABLE_COPY_MOVE(AtomicGenerationSource);

        [[nodiscard]]
        AZ::u32 Acquire() noexcept
        {
            AZ::u32 generation = m_nextGeneration.load(AZStd::memory_order_relaxed);
            while (generation != 0)
            {
                AZ::u32 nextGeneration = 0;
                if (generation != AZStd::numeric_limits<AZ::u32>::max())
                {
                    nextGeneration = generation + 1;
                }
                if (m_nextGeneration.compare_exchange_weak(
                    generation,
                    nextGeneration,
                    AZStd::memory_order_relaxed,
                    AZStd::memory_order_relaxed))
                {
                    return generation;
                }
            }
            return 0;
        }

    private:
        AZStd::atomic<AZ::u32> m_nextGeneration;
    };

    using AtomicGenerationSources = AZStd::array<AtomicGenerationSource, HandleKindCount>;

    [[nodiscard]]
    JOLT_API AZ::u32 AcquireHandleGeneration(HandleKind handleKind) noexcept;

    template<typename HandleType>
    [[nodiscard]]
    AZ::u32 AcquireHandleGeneration() noexcept
    {
        return AcquireHandleGeneration(HandleKindTraits<HandleType>::Kind);
    }

    template<typename HandleType>
    [[nodiscard]]
    AZ::u32 AcquireHandleGeneration(AtomicGenerationSources& generationSources) noexcept
    {
        constexpr size_t kindIndex = static_cast<size_t>(HandleKindTraits<HandleType>::Kind) - 1;
        return generationSources[kindIndex].Acquire();
    }

    class HandleAccess final
    {
    public:
        template<typename HandleType>
        [[nodiscard]]
        static constexpr HandleType FromValue(typename HandleType::ValueType value) noexcept
        {
            return HandleType(value);
        }

        template<typename HandleType>
        [[nodiscard]]
        static constexpr typename HandleType::ValueType ToValue(HandleType handle) noexcept
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

} // namespace Jolt::Internal
