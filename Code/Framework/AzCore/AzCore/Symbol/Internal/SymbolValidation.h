/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Symbol.h>

namespace AZ::Internal
{
    enum class SymbolValidationError : u8
    {
        None,
        TooLong,
        EmbeddedNull,
        ControlCharacter,
        MalformedUtf8,
        UnsupportedXmlCharacter,
    };

    [[nodiscard]]
    constexpr bool IsUtf8Continuation(const u8 value)
    {
        return (value & 0xC0) == 0x80;
    }

    //! Validates the exact Symbol text domain without normalization or case folding.
    [[nodiscard]]
    constexpr SymbolValidationError ValidateSymbolSpelling(const AZStd::string_view value)
    {
        if (value.size() > Symbol::MaxLength)
        {
            return SymbolValidationError::TooLong;
        }

        size_t offset = 0;
        while (offset < value.size())
        {
            const u8 first = static_cast<u8>(value[offset]);
            if (first == 0)
            {
                return SymbolValidationError::EmbeddedNull;
            }
            if (first < 0x20)
            {
                return SymbolValidationError::ControlCharacter;
            }
            if (first < 0x80)
            {
                ++offset;
                continue;
            }

            if (first >= 0xC2 && first <= 0xDF)
            {
                if (offset + 1 >= value.size()
                    || !IsUtf8Continuation(static_cast<u8>(value[offset + 1])))
                {
                    return SymbolValidationError::MalformedUtf8;
                }
                offset += 2;
                continue;
            }

            if (first >= 0xE0 && first <= 0xEF)
            {
                if (offset + 2 >= value.size())
                {
                    return SymbolValidationError::MalformedUtf8;
                }

                const u8 second = static_cast<u8>(value[offset + 1]);
                const u8 third = static_cast<u8>(value[offset + 2]);
                const bool secondIsValid =
                    (first == 0xE0 && second >= 0xA0 && second <= 0xBF)
                    || (first == 0xED && second >= 0x80 && second <= 0x9F)
                    || ((first >= 0xE1 && first <= 0xEC) && IsUtf8Continuation(second))
                    || ((first >= 0xEE && first <= 0xEF) && IsUtf8Continuation(second));
                if (!secondIsValid || !IsUtf8Continuation(third))
                {
                    return SymbolValidationError::MalformedUtf8;
                }
                if (first == 0xEF && second == 0xBF && (third == 0xBE || third == 0xBF))
                {
                    return SymbolValidationError::UnsupportedXmlCharacter;
                }
                offset += 3;
                continue;
            }

            if (first >= 0xF0 && first <= 0xF4)
            {
                if (offset + 3 >= value.size())
                {
                    return SymbolValidationError::MalformedUtf8;
                }

                const u8 second = static_cast<u8>(value[offset + 1]);
                const u8 third = static_cast<u8>(value[offset + 2]);
                const u8 fourth = static_cast<u8>(value[offset + 3]);
                const bool secondIsValid =
                    (first == 0xF0 && second >= 0x90 && second <= 0xBF)
                    || ((first >= 0xF1 && first <= 0xF3) && IsUtf8Continuation(second))
                    || (first == 0xF4 && second >= 0x80 && second <= 0x8F);
                if (!secondIsValid || !IsUtf8Continuation(third) || !IsUtf8Continuation(fourth))
                {
                    return SymbolValidationError::MalformedUtf8;
                }
                offset += 4;
                continue;
            }

            return SymbolValidationError::MalformedUtf8;
        }

        return SymbolValidationError::None;
    }

    [[nodiscard]]
    constexpr const char* GetSymbolValidationErrorMessage(const SymbolValidationError error)
    {
        switch (error)
        {
        case SymbolValidationError::None:
            return "valid AZ::Symbol spelling";
        case SymbolValidationError::TooLong:
            return "AZ::Symbol spelling exceeds AZ::Symbol::MaxLength";
        case SymbolValidationError::EmbeddedNull:
            return "AZ::Symbol spelling contains an embedded null";
        case SymbolValidationError::ControlCharacter:
            return "AZ::Symbol spelling contains a C0 control character";
        case SymbolValidationError::MalformedUtf8:
            return "AZ::Symbol spelling is not valid UTF-8";
        case SymbolValidationError::UnsupportedXmlCharacter:
            return "AZ::Symbol spelling contains a character unsupported by XML ObjectStream";
        }
        return "unknown AZ::Symbol spelling error";
    }
} // namespace AZ::Internal
