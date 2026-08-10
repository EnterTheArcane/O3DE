/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Queries.h>

#include <Box3D/Internal/HandleEncoding.h>
#include <Box3D/SystemInternal.h>
#include <Box3D/World.h>

#include <type_traits>

namespace Box3D
{
    static_assert(Internal::MaximumWorldCount == AZ::u32{1} << Internal::WorldIndexBits);

    static_assert(sizeof(WorldHandle) == sizeof(AZ::u64));
    static_assert(sizeof(BodyHandle) == sizeof(AZ::u64));
    static_assert(sizeof(ShapeHandle) == sizeof(AZ::u64));
    static_assert(sizeof(JointHandle) == sizeof(AZ::u64));
    static_assert(sizeof(MaterialHandle) == sizeof(AZ::u64));
    static_assert(sizeof(CharacterHandle) == sizeof(AZ::u64));
    static_assert(sizeof(CookedShapeHandle) == sizeof(AZ::u64));
    static_assert(sizeof(ReplayHandle) == sizeof(AZ::u64));

    static_assert(!WorldHandle::Invalid);

    static_assert(std::is_trivially_copyable_v<WorldHandle>);
    static_assert(std::is_trivially_copyable_v<BodyHandle>);
    static_assert(std::is_trivially_copyable_v<ShapeHandle>);
    static_assert(std::is_trivially_copyable_v<JointHandle>);
    static_assert(std::is_trivially_copyable_v<MaterialHandle>);
    static_assert(std::is_trivially_copyable_v<CharacterHandle>);
    static_assert(std::is_trivially_copyable_v<CookedShapeHandle>);
    static_assert(std::is_trivially_copyable_v<ReplayHandle>);

    static_assert(sizeof(OverlapHit) == 2 * sizeof(AZ::u64));
    static_assert(std::is_trivially_copyable_v<OverlapHit>);

    struct WorldLayoutChecks final
    {
        static_assert(sizeof(World::BodySlot) <= 32);
        static_assert(sizeof(World::ShapeSlot) <= 32);
        static_assert(sizeof(World::ShapeResources) <= 96);
        static_assert(sizeof(World::JointSlot) <= 24);
        static_assert(sizeof(World::CharacterSlot) <= 32);
        static_assert(sizeof(World::CharacterResources) <= 304);
    };

    struct SystemLayoutChecks final
    {
        static_assert(sizeof(System::MaterialSlot) <= 4);
        static_assert(sizeof(System::CookedShapeSlot) <= 4);
        static_assert(sizeof(System::CookedShapeResources) <= 96);
    };
} // namespace Box3D
