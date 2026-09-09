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
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Symbol/Internal/SymbolEntry.h>
#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Internal/SymbolStorageBudget.h>
#include <AzCore/Symbol/Internal/SymbolTable.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/SymbolJsonSerializer.h>
#include <AzCore/Symbol/SymbolSerializer.h>
#include <AzCore/std/createdestroy.h>
#include <AzCore/std/typetraits/is_destructible.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace AZ
{
    static_assert(sizeof(Symbol) == sizeof(void*));
    static_assert(std::is_standard_layout_v<Symbol>);
    static_assert(AZStd::is_trivially_copyable_v<Symbol>);
    static_assert(AZStd::is_trivially_destructible_v<Symbol>);
    static_assert(sizeof(Internal::SymbolEntry) == 16);

    namespace
    {
        [[noreturn]] void FailSymbolCreation(const AZStd::string_view value)
        {
            char reason[256];
            if (!Internal::IsSymbolValueSizeAllowed(value.size()))
            {
                std::snprintf(
                    reason,
                    sizeof(reason),
                    "AZ::Symbol value exceeds the configured limit: valueBytes=%zu limitBytes=%zu",
                    value.size(),
                    Internal::SymbolValueSizeLimit);
            }
            else if (const Internal::SymbolValidationError error = Internal::ValidateSymbolValue(value);
                error != Internal::SymbolValidationError::None)
            {
                std::snprintf(reason, sizeof(reason), "%s", Internal::GetSymbolValidationErrorMessage(error));
            }
            else
            {
                const Internal::SymbolStorageStats stats = Internal::GetSymbolStorageStats();
                std::snprintf(
                    reason,
                    sizeof(reason),
                    "AZ::Symbol creation failed: valueBytes=%zu storageUsedBytes=%zu storageLimitBytes=%zu",
                    value.size(),
                    stats.m_usedByteCount,
                    stats.m_limitByteCount);
            }
            std::fprintf(stderr, "%s\n", reason);
            std::fflush(stderr);
            AZ_Assert(false, "%s", reason);
            std::abort();
        }
    }

    Symbol::Symbol(const AZStd::string_view value)
    {
        if (!Internal::TryCreateSymbol(*this, value))
        {
            FailSymbolCreation(value);
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
                ->Constructor<AZStd::string_view>()
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
        return AZStd::string_view{m_entry->GetData(), static_cast<size_t>(m_entry->m_size)};
    }

    const char* Symbol::GetCStr() const
    {
        if (!m_entry)
        {
            return "";
        }
        return m_entry->GetData();
    }

    void Symbol::ScriptConstructor(
        Symbol* thisPtr,
        ScriptDataContext& dataContext)
    {
        if (dataContext.GetNumArguments() == 0)
        {
            AZStd::construct_at(thisPtr);
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
                const AZStd::string_view valueView{value, valueLength};
                if (!Internal::IsSymbolValueSizeAllowed(valueLength))
                {
                    dataContext.GetScriptContext()->Error(
                        ScriptContext::ErrorType::Error,
                        true,
                        "Symbol constructor rejected the value: valueBytes=%zu configuredLimitBytes=%zu",
                        valueLength,
                        Internal::SymbolValueSizeLimit);
                    AZStd::construct_at(thisPtr);
                    return;
                }
                const Internal::SymbolValidationError validationError =
                    Internal::ValidateSymbolValue(valueView);
                if (validationError == Internal::SymbolValidationError::None)
                {
                    const AZStd::optional<Symbol> symbol = TryCreate(valueView);
                    if (symbol)
                    {
                        AZStd::construct_at(thisPtr, *symbol);
                        return;
                    }

                    const Internal::SymbolStorageStats stats = Internal::GetSymbolStorageStats();
                    dataContext.GetScriptContext()->Error(
                        ScriptContext::ErrorType::Error,
                        true,
                        "Symbol constructor could not allocate permanent storage: requestedValueBytes=%zu "
                        "usedBytes=%zu limitBytes=%zu",
                        valueLength,
                        stats.m_usedByteCount,
                        stats.m_limitByteCount);
                    AZStd::construct_at(thisPtr);
                    return;
                }

                dataContext.GetScriptContext()->Error(
                    ScriptContext::ErrorType::Error,
                    true,
                    "Symbol constructor rejected the value: %s",
                    Internal::GetSymbolValidationErrorMessage(validationError));
                AZStd::construct_at(thisPtr);
                return;
            }
        }

        dataContext.GetScriptContext()->Error(
            ScriptContext::ErrorType::Error,
            true,
            "Symbol constructor expects valid UTF-8 text without U+0000");
        AZStd::construct_at(thisPtr);
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

            const SymbolEntry* entry = SymbolTable::Instance().InternValidated(value);
            if (!entry)
            {
                FailSymbolCreation(value);
            }
            return SymbolAccess::FromEntry(entry);
        }

        bool TryCreateSymbol(
            Symbol& result,
            const AZStd::string_view value)
        {
            if (value.empty())
            {
                result = {};
                return true;
            }

            const SymbolEntry* entry = SymbolTable::Instance().TryIntern(value);
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
            if (value.empty())
            {
                result = {};
                return true;
            }

            const SymbolEntry* entry = SymbolTable::Instance().Find(value);
            if (!entry)
            {
                return false;
            }
            result = SymbolAccess::FromEntry(entry);
            return true;
        }
    } // namespace Internal
} // namespace AZ
