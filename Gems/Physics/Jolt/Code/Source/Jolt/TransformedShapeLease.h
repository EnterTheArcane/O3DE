/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>

#include <AzCore/std/parallel/atomic.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

namespace Jolt::Internal
{
    struct TransformedShapeLeaseRecord final
    {
        JPH::TransformedShape m_nativeShape;
        WorldPosition m_worldOrigin;

        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        MaterialHandle m_materialHandle;
        ShapeHandle m_shapeHandle;

        AZ::u64 m_userData = 0;
        ShapeKind m_shapeKind = ShapeKind::None;
        void* m_owner = nullptr;
        AZStd::atomic_uint64_t m_referenceCount{1};
        TransformedShapeLeaseRecord* m_nextFree = nullptr;
    };

    void AcquireTransformedShapeLease(
        void* record);

    void ReleaseTransformedShapeLease(
        void* record);
} // namespace Jolt::Internal
