/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/string/string.h>

#include <cctype>
#include <cstring>

namespace LyShine::StringHashCompatibility
{
    inline const char* ToString(const char* value)
    {
        return value;
    }

    inline const char* ToString(const AZStd::string& value)
    {
        return value.c_str();
    }

    //! Retains the polynomial hash used by the old CryCommon maps. Changing the hash would alter
    //! bucket and iteration order even when key equality remains unchanged.
    template<class Key, bool CaseInsensitive>
    struct Hash
    {
        size_t operator()(const Key& key) const
        {
            unsigned int hash = 0;
            for (const char* character = ToString(key); *character; ++character)
            {
                const unsigned char value = static_cast<unsigned char>(*character);
                hash = 5 * hash + (CaseInsensitive ? std::tolower(value) : value);
            }
            return static_cast<size_t>(hash);
        }
    };

    template<class Key, bool CaseInsensitive>
    struct Equal
    {
        bool operator()(const Key& left, const Key& right) const
        {
            if constexpr (CaseInsensitive)
            {
                return azstricmp(ToString(left), ToString(right)) == 0;
            }
            else
            {
                return std::strcmp(ToString(left), ToString(right)) == 0;
            }
        }
    };
} // namespace LyShine::StringHashCompatibility
