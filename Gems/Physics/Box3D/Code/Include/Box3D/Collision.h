/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/limits.h>

namespace Box3D
{
    inline constexpr AZ::u32 MaximumCollisionLayerCount = 64;

    //! Selects one collision category bit.
    class CollisionLayer final
    {
    public:
        constexpr CollisionLayer() noexcept = default;

        explicit constexpr CollisionLayer(
            AZ::u8 index) noexcept
            : m_index(index)
        {
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_index < MaximumCollisionLayerCount;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]]
        constexpr AZ::u8 GetIndex() const noexcept
        {
            return m_index;
        }

        [[nodiscard]]
        constexpr AZ::u64 GetMask() const noexcept
        {
            if (IsValid())
            {
                return AZ::u64{1} << m_index;
            }

            return 0;
        }

        friend constexpr bool operator==(CollisionLayer, CollisionLayer) noexcept = default;

    private:
        AZ::u8 m_index = 0;
    };

    //! Broad-phase category and acceptance masks with an optional shape-contact group override.
    //! Queries use only the category and mask bits because Box3D query filters do not have group semantics.
    struct CollisionFilter final
    {
        AZ_TYPE_INFO(CollisionFilter, "{8048963A-C312-4302-8A4C-D9BC0DF3622F}");

        [[nodiscard]]
        constexpr bool Allows(
            const CollisionFilter& other) const noexcept
        {
            if (m_groupIndex != 0 && m_groupIndex == other.m_groupIndex)
            {
                return m_groupIndex > 0;
            }
            return (m_categoryBits & other.m_maskBits) != 0 && (other.m_categoryBits & m_maskBits) != 0;
        }

        AZ::u64 m_categoryBits = 1;
        AZ::u64 m_maskBits = AZStd::numeric_limits<AZ::u64>::max();

        AZ::s32 m_groupIndex = 0;
    };
} // namespace Box3D
