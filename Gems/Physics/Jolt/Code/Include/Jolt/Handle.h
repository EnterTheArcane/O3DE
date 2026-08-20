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

namespace Jolt
{
    namespace Internal
    {
        class HandleAccess;
    }

    template<typename Tag>
    class Handle final
    {
    public:
        using ValueType = AZ::u64;

        static const Handle Invalid;

        constexpr Handle() noexcept = default;

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        friend constexpr bool operator==(Handle, Handle) noexcept = default;

        friend constexpr bool operator<(
            Handle left,
            Handle right) noexcept
        {
            return left.m_value < right.m_value;
        }

    private:
        explicit constexpr Handle(
            ValueType value) noexcept
            : m_value(value)
        {
        }

        ValueType m_value{};

        friend class Internal::HandleAccess;

        template<typename>
        friend struct AZStd::hash;
    };

    template<typename Tag>
    inline constexpr Handle<Tag> Handle<Tag>::Invalid{};

    struct BodyHandleTag final
    {
        AZ_TYPE_INFO(BodyHandleTag, "{74A5936C-7EF0-4AC7-8C74-B936DD2E0688}");
    };

    struct CharacterHandleTag final
    {
        AZ_TYPE_INFO(CharacterHandleTag, "{5C305D1C-9B1E-4AA2-A6E9-AE6E5CFB97ED}");
    };

    struct ConstraintHandleTag final
    {
        AZ_TYPE_INFO(ConstraintHandleTag, "{4D0CDC9C-99C7-4F9F-81C6-D20D7251BBDE}");
    };

    struct GroupFilterHandleTag final
    {
        AZ_TYPE_INFO(GroupFilterHandleTag, "{833DEE06-607F-4F1C-B8BA-85C11372E5DB}");
    };

    struct CookedShapeHandleTag final
    {
        AZ_TYPE_INFO(CookedShapeHandleTag, "{DA4FACAB-5A88-4050-9DCC-DD24E7592251}");
    };

    struct HairHandleTag final
    {
        AZ_TYPE_INFO(HairHandleTag, "{270065B1-3FE4-423E-BA91-78D59585C958}");
    };

    struct HairDefinitionHandleTag final
    {
        AZ_TYPE_INFO(HairDefinitionHandleTag, "{E0A4376D-F1B1-4C19-85CC-E0956A816743}");
    };

    struct MaterialHandleTag final
    {
        AZ_TYPE_INFO(MaterialHandleTag, "{7677A85F-3191-4CB0-8B7C-83A5BF83A48B}");
    };

    struct PathHandleTag final
    {
        AZ_TYPE_INFO(PathHandleTag, "{EA3E2713-85B3-4B0E-8AF7-1C6A5727A9BD}");
    };

    struct RagdollHandleTag final
    {
        AZ_TYPE_INFO(RagdollHandleTag, "{2D0DA144-1DC0-4343-B1DF-ECB1BDFA3EFE}");
    };

    struct RagdollDefinitionHandleTag final
    {
        AZ_TYPE_INFO(RagdollDefinitionHandleTag, "{80B4C10A-AF53-4A57-8CD6-165E72CD8D68}");
    };

    struct SceneDefinitionHandleTag final
    {
        AZ_TYPE_INFO(SceneDefinitionHandleTag, "{7133D85F-41BC-4DC5-ADD5-AC6B4B4527C7}");
    };

    struct SceneInstanceHandleTag final
    {
        AZ_TYPE_INFO(SceneInstanceHandleTag, "{09F0D185-A391-4C3A-A34B-4A5E808C151F}");
    };

    struct ShapeHandleTag final
    {
        AZ_TYPE_INFO(ShapeHandleTag, "{824FF3F6-53DE-44CB-8805-B6773085E4C9}");
    };

    struct SkeletalAnimationHandleTag final
    {
        AZ_TYPE_INFO(SkeletalAnimationHandleTag, "{5EB8EA6D-47AF-41A3-A213-401616565661}");
    };

    struct SkeletonDefinitionHandleTag final
    {
        AZ_TYPE_INFO(SkeletonDefinitionHandleTag, "{283EA769-1AA5-42E5-AEC3-82FAFA28DF2A}");
    };

    struct SkeletonMapperHandleTag final
    {
        AZ_TYPE_INFO(SkeletonMapperHandleTag, "{241EA619-135F-45CC-97ED-866A6D80CBE4}");
    };

    struct SkeletonPoseHandleTag final
    {
        AZ_TYPE_INFO(SkeletonPoseHandleTag, "{52A46E75-C91A-48A9-B69C-7581A4297F7E}");
    };

    struct StateSnapshotHandleTag final
    {
        AZ_TYPE_INFO(StateSnapshotHandleTag, "{9EEB0BE8-F721-49EA-A4FD-AADA6E1EA1D9}");
    };

    struct SoftBodyDefinitionHandleTag final
    {
        AZ_TYPE_INFO(SoftBodyDefinitionHandleTag, "{4188CDF4-A66F-45A1-9BFD-C2E561158460}");
    };

    struct VehicleHandleTag final
    {
        AZ_TYPE_INFO(VehicleHandleTag, "{E673C996-3CF8-4200-AD48-75663DD42743}");
    };

    struct VirtualCharacterHandleTag final
    {
        AZ_TYPE_INFO(VirtualCharacterHandleTag, "{A8355609-5A4C-487E-9883-F748D4FC5DEE}");
    };

    struct WorldHandleTag final
    {
        AZ_TYPE_INFO(WorldHandleTag, "{DFB2E007-51B1-4B6C-B557-9EF60F8FB44D}");
    };

    using BodyHandle = Handle<BodyHandleTag>;
    using CharacterHandle = Handle<CharacterHandleTag>;
    using ConstraintHandle = Handle<ConstraintHandleTag>;
    using CookedShapeHandle = Handle<CookedShapeHandleTag>;
    using GroupFilterHandle = Handle<GroupFilterHandleTag>;
    using HairHandle = Handle<HairHandleTag>;
    using HairDefinitionHandle = Handle<HairDefinitionHandleTag>;
    using MaterialHandle = Handle<MaterialHandleTag>;
    using PathHandle = Handle<PathHandleTag>;
    using RagdollHandle = Handle<RagdollHandleTag>;
    using RagdollDefinitionHandle = Handle<RagdollDefinitionHandleTag>;
    using SceneDefinitionHandle = Handle<SceneDefinitionHandleTag>;
    using SceneInstanceHandle = Handle<SceneInstanceHandleTag>;
    using ShapeHandle = Handle<ShapeHandleTag>;
    using SkeletalAnimationHandle = Handle<SkeletalAnimationHandleTag>;
    using SkeletonDefinitionHandle = Handle<SkeletonDefinitionHandleTag>;
    using SkeletonMapperHandle = Handle<SkeletonMapperHandleTag>;
    using SkeletonPoseHandle = Handle<SkeletonPoseHandleTag>;
    using StateSnapshotHandle = Handle<StateSnapshotHandleTag>;
    using SoftBodyDefinitionHandle = Handle<SoftBodyDefinitionHandleTag>;
    using VehicleHandle = Handle<VehicleHandleTag>;
    using VirtualCharacterHandle = Handle<VirtualCharacterHandleTag>;
    using WorldHandle = Handle<WorldHandleTag>;
} // namespace Jolt

namespace AZ
{
    AZ_TYPE_INFO_TEMPLATE(Jolt::Handle, "{C3DF8CFF-CE27-49B2-A344-DFEE92E4F152}", AZ_TYPE_INFO_TYPENAME);
}

template<typename Tag>
struct AZStd::hash<Jolt::Handle<Tag>> final
{
    constexpr size_t operator()(
        Jolt::Handle<Tag> handle) const noexcept
    {
        return AZStd::hash<AZ::u64>{}(handle.m_value);
    }
};
