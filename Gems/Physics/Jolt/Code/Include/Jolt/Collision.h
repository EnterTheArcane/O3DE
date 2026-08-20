/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Rollback.h>

#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/Name/Name.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/limits.h>

#include <cstddef>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    class CollisionGroupId final
    {
    public:
        using ValueType = AZ::u32;

        static const CollisionGroupId Invalid;

        constexpr CollisionGroupId() noexcept = default;

        explicit constexpr CollisionGroupId(
            const ValueType value) noexcept
            : m_value(value)
        {
        }

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ_TYPE_INFO(CollisionGroupId, "{49E28882-920E-4147-978A-2064CB194ED1}");

        [[nodiscard]]
        constexpr ValueType GetValue() const noexcept
        {
            return m_value;
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != AZStd::numeric_limits<ValueType>::max();
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(CollisionGroupId, CollisionGroupId) noexcept = default;

    private:
        ValueType m_value = AZStd::numeric_limits<ValueType>::max();
    };

    inline constexpr CollisionGroupId CollisionGroupId::Invalid{};

    class CollisionSubGroupId final
    {
    public:
        using ValueType = AZ::u32;

        static const CollisionSubGroupId Invalid;

        constexpr CollisionSubGroupId() noexcept = default;

        explicit constexpr CollisionSubGroupId(
            const ValueType value) noexcept
            : m_value(value)
        {
        }

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ_TYPE_INFO(CollisionSubGroupId, "{5AC1A651-408B-4FFC-821F-397628FB492E}");

        [[nodiscard]]
        constexpr ValueType GetValue() const noexcept
        {
            return m_value;
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != AZStd::numeric_limits<ValueType>::max();
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(CollisionSubGroupId, CollisionSubGroupId) noexcept = default;

    private:
        ValueType m_value = AZStd::numeric_limits<ValueType>::max();
    };

    inline constexpr CollisionSubGroupId CollisionSubGroupId::Invalid{};

    struct SubGroupPair final
    {
        AZ_TYPE_INFO(SubGroupPair, SubGroupPairTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        CollisionSubGroupId m_first;
        CollisionSubGroupId m_second;
    };

    struct GroupFilterTableConfiguration final
    {
        AZ_TYPE_INFO(GroupFilterTableConfiguration, GroupFilterTableConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ::u32 m_subGroupCount = 0;
        AZStd::vector<SubGroupPair> m_disabledPairs;
    };

    struct CollisionGroup final
    {
        CollisionGroupId m_groupId;
        CollisionSubGroupId m_subGroupId;
    };

    class IGroupFilter
        : public IRollbackParticipant
    {
    public:
        virtual ~IGroupFilter() = default;

        //! Callbacks may run concurrently. They must be deterministic and thread-safe, and must not call ISystem.

        [[nodiscard]]
        virtual bool CanCollide(
            CollisionGroup firstGroup,
            CollisionGroup secondGroup) const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;
    };

    struct CollisionGroupConfiguration final
    {
        AZ_TYPE_INFO(CollisionGroupConfiguration, CollisionGroupConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        GroupFilterHandle m_filterHandle;
        CollisionGroupId m_groupId;
        CollisionSubGroupId m_subGroupId;
    };

    class BroadPhaseLayer final
    {
    public:
        using ValueType = AZ::u8;

        static const BroadPhaseLayer Invalid;

        constexpr BroadPhaseLayer() noexcept = default;

        explicit constexpr BroadPhaseLayer(
            ValueType value) noexcept
            : m_value(value)
        {
        }

        AZ_TYPE_INFO(BroadPhaseLayer, BroadPhaseLayerTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr ValueType GetValue() const noexcept
        {
            return m_value;
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(BroadPhaseLayer, BroadPhaseLayer) noexcept = default;

    private:
        ValueType m_value = 0;
    };

    inline constexpr BroadPhaseLayer BroadPhaseLayer::Invalid{};

    class ObjectLayer final
    {
    public:
        using ValueType = AZ::u16;

        static const ObjectLayer Invalid;

        constexpr ObjectLayer() noexcept = default;

        explicit constexpr ObjectLayer(
            ValueType value) noexcept
            : m_value(value)
        {
        }

        AZ_TYPE_INFO(ObjectLayer, ObjectLayerTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        [[nodiscard]]
        constexpr ValueType GetValue() const noexcept
        {
            return m_value;
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(ObjectLayer, ObjectLayer) noexcept = default;

    private:
        ValueType m_value = 0;
    };

    inline constexpr ObjectLayer ObjectLayer::Invalid{};

    namespace DefaultLayers
    {
        inline constexpr ObjectLayer Moving{2};
        inline constexpr ObjectLayer NonMoving{1};
    } // namespace DefaultLayers

    namespace DefaultBroadPhaseLayers
    {
        inline constexpr BroadPhaseLayer Moving{2};
        inline constexpr BroadPhaseLayer NonMoving{1};
    } // namespace DefaultBroadPhaseLayers

    struct BroadPhaseLayerConfiguration final
    {
        AZ_TYPE_INFO(BroadPhaseLayerConfiguration, BroadPhaseLayerConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ::Name m_name;
    };

    struct ObjectLayerConfiguration final
    {
        AZ_TYPE_INFO(ObjectLayerConfiguration, ObjectLayerConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZ::Name m_name;
        AZStd::vector<ObjectLayer> m_collidesWith;
        BroadPhaseLayer m_broadPhaseLayer;
    };

    enum class AllowedDofs : AZ::u8
    {
        None = 0,
        TranslationX = 1 << 0,
        TranslationY = 1 << 1,
        TranslationZ = 1 << 2,
        RotationX = 1 << 3,
        RotationY = 1 << 4,
        RotationZ = 1 << 5,
        All = 0x3f,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(AllowedDofs)

    enum class MotionQuality : AZ::u8
    {
        None = 0,
        Continuous,
        Discrete,
    };

    enum class MotionType : AZ::u8
    {
        None = 0,
        Dynamic,
        Kinematic,
        Static,
    };
} // namespace Jolt

template<>
struct AZStd::hash<Jolt::ObjectLayer> final
{
    constexpr size_t operator()(
        Jolt::ObjectLayer layer) const noexcept
    {
        return AZStd::hash<AZ::u16>{}(layer.GetValue());
    }
};
