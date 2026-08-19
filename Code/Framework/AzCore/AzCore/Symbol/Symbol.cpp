/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Symbol.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Symbol/Internal/SymbolEntry.h>
#include <AzCore/Symbol/Internal/SymbolFailure.h>
#include <AzCore/Symbol/Internal/SymbolTable.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/SymbolJsonSerializer.h>
#include <AzCore/Symbol/SymbolSerializer.h>
#include <AzCore/std/typetraits/is_destructible.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <new>
#include <type_traits>
#include <xxhash.h>

namespace AZ
{
    static_assert(sizeof(Symbol) == sizeof(void*));
    static_assert(std::is_standard_layout_v<Symbol>);
    static_assert(AZStd::is_trivially_copyable_v<Symbol>);
    static_assert(AZStd::is_trivially_destructible_v<Symbol>);

    Symbol::Symbol(const AZStd::string_view value)
    {
        const Internal::SymbolValidationError error = Internal::ValidateSymbolSpelling(value);
        if (error != Internal::SymbolValidationError::None)
        {
            Internal::FailSymbol(Internal::GetSymbolValidationErrorMessage(error));
        }
        *this = Internal::InternValidatedSymbol(value);
    }

    void Symbol::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<Symbol>()
                ->Serializer<SymbolSerializer>();
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<Symbol>("Symbol")
                ->Attribute(Script::Attributes::Scope, Script::Attributes::ScopeFlags::Common)
                ->Attribute(Script::Attributes::Module, "symbol")
                ->Attribute(Script::Attributes::Storage, Script::Attributes::StorageType::Value)
                ->Attribute(Script::Attributes::ConstructorOverride, &Symbol::ScriptConstructor)
                ->Constructor()
                ->Method("ToString", &Symbol::GetCStr)
                ->Method("__repr__", &Symbol::GetCStr)
                ->Method("IsEmpty", &Symbol::IsEmpty)
                ->Method(
                    "Equal",
                    [](const Symbol lhs, const Symbol rhs)
                    {
                        return lhs == rhs;
                    })
                ->Attribute(Script::Attributes::Operator, Script::Attributes::OperatorType::Equal);
        }

        if (auto* jsonContext = azrtti_cast<JsonRegistrationContext*>(context))
        {
            jsonContext->Serializer<SymbolJsonSerializer>()
                ->HandlesType<Symbol>();
        }
    }

    AZStd::string_view Symbol::GetStringView() const
    {
        if (!m_entry)
        {
            return {};
        }
        return AZStd::string_view{m_entry->GetData(), m_entry->m_size};
    }

    const char* Symbol::GetCStr() const
    {
        if (!m_entry)
        {
            return "";
        }
        return m_entry->GetData();
    }

    bool Symbol::IsEmpty() const
    {
        return !m_entry;
    }

    void Symbol::ScriptConstructor(
        Symbol* thisPtr,
        ScriptDataContext& dataContext)
    {
        if (dataContext.GetNumArguments() == 0)
        {
            ::new (thisPtr) Symbol{};
            return;
        }

        if (dataContext.GetNumArguments() == 1 && dataContext.IsString(0, false))
        {
            const char* value = nullptr;
            dataContext.ReadArg<const char*>(0, value);
            if (value)
            {
                const size_t valueLength = strnlen(value, MaxLength + 1);
                const AZStd::string_view spelling{value, valueLength};
                if (Internal::ValidateSymbolSpelling(spelling) == Internal::SymbolValidationError::None)
                {
                    ::new (thisPtr) Symbol{spelling};
                    return;
                }
            }
        }

        dataContext.GetScriptContext()->Error(
            ScriptContext::ErrorType::Error,
            true,
            "Symbol constructor expects valid UTF-8 text no longer than %zu bytes",
            MaxLength);
        ::new (thisPtr) Symbol{};
    }

    size_t SymbolHash::operator()(const Symbol value) const
    {
        return reinterpret_cast<size_t>(value.m_entry);
    }

    bool SymbolEqual::operator()(
        const Symbol lhs,
        const Symbol rhs) const
    {
        return lhs == rhs;
    }

    namespace Internal
    {
        Symbol InternValidatedSymbol(const AZStd::string_view value)
        {
            if (value.empty())
            {
                return {};
            }

            const SymbolValidationError error = ValidateSymbolSpelling(value);
            if (error != SymbolValidationError::None)
            {
                FailSymbol(GetSymbolValidationErrorMessage(error));
            }

            const u64 hash = XXH3_64bits(value.data(), value.size());
            return Symbol{SymbolTable::Instance().Intern(value, hash)};
        }

        bool FindSymbol(
            Symbol& result,
            const AZStd::string_view value)
        {
            if (ValidateSymbolSpelling(value) != SymbolValidationError::None)
            {
                return false;
            }
            if (value.empty())
            {
                result = {};
                return true;
            }

            const u64 hash = XXH3_64bits(value.data(), value.size());
            const SymbolEntry* entry = SymbolTable::Instance().Find(value, hash);
            if (!entry)
            {
                return false;
            }
            result = Symbol{entry};
            return true;
        }
    } // namespace Internal
} // namespace AZ
