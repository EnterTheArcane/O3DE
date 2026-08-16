/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class SerializeContext;
}

namespace AZ::IO
{
    class GenericStream;
}

namespace Jolt
{
    [[nodiscard]]
    bool SaveAssetProduct(
        const AZStd::string& path,
        const void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext& serializeContext);

    [[nodiscard]]
    bool LoadAssetProduct(
        AZ::IO::GenericStream& stream,
        void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext* serializeContext = nullptr);

    [[nodiscard]]
    bool LoadAssetProductFile(
        const AZStd::string& path,
        void* asset,
        const AZ::TypeId& assetType,
        AZ::SerializeContext* serializeContext = nullptr);
} // namespace Jolt
