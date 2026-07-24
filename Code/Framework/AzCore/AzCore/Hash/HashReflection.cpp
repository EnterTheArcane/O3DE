/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Hash/HashReflection.h>

#include <AzCore/Hash/City.h>
#include <AzCore/Hash/Crc.h>
#include <AzCore/Hash/Fnv.h>
#include <AzCore/Hash/Xxh.h>
#include <AzCore/Hash/Xxh3.h>

namespace AZ
{
    void HashReflect(ReflectContext* context)
    {
        Hash::City32::Reflect(context);
        Hash::City64::Reflect(context);
        Hash::City128::Reflect(context);
        Hash::Crc32::Reflect(context);
        Hash::Crc64::Reflect(context);
        Hash::Fnv32::Reflect(context);
        Hash::Fnv64::Reflect(context);
        Hash::Xxh32::Reflect(context);
        Hash::Xxh64::Reflect(context);
        Hash::Xxh3::Reflect(context);
        Hash::Xxh128::Reflect(context);
    }
}
