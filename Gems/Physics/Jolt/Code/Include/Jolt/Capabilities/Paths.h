/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Path.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Paths
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Paths* Get();

        [[nodiscard]]
        PathHandle CreatePath(const HermitePathConfiguration& configuration);

        [[nodiscard]]
        PathHandle CreatePath(const CustomPathConfiguration& configuration);

        bool DestroyPath(PathHandle pathHandle);

        [[nodiscard]]
        bool IsValid(PathHandle pathHandle) const;

        [[nodiscard]]
        bool GetPathState(
            PathHandle pathHandle,
            PathState& state) const;

        [[nodiscard]]
        bool GetCustomPathInfo(
            PathHandle pathHandle,
            CustomPathInfo& info) const;

        bool SamplePath(
            PathHandle pathHandle,
            float fraction,
            PathSample& sample) const;

        bool FindClosestPathPoint(
            PathHandle pathHandle,
            const AZ::Vector3& position,
            float fractionHint,
            PathSample& sample) const;

    private:
        friend class Runtime;

        Paths() = default;
        ~Paths() = default;
    };
} // namespace Jolt
