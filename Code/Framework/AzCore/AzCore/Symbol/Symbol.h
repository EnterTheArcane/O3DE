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
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/std/optional.h>
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
        struct SymbolAccess;
    } // namespace Internal

    //! Process-local identity for exact UTF-8 bytes, with case-sensitive comparison and no normalization.
    //! Empty is a valid value and the default.
    //! Intended for low-cardinality identifiers because interned values occupy permanent, budgeted storage.
    //! Equality and hashing use entry pointers, so persist and transmit value bytes.
    //! Symbols and their views remain valid only while the owning AzCore module is loaded.
    //! Interning and first literal resolution can acquire entropy, allocate, and lock.
    //! Do not intern from DllMain or static initializers in a dynamically loaded module.
    //! Use AZ::Name when reference-counted storage is needed.
    class AZCORE_API Symbol final
    {
    public:
        AZ_TYPE_INFO(Symbol, "{AEF1422B-7C39-4F96-BE22-33D69A8021C7}");

        constexpr Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;

        //! Terminates with a diagnostic if validation, admission policy, or storage allocation fails.
        explicit Symbol(AZStd::string_view value);

        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        ~Symbol() = default;

        static void Reflect(ReflectContext* context);

        //! Checks UTF-8 validity and rejects embedded NUL.
        //! Storage admission and integration-specific limits are checked separately.
        [[nodiscard]]
        static constexpr bool IsValid(AZStd::string_view value)
        {
            return Internal::ValidateSymbolValue(value) == Internal::SymbolValidationError::None;
        }

        //! Equivalent to Symbol(value).
        [[nodiscard]]
        static Symbol Create(AZStd::string_view value);

        //! Returns an engaged empty Symbol for an empty value.
        //! Returns no value on validation or storage admission failure, without a diagnostic.
        [[nodiscard]]
        static AZStd::optional<Symbol> TryCreate(AZStd::string_view value);

        //! Looks up without admitting new permanent storage.
        //! Empty is always found as an engaged empty Symbol.
        [[nodiscard]]
        static AZStd::optional<Symbol> Find(AZStd::string_view value);

        //! Returns a view of the value bytes.
        [[nodiscard]]
        AZStd::string_view GetStringView() const;

        //! Returns a pointer to the NUL-terminated value, or an empty string for an empty Symbol.
        [[nodiscard]]
        const char* GetCStr() const;

        [[nodiscard]]
        constexpr bool IsEmpty() const
        {
            return !m_entry;
        }

        friend constexpr bool operator==(Symbol lhs, Symbol rhs)
        {
            return lhs.m_entry == rhs.m_entry;
        }

    private:
        explicit constexpr Symbol(const Internal::SymbolEntry* entry)
            : m_entry{entry}
        {
        }

        static void ScriptConstructor(Symbol* thisPtr, ScriptDataContext& dataContext);

        const Internal::SymbolEntry* m_entry = nullptr;

        friend struct Internal::SymbolAccess;
        friend struct AZStd::hash<Symbol>;
    };
} // namespace AZ

template<>
struct AZStd::hash<AZ::Symbol>
{
    [[nodiscard]]
    size_t operator()(const AZ::Symbol value) const
    {
        return AZStd::hash<const AZ::Internal::SymbolEntry*>{}(value.m_entry);
    }
};
