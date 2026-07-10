/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "SymbolTable.h"
#include "DependencySolver.tpl"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace AZ::ShaderCompiler
{
    // mangled symbol path of the rootconstant constant buffer
    static constexpr QualifiedNameView RootConstantsViewName{"/rootconstantsCB"};

    //! policy enum for AddIdentifier method parameter
    enum class AddIdentifierChecks
    {
        None, ReservedNames,
    };

    /// Higher level table that holds 2 symbol databases: an intrinsic "fixed", and a user-source driven "elastic"
    class SymbolAggregator
    {
    public:
        SymbolAggregator();

        bool HasIdentifier(QualifiedNameView symbol) const;

        IdAndKind* GetIdAndKindInfo(QualifiedNameView symbol);

        const IdAndKind* GetIdAndKindInfo(QualifiedNameView symbol) const;

        /// Register a fresh entry in the symbol map. No KindInfo filled up, the client must do it.
        /// Will return a reference to the newly inserted data.
        /// Will throw in case of ODR violation.
        IdAndKind& AddIdentifier(QualifiedNameView symbol, Kind kind, std::optional<size_t> lineNumber = std::nullopt, AddIdentifierChecks = AddIdentifierChecks::ReservedNames);

        bool DeleteIdentifier(IdentifierUID name);

        /// Links a name to the closest reachable declaration.
        /// Using current scope path. example: returns "/gfx/device" if you pass e.g. "/gfx/detail/" and "device"
        /// This is an unfortunately complex feature, refer to https://en.cppreference.com/w/cpp/language/unqualified_lookup
        /// If you have an already fully-qualified name, this function is not for you (just use GetIdentifier).
        IdAndKind* LookupSymbol(QualifiedNameView scope, UnqualifiedNameView name);

        /// Lookup in context, the most iteratively less-qualified version, that still points to the same symbol
        UnqualifiedName FindLeastQualifiedName(QualifiedNameView scope, IdentifierUID uid);

        /// The order collection form the elastic table. (we'll just assume the fixed-table order is totally un-interesting)
        decltype(SymbolTable::m_order)& GetOrderedSymbols();

        /// const version
        const decltype(SymbolTable::m_order)& GetOrderedSymbols() const;

        /// Immediately access the contained SubInfo from a symbol lookup
        /// Convenient shortcut when you know what you are working with.
        /// Will return nullptr if you request a wrong subtype T, or if the symbol is absent.
        template <typename T>
        const T*
        GetAsSub(IdentifierUID symbol) const
        {
            auto idKindPtr = GetIdAndKindInfo(symbol.GetName());
            if (!idKindPtr)
            {
                return nullptr;
            }
            return idKindPtr->second.GetSubAs<T>();
        }

        /// mutable version (Scott Meyers's way of factorizing)
        template <typename T>
        T*
        GetAsSub(IdentifierUID symbol)
        {
            return const_cast<T*>(const_cast<const SymbolAggregator*>(this)->GetAsSub<T>(symbol));
        }

        template <typename Sub>
        using Id_Sub_Kind = std::tuple<IdentifierUID, Sub*, KindInfo*>;

        /// list of pre-gotten subinfo of type Sub, filtered from GetOrderedSymbols()
        template <typename Sub>
        std::vector<Sub*> GetOrderedSubInfosOfSubType()
        {
            std::vector<Sub*> filteredList;
            for (const auto& srg : GetOrderedSymbols())
            {
                auto* sub = GetAsSub<Sub>(srg);
                if (sub)
                {
                    filteredList.push_back(sub);
                }
            }
            return filteredList;
        }

        /// Ordered symbol list, filtered by subinfo of type Sub
        /// each element is a pair (id, Sub*)
        template <typename Sub>
        std::vector<std::pair<IdentifierUID, Sub*>> GetOrderedSymbolsOfSubType_2()
        {
            std::vector<std::pair<IdentifierUID, Sub*>> filteredList;
            for (const auto& srg : GetOrderedSymbols())
            {
                auto& [uid, info] = *m_elastic.GetIdAndKindInfo(srg.m_name);
                auto* sub = info.GetSubAs<Sub>();
                if (sub)
                {
                    filteredList.emplace_back(uid, sub);
                }
            }
            return filteredList;
        }

        /// Ordered symbol list, filtered by subinfo of type Sub
        /// each element is a tuple (id, Sub*, KindInfo*)
        template <typename Sub>
        std::vector<Id_Sub_Kind<Sub>> GetOrderedSymbolsOfSubType_3()
        {
            std::vector<Id_Sub_Kind<Sub>> filteredList;
            for (const auto& srg : GetOrderedSymbols())
            {
                auto& [uid, info] = *m_elastic.GetIdAndKindInfo(srg.m_name);
                auto* sub = info.GetSubAs<Sub>();
                if (sub)
                {
                    filteredList.emplace_back(uid, sub, &info);
                }
            }
            return filteredList;
        }

        //! Adds a new attribute either to the global or attached attribute list
        void PushPendingAttribute(const AttributeInfo& attrInfo, AttributeScope scope);

        //! Gets the list of attributes attached to the identifier, if any. nullptr if std::nullopt
        const std::vector<AttributeInfo>* GetAttributeList(const IdentifierUID& uid) const;

        //! Gets the attribute with the matching name attached to the identifier, if any
        std::optional<AttributeInfo> GetAttribute(const IdentifierUID& uid, const std::string& attributeName) const;

        //! Gets the list of global (unattached) attributes
        const std::vector<AttributeInfo>& GetGlobalAttributeList() const;

        //! Change (in-place) the elastic.m_order vector to shuffle the symbol IDs so that they
        //! get ordered to respect dependencies on each other.
        void ReorderBySymbolDependency();

        const SymbolTable m_fixed; // populated once with startup symbols (global namespace and predefined types)
        SymbolTable m_elastic; // from user's source

    protected:
        /// Attaches the accumulated non-global attributes to the uid and flushes the list
        void AttachAccumulatedAttributes(const IdentifierUID& uid);

        //! List of encountered, but pending, attributes
        std::array<std::vector<AttributeInfo>, AttributeScope::EndEnumeratorSentinel_> m_orphanAttributesList;
        //! List of attached attributes (during parsing, encountered attributes accumulates, then flow to their definitive place)
        std::map<IdentifierUID, std::vector<AttributeInfo>> m_idToAttributeMap;
    };
}
