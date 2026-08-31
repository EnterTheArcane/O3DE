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

    //! Process-local canonical string identity for exact UTF-8 bytes. Comparison is case-sensitive; no Unicode normalization is performed.
    //! Non-empty values use permanent, AzCore-owned storage charged to one global requested-byte budget. Symbol is intended for bounded,
    //! low-cardinality identifiers, not unbounded external input. Equality compares canonical entry pointers.
    //! The pointer and randomized table hash are process/module-instance details: do not persist, transmit, or compare them across module reloads.
    //! Reflected binary, XML, and JSON persistence writes the raw value and reconstructs identity on load. A Symbol remains valid only while
    //! its owning AzCore module instance remains loaded. Construction and first literal resolution can initialize entropy, allocate, and lock.
    //! Do not intern Symbols from DllMain or from global/static initialization in a dynamically loaded module; defer resolution until after load.
    class AZCORE_API Symbol final
    {
    public:
        AZ_TYPE_INFO(Symbol, "{AEF1422B-7C39-4F96-BE22-33D69A8021C7}");

        constexpr Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;

        //! Invalid values terminate with a validation diagnostic. Storage exhaustion terminates with requested/used/limit byte counts.
        explicit Symbol(AZStd::string_view value);

        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        ~Symbol() = default;

        static void Reflect(ReflectContext* context);

        //! Maximum bytes reserved for a value and its required trailing NUL.
        static constexpr size_t MaxStringBufferSize = 1024;
        //! Maximum bytes in the UTF-8 value itself. Integrations with smaller encodings must enforce their own lower limit.
        static constexpr size_t MaxStringSize = MaxStringBufferSize - 1;

        [[nodiscard]]
        static constexpr bool IsValid(AZStd::string_view value)
        {
            return Internal::ValidateSymbolValue(value, MaxStringSize) == Internal::SymbolValidationError::None;
        }

        [[nodiscard]]
        static Symbol Create(AZStd::string_view value);

        //! Returns an engaged empty Symbol for an empty value. Returns no value for invalid input or storage exhaustion without a diagnostic.
        [[nodiscard]]
        static AZStd::optional<Symbol> TryCreate(AZStd::string_view value);

        //! Looks up without admitting new permanent storage. Empty is always found as an engaged empty Symbol.
        [[nodiscard]]
        static AZStd::optional<Symbol> Find(AZStd::string_view value);

        //! Preferred length-aware access. The returned view remains valid for the Symbol storage lifetime.
        [[nodiscard]]
        AZStd::string_view GetStringView() const;

        //! Returns a NUL-terminated pointer. The empty Symbol returns a pointer to an empty string.
        [[nodiscard]]
        const char* GetCStr() const;

        [[nodiscard]]
        bool IsEmpty() const;

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
