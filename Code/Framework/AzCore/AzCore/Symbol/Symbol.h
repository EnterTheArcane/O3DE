/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/string/string_view.h>

namespace AZ
{
    class ReflectContext;
    class ScriptDataContext;
    class Symbol;

    namespace Internal
    {
        struct SymbolEntry;

        //! Interns spelling already checked by ValidateSymbolSpelling. Allocation failure terminates.
        [[nodiscard]]
        AZCORE_API Symbol InternValidatedSymbol(AZStd::string_view value);

        //! Looks up spelling without creating permanent storage. Empty is always found as the null Symbol.
        [[nodiscard]]
        AZCORE_API bool FindSymbol(Symbol& result, AZStd::string_view value);
    } // namespace Internal

    //! Process-local canonical string identity with permanent, AzCore-owned spelling storage.
    //! Equality compares canonical entry pointers.
    //! Persist or transmit the complete spelling, never the pointer or implementation hash.
    class AZCORE_API Symbol final
    {
    public:
        AZ_TYPE_INFO(Symbol, "{AEF1422B-7C39-4F96-BE22-33D69A8021C7}");

        constexpr Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;

        //! Invalid spellings are programming errors and terminate in every build configuration.
        explicit Symbol(AZStd::string_view value);

        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        ~Symbol() = default;

        static void Reflect(ReflectContext* context);

        [[nodiscard]]
        AZStd::string_view GetStringView() const;

        [[nodiscard]]
        const char* GetCStr() const;

        [[nodiscard]]
        bool IsEmpty() const;

        [[nodiscard]]
        explicit constexpr operator bool() const
        {
            return m_entry;
        }

        friend constexpr bool operator==(Symbol lhs, Symbol rhs)
        {
            return lhs.m_entry == rhs.m_entry;
        }

        //! Maximum number of bytes in a Symbol spelling, excluding the trailing null terminator.
        static constexpr size_t MaxLength = 1023;

    private:
        explicit constexpr Symbol(const Internal::SymbolEntry* entry)
            : m_entry{entry}
        {
        }

        static void ScriptConstructor(Symbol* thisPtr, ScriptDataContext& dataContext);

        const Internal::SymbolEntry* m_entry = nullptr;

        friend Symbol Internal::InternValidatedSymbol(AZStd::string_view value);
        friend bool Internal::FindSymbol(Symbol& result, AZStd::string_view value);
        friend struct SymbolHash;
    };

    struct AZCORE_API SymbolHash final
    {
        [[nodiscard]]
        size_t operator()(Symbol value) const;
    };

    struct AZCORE_API SymbolEqual final
    {
        [[nodiscard]]
        bool operator()(Symbol lhs, Symbol rhs) const;
    };
} // namespace AZ

template<>
struct AZStd::hash<AZ::Symbol>
{
    [[nodiscard]]
    size_t operator()(const AZ::Symbol value) const
    {
        return AZ::SymbolHash{}(value);
    }
};
