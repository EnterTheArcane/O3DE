/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/System.h>

#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    [[nodiscard]]
    AZStd::unique_ptr<ISystem> CreateAssetBuilderSystem();
} // namespace Jolt
