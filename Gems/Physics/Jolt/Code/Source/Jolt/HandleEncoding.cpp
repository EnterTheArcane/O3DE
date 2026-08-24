/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HandleEncoding.h>

namespace Jolt::Internal
{
    namespace
    {
        constexpr bool HandleKindTraitsAreExhaustiveAndUnique() noexcept
        {
            constexpr AZStd::array<HandleKind, HandleKindCount> handleKinds = {
                HandleKindTraits<BodyHandle>::Kind,
                HandleKindTraits<CharacterHandle>::Kind,
                HandleKindTraits<ConstraintHandle>::Kind,
                HandleKindTraits<CookedShapeHandle>::Kind,
                HandleKindTraits<ExtensionHandle>::Kind,
                HandleKindTraits<GroupFilterHandle>::Kind,
                HandleKindTraits<HairHandle>::Kind,
                HandleKindTraits<HairDefinitionHandle>::Kind,
                HandleKindTraits<MaterialHandle>::Kind,
                HandleKindTraits<PathHandle>::Kind,
                HandleKindTraits<RagdollHandle>::Kind,
                HandleKindTraits<RagdollDefinitionHandle>::Kind,
                HandleKindTraits<SceneDefinitionHandle>::Kind,
                HandleKindTraits<SceneInstanceHandle>::Kind,
                HandleKindTraits<ShapeHandle>::Kind,
                HandleKindTraits<SkeletalAnimationHandle>::Kind,
                HandleKindTraits<SkeletonDefinitionHandle>::Kind,
                HandleKindTraits<SkeletonMapperHandle>::Kind,
                HandleKindTraits<SkeletonPoseHandle>::Kind,
                HandleKindTraits<StateSnapshotHandle>::Kind,
                HandleKindTraits<SoftBodyDefinitionHandle>::Kind,
                HandleKindTraits<VehicleHandle>::Kind,
                HandleKindTraits<VirtualCharacterHandle>::Kind,
                HandleKindTraits<WorldHandle>::Kind,
            };

            for (size_t kindIndex = 0; kindIndex < handleKinds.size(); ++kindIndex)
            {
                const size_t handleKind = static_cast<size_t>(handleKinds[kindIndex]);
                if (handleKind == static_cast<size_t>(HandleKind::None) || handleKind >= static_cast<size_t>(HandleKind::Count))
                {
                    return false;
                }
                for (size_t otherKindIndex = kindIndex + 1; otherKindIndex < handleKinds.size(); ++otherKindIndex)
                {
                    if (handleKinds[kindIndex] == handleKinds[otherKindIndex])
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        static_assert(HandleKindCount == 24);
        static_assert(HandleKindTraitsAreExhaustiveAndUnique());

        AtomicGenerationSources ModuleHandleGenerationSources;
    } // namespace

    AZ::u32 AcquireHandleGeneration(const HandleKind handleKind) noexcept
    {
        const size_t handleKindValue = static_cast<size_t>(handleKind);
        if (handleKindValue == static_cast<size_t>(HandleKind::None) || handleKindValue >= static_cast<size_t>(HandleKind::Count))
        {
            return 0;
        }
        const size_t kindIndex = handleKindValue - 1;
        return ModuleHandleGenerationSources[kindIndex].Acquire();
    }
} // namespace Jolt::Internal
