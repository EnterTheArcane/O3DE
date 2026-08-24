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

    //! Process-local canonical string identity with permanent, AzCore-owned string storage.
    //! Equality compares canonical entry pointers.
    //! Persist or transmit the complete value, never the pointer or implementation hash.
    class AZCORE_API Symbol final
    {
    public:
        AZ_TYPE_INFO(Symbol, "{AEF1422B-7C39-4F96-BE22-33D69A8021C7}");

        constexpr Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;

        //! Invalid values and storage failures are programming errors and terminate in every build configuration.
        explicit Symbol(AZStd::string_view value);

        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        ~Symbol() = default;

        static void Reflect(ReflectContext* context);

        //! Maximum bytes reserved for a value and its required trailing NUL.
        static constexpr size_t MaxStringBufferSize = 1024;
        //! Maximum bytes in the UTF-8 value itself.
        static constexpr size_t MaxStringSize = MaxStringBufferSize - 1;

        [[nodiscard]]
        static constexpr bool IsValid(AZStd::string_view value)
        {
            return Internal::ValidateSymbolValue(value, MaxStringSize) == Internal::SymbolValidationError::None;
        }

        [[nodiscard]]
        static Symbol Create(AZStd::string_view value);

        [[nodiscard]]
        static AZStd::optional<Symbol> TryCreate(AZStd::string_view value);

        [[nodiscard]]
        static AZStd::optional<Symbol> Find(AZStd::string_view value);

        [[nodiscard]]
        AZStd::string_view GetStringView() const;

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
