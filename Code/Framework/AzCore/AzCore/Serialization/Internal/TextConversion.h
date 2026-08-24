/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/string/string_view.h>

namespace AZ::Internal
{
    //! Distinguishes a failed text conversion from a successful conversion of an empty value.
    inline constexpr size_t TextConversionFailure = AZStd::numeric_limits<size_t>::max();

    [[nodiscard]]
    constexpr bool IsTextUtf8Continuation(const u8 value)
    {
        return (value & 0xC0) == 0x80;
    }

    //! Returns whether text is well-formed UTF-8 in the XML 1.0 character domain.
    [[nodiscard]]
    constexpr bool IsValidXmlText(const AZStd::string_view text)
    {
        size_t offset = 0;
        while (offset < text.size())
        {
            const u8 first = static_cast<u8>(text[offset]);
            u32 codePoint = first;
            size_t encodedSize = 1;

            if (first < 0x80)
            {
                // The initial byte is the complete code point.
            }
            else if (first >= 0xC2 && first <= 0xDF)
            {
                if (offset + 1 >= text.size())
                {
                    return false;
                }
                const u8 second = static_cast<u8>(text[offset + 1]);
                if (!IsTextUtf8Continuation(second))
                {
                    return false;
                }
                codePoint = static_cast<u32>(first & 0x1F) << 6;
                codePoint |= second & 0x3F;
                encodedSize = 2;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                if (offset + 2 >= text.size())
                {
                    return false;
                }
                const u8 second = static_cast<u8>(text[offset + 1]);
                const u8 third = static_cast<u8>(text[offset + 2]);
                const bool secondIsValid =
                    (first == 0xE0 && second >= 0xA0 && second <= 0xBF)
                    || (first == 0xED && second >= 0x80 && second <= 0x9F)
                    || ((first >= 0xE1 && first <= 0xEC) && IsTextUtf8Continuation(second))
                    || ((first >= 0xEE && first <= 0xEF) && IsTextUtf8Continuation(second));
                if (!secondIsValid || !IsTextUtf8Continuation(third))
                {
                    return false;
                }
                codePoint = static_cast<u32>(first & 0x0F) << 12;
                codePoint |= static_cast<u32>(second & 0x3F) << 6;
                codePoint |= third & 0x3F;
                encodedSize = 3;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                if (offset + 3 >= text.size())
                {
                    return false;
                }
                const u8 second = static_cast<u8>(text[offset + 1]);
                const u8 third = static_cast<u8>(text[offset + 2]);
                const u8 fourth = static_cast<u8>(text[offset + 3]);
                const bool secondIsValid =
                    (first == 0xF0 && second >= 0x90 && second <= 0xBF)
                    || ((first >= 0xF1 && first <= 0xF3) && IsTextUtf8Continuation(second))
                    || (first == 0xF4 && second >= 0x80 && second <= 0x8F);
                if (!secondIsValid || !IsTextUtf8Continuation(third) || !IsTextUtf8Continuation(fourth))
                {
                    return false;
                }
                codePoint = static_cast<u32>(first & 0x07) << 18;
                codePoint |= static_cast<u32>(second & 0x3F) << 12;
                codePoint |= static_cast<u32>(third & 0x3F) << 6;
                codePoint |= fourth & 0x3F;
                encodedSize = 4;
            }
            else
            {
                return false;
            }

            const bool isSupportedControl = codePoint == 0x09 || codePoint == 0x0A || codePoint == 0x0D;
            if (codePoint < 0x20 && !isSupportedControl)
            {
                return false;
            }
            if (codePoint == 0xFFFE || codePoint == 0xFFFF)
            {
                return false;
            }

            offset += encodedSize;
        }
        return true;
    }
} // namespace AZ::Internal
