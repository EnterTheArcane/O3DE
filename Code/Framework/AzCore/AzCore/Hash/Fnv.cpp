/*
* Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "Fnv.h"

#include <AzCore/std/containers/map.h>
#include <AzCore/std/string/string.h>

static AZStd::map<AZ::u32, AZStd::string> s_debugStrings_Fnv32;
static AZStd::map<AZ::u64, AZStd::string> s_debugStrings_Fnv64;

void AZ::Hash::Fnv32::DebugString([[maybe_unused]] const AZStd::string_view str) const
{
    s_debugStrings_Fnv32[m_value] = str;
}

void AZ::Hash::Fnv64::DebugString([[maybe_unused]] const AZStd::string_view str) const
{
    s_debugStrings_Fnv64[m_value] = str;
}
