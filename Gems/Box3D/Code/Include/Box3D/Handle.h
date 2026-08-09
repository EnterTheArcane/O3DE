/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/hash.h>

#include <cstddef>
#include <type_traits>

namespace Box3D
{
    namespace Internal
    {
        class HandleAccess;
    }

    //! Strong transient identity for an object owned by the Box3D system.
    //! Handles are validated on every lookup and are not persistent asset or network identities.
    template<class Tag>
    class Handle final
    {
    public:
        using ValueType = AZ::u64;

        constexpr Handle() noexcept = default;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]] constexpr bool Equal(Handle other) const noexcept
        {
            return m_value == other.m_value;
        }

        friend constexpr bool operator==(Handle, Handle) noexcept = default;

        friend constexpr bool operator<(Handle left, Handle right) noexcept
        {
            return left.m_value < right.m_value;
        }

    private:
        explicit constexpr Handle(ValueType value) noexcept
            : m_value(value)
        {
        }

        ValueType m_value{};

        friend class Internal::HandleAccess;
        template<class>
        friend struct AZStd::hash;
    };

    struct WorldHandleTag final
    {
        AZ_TYPE_INFO(WorldHandleTag, "{B825BF10-D8D3-40D6-8B27-D0706DF86EE6}");
    };

    struct BodyHandleTag final
    {
        AZ_TYPE_INFO(BodyHandleTag, "{EE77D1D9-0BB6-4422-A7FB-3D6ED180FF3E}");
    };

    struct ShapeHandleTag final
    {
        AZ_TYPE_INFO(ShapeHandleTag, "{A7F4D42C-A582-4C2D-B152-0E622A4CB09E}");
    };

    struct JointHandleTag final
    {
        AZ_TYPE_INFO(JointHandleTag, "{B74C5032-B2F1-4A9D-B2A4-F95DD7D57986}");
    };

    struct MaterialHandleTag final
    {
        AZ_TYPE_INFO(MaterialHandleTag, "{1D6C3913-C39C-49D1-8D87-872874B94741}");
    };

    struct CharacterHandleTag final
    {
        AZ_TYPE_INFO(CharacterHandleTag, "{8FA71519-6FA4-42AA-8811-E53C07509720}");
    };

    struct CookedShapeHandleTag final
    {
        AZ_TYPE_INFO(CookedShapeHandleTag, "{375622D4-EA65-4E02-A90E-FDAA370621CE}");
    };

    struct ReplayHandleTag final
    {
        AZ_TYPE_INFO(ReplayHandleTag, "{7D9DE457-1AF7-4D77-9201-D81F4F144920}");
    };

    using WorldHandle = Handle<WorldHandleTag>;
    using BodyHandle = Handle<BodyHandleTag>;
    using ShapeHandle = Handle<ShapeHandleTag>;
    using JointHandle = Handle<JointHandleTag>;
    using MaterialHandle = Handle<MaterialHandleTag>;
    using CharacterHandle = Handle<CharacterHandleTag>;
    using CookedShapeHandle = Handle<CookedShapeHandleTag>;
    using ReplayHandle = Handle<ReplayHandleTag>;

    inline constexpr WorldHandle InvalidWorldHandle;
    inline constexpr BodyHandle InvalidBodyHandle;
    inline constexpr ShapeHandle InvalidShapeHandle;
    inline constexpr JointHandle InvalidJointHandle;
    inline constexpr MaterialHandle InvalidMaterialHandle;
    inline constexpr CharacterHandle InvalidCharacterHandle;
    inline constexpr CookedShapeHandle InvalidCookedShapeHandle;
    inline constexpr ReplayHandle InvalidReplayHandle;

    //! Number of values written and the capacity needed for an untruncated result.
    struct BufferResult final
    {
        size_t m_count = 0;
        size_t m_requiredCount = 0;

        [[nodiscard]] constexpr bool HasOverflow() const noexcept
        {
            return m_requiredCount > m_count;
        }
    };

    static_assert(sizeof(WorldHandle) == sizeof(AZ::u64));
    static_assert(sizeof(BodyHandle) == sizeof(AZ::u64));
    static_assert(sizeof(ShapeHandle) == sizeof(AZ::u64));
    static_assert(sizeof(JointHandle) == sizeof(AZ::u64));
    static_assert(sizeof(MaterialHandle) == sizeof(AZ::u64));
    static_assert(sizeof(CharacterHandle) == sizeof(AZ::u64));
    static_assert(sizeof(CookedShapeHandle) == sizeof(AZ::u64));
    static_assert(sizeof(ReplayHandle) == sizeof(AZ::u64));
    static_assert(std::is_trivially_copyable_v<WorldHandle>);
    static_assert(std::is_trivially_copyable_v<BodyHandle>);
    static_assert(std::is_trivially_copyable_v<ShapeHandle>);
    static_assert(std::is_trivially_copyable_v<JointHandle>);
    static_assert(std::is_trivially_copyable_v<MaterialHandle>);
    static_assert(std::is_trivially_copyable_v<CharacterHandle>);
    static_assert(std::is_trivially_copyable_v<CookedShapeHandle>);
    static_assert(std::is_trivially_copyable_v<ReplayHandle>);
} // namespace Box3D

namespace AZ
{
    AZ_TYPE_INFO_TEMPLATE(Box3D::Handle, "{EAD841B0-719C-464C-AF13-921647711B6E}", AZ_TYPE_INFO_TYPENAME);
}

namespace AZStd
{
    template<class Tag>
    struct hash<Box3D::Handle<Tag>> final
    {
        constexpr size_t operator()(Box3D::Handle<Tag> handle) const noexcept
        {
            return hash<AZ::u64>{}(handle.m_value);
        }
    };
} // namespace AZStd
