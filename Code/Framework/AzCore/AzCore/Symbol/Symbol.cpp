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
#include <AzCore/Script/lua/lua.h>
#include <AzCore/Serialization/Internal/ObjectStreamAttributes.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Symbol/Internal/SymbolEntry.h>
#include <AzCore/Symbol/Internal/SymbolFailure.h>
#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Internal/SymbolTable.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/SymbolJsonSerializer.h>
#include <AzCore/Symbol/SymbolSerializer.h>
#include <AzCore/std/typetraits/is_destructible.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <new>
#include <type_traits>

#define XXH_INLINE_ALL
#include <xxhash.h>
#undef XXH_INLINE_ALL

namespace AZ
{
    static_assert(sizeof(Symbol) == sizeof(void*));
    static_assert(std::is_standard_layout_v<Symbol>);
    static_assert(AZStd::is_trivially_copyable_v<Symbol>);
    static_assert(AZStd::is_trivially_destructible_v<Symbol>);

    Symbol::Symbol(const AZStd::string_view value)
    {
        if (!Internal::TryCreateSymbol(*this, value))
        {
            const Internal::SymbolValidationError error = Internal::ValidateSymbolValue(value, MaxStringSize);
            if (error != Internal::SymbolValidationError::None)
            {
                Internal::FailSymbol(Internal::GetSymbolValidationErrorMessage(error));
            }
            Internal::FailSymbol("AZ::Symbol storage budget or allocation exhausted");
        }
    }

    Symbol Symbol::Create(const AZStd::string_view value)
    {
        return Symbol{value};
    }

    AZStd::optional<Symbol> Symbol::TryCreate(const AZStd::string_view value)
    {
        Symbol symbol;
        if (!Internal::TryCreateSymbol(symbol, value))
        {
            return AZStd::nullopt;
        }
        return symbol;
    }

    AZStd::optional<Symbol> Symbol::Find(const AZStd::string_view value)
    {
        Symbol symbol;
        if (!Internal::FindSymbol(symbol, value))
        {
            return AZStd::nullopt;
        }
        return symbol;
    }

    void Symbol::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<Symbol>()
                ->Serializer<SymbolSerializer>()
                ->Attribute(ObjectStreamInternal::RejectInvalidSerializerData, true);
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
            size_t valueLength = 0;
            const char* value = lua_tolstring(
                dataContext.GetNativeContext(),
                dataContext.GetStartIndex(),
                &valueLength);
            if (value)
            {
                const AZStd::optional<Symbol> symbol = TryCreate(AZStd::string_view{value, valueLength});
                if (symbol)
                {
                    ::new (thisPtr) Symbol{*symbol};
                    return;
                }
            }
        }

        dataContext.GetScriptContext()->Error(
            ScriptContext::ErrorType::Error,
            true,
            "Symbol constructor expects valid UTF-8 text without U+0000 and no longer than %zu bytes",
            MaxStringSize);
        ::new (thisPtr) Symbol{};
    }

    namespace Internal
    {
        struct SymbolAccess final
        {
            [[nodiscard]]
            static Symbol FromEntry(const SymbolEntry* entry)
            {
                return Symbol{entry};
            }
        };

        Symbol InternValidatedSymbol(const AZStd::string_view value)
        {
            if (value.empty())
            {
                return {};
            }

            const u64 hash = XXH3_64bits(value.data(), value.size());
            const SymbolEntry* entry = SymbolTable::Instance().TryIntern(value, hash);
            if (!entry)
            {
                FailSymbol("AZ::Symbol storage budget or allocation exhausted");
            }
            return SymbolAccess::FromEntry(entry);
        }

        bool TryCreateSymbol(
            Symbol& result,
            const AZStd::string_view value)
        {
            if (value.size() > Symbol::MaxStringSize)
            {
                return false;
            }
            if (value.empty())
            {
                result = {};
                return true;
            }

            const u64 hash = XXH3_64bits(value.data(), value.size());
            SymbolTable& table = SymbolTable::Instance();
            if (const SymbolEntry* entry = table.Find(value, hash))
            {
                result = SymbolAccess::FromEntry(entry);
                return true;
            }

            if (ValidateSymbolValue(value, Symbol::MaxStringSize) != SymbolValidationError::None)
            {
                return false;
            }

            const SymbolEntry* entry = table.TryIntern(value, hash);
            if (!entry)
            {
                return false;
            }
            result = SymbolAccess::FromEntry(entry);
            return true;
        }

        bool FindSymbol(
            Symbol& result,
            const AZStd::string_view value)
        {
            if (value.size() > Symbol::MaxStringSize)
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
            result = SymbolAccess::FromEntry(entry);
            return true;
        }
    } // namespace Internal
} // namespace AZ
