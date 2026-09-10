/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/Symbol.h>

namespace AZ::Internal
{
    template<size_t Size>
    struct SymbolLiteral final
    {
        char m_value[Size];

        consteval SymbolLiteral(const char (&value)[Size])
        {
            for (size_t index = 0; index < Size; ++index)
            {
                m_value[index] = value[index];
            }
        }

        [[nodiscard]]
        static consteval size_t GetSize()
        {
            return Size - 1;
        }
    };

    template<SymbolLiteral Literal>
    [[nodiscard]]
    AZ_FORCE_INLINE Symbol GetLiteralSymbol()
    {
        constexpr AZStd::string_view value{Literal.m_value, Literal.GetSize()};
        static_assert(
            Symbol::IsValid(value),
            "AZ::Symbol literal is outside the supported text domain");

        if constexpr (Literal.GetSize() == 0)
        {
            return Symbol{};
        }

        static const Symbol symbol = InternValidatedSymbol(value);
        return symbol;
    }
} // namespace AZ::Internal

namespace AZ::Literals
{
    //! Validates UTF-8 at compile time and caches the Symbol on first use.
    //! First use can allocate and lock, and storage admission failure terminates.
    template<Internal::SymbolLiteral Literal>
    [[nodiscard]]
    AZ_FORCE_INLINE Symbol operator""_sym()
    {
        return Internal::GetLiteralSymbol<Literal>();
    }
} // namespace AZ::Literals
