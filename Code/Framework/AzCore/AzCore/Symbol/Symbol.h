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

    //! Process-local canonical string identity for exact UTF-8 bytes.
    //! Comparison is case-sensitive and performs no Unicode normalization.
    //! Non-empty values use permanent AzCore-owned storage charged to one global requested-byte budget.
    //! Symbol is intended for low-cardinality identifiers, not unbounded external input.
    //! Equality compares canonical entry pointers.
    //! Pointers and randomized table hashes are process/module-instance details.
    //! Do not persist, transmit, or compare them across module reloads.
    //! Binary and JSON persistence preserve the value bytes.
    //! XML escapes values its text format cannot represent.
    //! Loading reconstructs identity.
    //! AZ::Name remains a separate choice when reference-counted storage is needed.
    //! A Symbol remains valid only while its owning AzCore module instance remains loaded.
    //! Construction and first literal resolution can initialize entropy, allocate, and lock.
    //! Do not intern Symbols from DllMain or global/static initialization in a dynamically loaded module.
    //! Defer resolution until after load.
    class AZCORE_API Symbol final
    {
    public:
        AZ_TYPE_INFO(Symbol, "{AEF1422B-7C39-4F96-BE22-33D69A8021C7}");

        constexpr Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;

        //! Invalid values terminate with a validation diagnostic.
        //! Admission-policy rejection reports the requested length and configured limit.
        //! Storage exhaustion terminates with requested/used/limit byte counts.
        explicit Symbol(AZStd::string_view value);

        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        ~Symbol() = default;

        static void Reflect(ReflectContext* context);

        //! Validates UTF-8 and excludes U+0000.
        //! Creation additionally requires that the value fits the build's admission policy and the global storage budget.
        //! Integrations, including networking, impose their own smaller limits and reject unsupported values without truncation.
        [[nodiscard]]
        static constexpr bool IsValid(AZStd::string_view value)
        {
            return Internal::ValidateSymbolValue(value) == Internal::SymbolValidationError::None;
        }

        [[nodiscard]]
        static Symbol Create(AZStd::string_view value);

        //! Returns an engaged empty Symbol for an empty value.
        //! Returns no value for invalid input, admission-policy rejection, or unavailable storage without a diagnostic.
        [[nodiscard]]
        static AZStd::optional<Symbol> TryCreate(AZStd::string_view value);

        //! Looks up without admitting new permanent storage.
        //! Empty is always found as an engaged empty Symbol.
        [[nodiscard]]
        static AZStd::optional<Symbol> Find(AZStd::string_view value);

        //! Preferred length-aware access.
        //! The returned view remains valid for the Symbol storage lifetime.
        [[nodiscard]]
        AZStd::string_view GetStringView() const;

        //! Returns a NUL-terminated pointer.
        //! The empty Symbol returns a pointer to an empty string.
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
        friend struct SymbolHash;
    };

    struct SymbolHash final
    {
        [[nodiscard]]
        size_t operator()(Symbol value) const
        {
            return AZStd::hash<const Internal::SymbolEntry*>{}(value.m_entry);
        }
    };

    struct SymbolEqual final
    {
        [[nodiscard]]
        constexpr bool operator()(Symbol lhs, Symbol rhs) const
        {
            return lhs == rhs;
        }
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
