/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Crc.h>

namespace AZ::ObjectStreamInternal
{
    //! Marks a leaf serializer whose invalid binary or text data must reject the containing ObjectStream element.
    inline constexpr Crc32 RejectInvalidSerializerData = AZ_CRC_CE("RejectInvalidSerializerData");
} // namespace AZ::ObjectStreamInternal
