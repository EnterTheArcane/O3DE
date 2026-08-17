/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SymbolAggregator.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace AZ::ShaderCompiler
{
    template <auto N>
    void AddTypeBag(const AZ::ShaderCompiler::Predefined::Bag<N>& bag, SymbolTable& st)
    {
        for (const std::string_view symbol : bag.m_bag)
        {
            QualifiedName azirName{"?"}; // AZIR prefix for predefined types
            azirName += symbol;
            auto& [uid, kindInfo] = st.AddIdentifier(azirName, Kind::Type); // the kind is Type because all predefined are stored as such.
            auto& typeInfo = kindInfo.GetSubAfterInitAs<Kind::Type>();
            auto typeClass = TypeClass::FromStr(bag.m_name);
            ArithmeticTraits arithmetic{};
            if (IsNonGenericArithmetic(typeClass))
            {
                arithmetic = CreateArithmeticTraits(azirName);
            }
            typeInfo = TypeRefInfo{uid, arithmetic, typeClass};
        }
    }

    static SymbolTable InitFixedTable()
    {
        SymbolTable st;
        // Register the global namespace as a resident symbol in the database:
        // This allows canonical treatment when working with `current scope`.
        st.AddIdentifier(QualifiedNameView("/"), Kind::Namespace);

        // temporarily disable the verbosity since it completely bloats the output
        GetVerboseStream() << " registering of all predefined types in fixed symbol table (kept silent)...";
        const bool oldVerbosity = GetVerboseStream().m_on;
        GetVerboseStream().m_on = false;

        // another helpful canonicalization is for types.
        // let's register all predefined so that TypeRef can be simplified to IdentifierUID
        // the (pack op ...) is a C++17 unary-right-fold-expression using comma as op
        std::apply(
            [&st](auto&&... args)
            {
                (AddTypeBag(args, st), ...);
            },
            AZ::ShaderCompiler::Predefined::All);

        GetVerboseStream().m_on = oldVerbosity;
        GetVerboseStream() << " done\n";

        return st;
    }

    SymbolAggregator::SymbolAggregator()
        : m_fixed{InitFixedTable()}
    {
    }

    bool SymbolAggregator::HasIdentifier(const QualifiedNameView symbol) const
    {
        return m_elastic.HasIdentifier(symbol) || m_fixed.HasIdentifier(symbol);
    }

    IdAndKind* SymbolAggregator::GetIdAndKindInfo(const QualifiedNameView symbol)
    {
        IdAndKind* idAndKind = m_elastic.GetIdAndKindInfo(symbol);
        if (idAndKind)
        {
            return idAndKind;
        }

        // I am going to go ahead and tolerate returning of non const data to the fixed table.
        // Not ideal, but will cause much lost time and pondering, if we don't consider it through non-const Gets.
        return const_cast<std::remove_const_t<decltype(m_fixed)>&>(m_fixed).GetIdAndKindInfo(symbol);
    }

    const IdAndKind* SymbolAggregator::GetIdAndKindInfo(const QualifiedNameView symbol) const
    {
        const IdAndKind* idAndKind = m_elastic.GetIdAndKindInfo(symbol);
        if (idAndKind)
        {
            return idAndKind;
        }

        return m_fixed.GetIdAndKindInfo(symbol);
    }

    IdAndKind& SymbolAggregator::AddIdentifier(
        QualifiedNameView symbol,
        const Kind kind,
        const std::optional<size_t> lineNumber /*=std::nullopt*/,
        const AddIdentifierChecks checkPolicy /*= AddIdentifierChecks::ReservedNames*/)
    {
        // check against reserved names
        static constexpr std::array<std::string_view, 2> ReservedNames = {
            "/Root_Constants",
            RootConstantsViewName,
        };

        if (checkPolicy == AddIdentifierChecks::ReservedNames && (
            IsIn(symbol, ReservedNames) || IsIn(ExtractLeaf(symbol), ReservedNames)))
        {
            throw AzslcException(
                ADVANCED_RESERVED_NAME_USED,
                "Symbol",
                lineNumber,
                std::nullopt,
                ConcatString(symbol, " is a reserved name"));
        }
        auto& symAndKind = m_elastic.AddIdentifier(symbol, kind, lineNumber);
        AttachAccumulatedAttributes(symAndKind.first);
        return symAndKind;
    }

    bool SymbolAggregator::DeleteIdentifier(IdentifierUID name)
    {
        m_idToAttributeMap.erase(name);
        return m_elastic.DeleteIdentifier(name);
    }

    IdAndKind* SymbolAggregator::LookupSymbol(const QualifiedNameView scope, const UnqualifiedNameView name)
    {
        using namespace std::string_literals;
        if (IsRooted(name))
        {
            // It is possible that fully-qualified symbols find their way inside unqualified-tainted names.
            // For the reason mentioned in the comment decorating ExtractNameFromIdExpression() function. please refer.
            // Even fully qualified inputs must go through lookup resolution (to solve inherited access)
            return LookupSymbol(QualifiedNameView{"/"}, UnqualifiedNameView{Slice(name, 1, -1)});
        }
        // try as floating symbol in priority (predefined are found at any scope)
        IdAndKind* got = GetIdAndKindInfo(QualifiedName{"?"s + name.data()});
        if (got)
        {
            return got;
        }
        assert(!IsLeafDecoratedByArguments(name)); // refer to ../Documentation/function-overloading/research.txt
        // from now on scope matters
        assert(IsRooted(scope));
        // Iterative lookup of the closest reachable symbol
        // by going further toward global.
        // e.g try to locate: /Typ/Sub/Sym/name; if not found: /Typ/Sub/name; if not found: /Typ/name; ...
        // this is the classic symbol shadowing scheme: closer symbols (less qualification distance) shadow more "global" symbols.
        //   say:
        //      int a;
        //      class C {
        //          int a;
        //          void f() { a; }  // refers to /C/a  but would refer to /a if `C` didn't have a member a. /C/a shadows /a
        //      };
        std::string_view path = scope;
        bool exit = false;
        do
        {
            auto attempt = QualifiedName{JoinPath(path, name)};
            got = GetIdAndKindInfo(attempt);
            exit = path == "/" || path.empty();
            if (!got)
            {
                if (const auto* scopeAsClass = GetAsSub<ClassInfo>(IdentifierUID{GetParentName(attempt)})) // get enclosing class
                {
                    // classes need deep lookup, because may have bases. classes don't save in the symbol-table all the fields they render accessible.
                    // Because multiple bases (upways and sideways) can shadow each-other's fields; caching inheritated fields would require complicated mangling.
                    for (auto it = scopeAsClass->GetBases().begin(); it != scopeAsClass->GetBases().end() && !got; ++it)
                    {
                        got = LookupSymbol(it->GetName(), ExtractLeaf(attempt));
                    }
                }
            }
            path = LevelUp(path);
        }
        while (!got && !exit);
        return got;
    }

    UnqualifiedName SymbolAggregator::FindLeastQualifiedName(const QualifiedNameView scope, IdentifierUID uid)
    {
        // start from the completely unqualified version:
        IdAndKind* got;
        UnqualifiedName name;
        const QualifiedName target = RemoveLastParenthesisGroup(uid.GetName());
        UnqualifiedName nextname = ExtractLeaf(target);
        const std::vector<std::string_view> split = SplitPath(uid.m_name);
        bool found = false;
        const int numsplits = static_cast<int>(split.size());
        for (int i = numsplits - 2; // start from leaf-1 (e.g. from "/A/B/Leaf", name is already Leaf, we need to prepend "B", then "A")
             i >= -1 && !found; // loop until found, or for "all elements in the split, +1" (for the opportunity to try out the last nextname)
             --i) // progressively add qualifiers
        {
            name = nextname;
            got = LookupSymbol(scope, name);
            found = got && got->first.GetName() == target;
            // prepare next iteration
            if (i >= 0)
            {
                nextname = UnqualifiedName{JoinPath(split[i], name)};
            }
        }
        return RemoveFloatingMark(name);
    }

    decltype(SymbolTable::m_order)& SymbolAggregator::GetOrderedSymbols()
    {
        return m_elastic.m_order;
    }

    const decltype(SymbolTable::m_order)& SymbolAggregator::GetOrderedSymbols() const
    {
        return m_elastic.m_order;
    }

    void SymbolAggregator::PushPendingAttribute(const AttributeInfo& attrInfo, const AttributeScope scope)
    {
        m_orphanAttributesList[scope].push_back(attrInfo);
    }

    const std::vector<AttributeInfo>* SymbolAggregator::GetAttributeList(const IdentifierUID& uid) const
    {
        const auto attrList = m_idToAttributeMap.find(uid);
        if (attrList == m_idToAttributeMap.end())
        {
            return nullptr;
        }

        return &attrList->second;
    }

    static std::optional<AttributeInfo> FindAttributeByNameInList(
        const std::vector<AttributeInfo>& attrList,
        const std::string& attributeName)
    {
        const auto iter = std::find_if(
            attrList.begin(),
            attrList.end(),
            [=](const auto& attrInfo)
            {
                return attrInfo.m_attribute == attributeName;
            });

        if (iter == attrList.end())
        {
            return std::nullopt;
        }

        return std::optional{*iter};
    }

    std::optional<AttributeInfo> SymbolAggregator::GetAttribute(
        const IdentifierUID& uid,
        const std::string& attributeName) const
    {
        std::optional<AttributeInfo> result;
        if (const auto attrList = GetAttributeList(uid))
        {
            result = FindAttributeByNameInList(*attrList, attributeName);
        }
        return result;
    }

    const std::vector<AttributeInfo>& SymbolAggregator::GetGlobalAttributeList() const
    {
        return m_orphanAttributesList[AttributeScope::Global];
    }

    void SymbolAggregator::AttachAccumulatedAttributes(const IdentifierUID& uid)
    {
        if (m_orphanAttributesList[AttributeScope::Attached].empty())
        {
            return; // Nothing to attach
        }

        m_idToAttributeMap.try_emplace(uid, std::move(m_orphanAttributesList[AttributeScope::Attached]));
        m_orphanAttributesList[AttributeScope::Attached].clear();
    }

    void SymbolAggregator::ReorderBySymbolDependency()
    {
        auto disambiguatorChar = '#';
        // query for symbol kind; because that function is specific to this algorithm, it's ok locally only.
        auto isFunctionOrVariableOrType = [this, disambiguatorChar](std::string_view name)
        {
            name = Slice(name, 0, name.find_first_of(disambiguatorChar));
            const KindInfo& ki = GetIdAndKindInfo(QualifiedNameView{name})->second;
            return std::make_tuple(
                ki.IsKindOneOf(Kind::Function, Kind::OverloadSet),
                ki.IsKindOneOf(Kind::Variable),
                IsKindOneOfTypeRelated(ki.GetKind()));
        };
        // instanciate an empty solver and fill it up with the elastic symbols from the aggregator to reorder them
        DependencySolver<IdentifierUID, 50_maxdep_pernode> solver;
        // state variable that remembers the last iterated symbol that was of a specific nesting depth
        std::stack<IdentifierUID> lastSymbolAtCurrentLevel;
        size_t curDepth = 0;
        for (const IdentifierUID& id : m_elastic.m_order)
        {
            // We need to uniquify names to preserve repetitions (it happens for function declarations versus definition).
            // The solver is full of maps and sets so it will have the bad habit of deduplicating your nodes if not.
            auto disambiguated = id;
            while (solver.Has(disambiguated))
            {
                disambiguated.m_name += disambiguatorChar;
            }
            solver.AddNode(disambiguated);
            // if you visualize the the AST with the root at the top and nestings growing downward. Brother symbols are horizontal.
            //                 ●  '/'          ╔═════════════════╗               ●  '/'
            //                                 ║we need to create║
            //        ● 'g_fog'   ● 'class C'  ║  these links:   ║      ● 'g_fog' ← ● 'class C'
            //                                 ╚═════════════════╝                       ↑
            //                    ● 'struct C/S'                                    ● 'struct C/S'
            const size_t symbolDepth = GetSymbolDepth(disambiguated.GetName());
            if (symbolDepth > curDepth)
            {
                lastSymbolAtCurrentLevel.push(disambiguated);
            }
            else
            {
                while (symbolDepth < lastSymbolAtCurrentLevel.size())
                {
                    lastSymbolAtCurrentLevel.pop();
                }
                // establish a horizontal link between symbols of the same level to preserve the apparition order
                const bool sameParentAsLast = GetParentName(disambiguated.GetName()) == GetParentName(lastSymbolAtCurrentLevel.top().GetName());
                const bool parentIsT = std::get<2>(isFunctionOrVariableOrType(GetParentName(disambiguated.GetName())));
                const bool lastIsFunction = std::get<0>(isFunctionOrVariableOrType(lastSymbolAtCurrentLevel.top().GetName()));
                const bool curIsT = std::get<2>(isFunctionOrVariableOrType(disambiguated.GetName()));
                const bool isNestedType = curIsT && symbolDepth > 0;
                // verifying !lastIsFunction, permits to break dependency cycles.
                if (sameParentAsLast && !lastIsFunction && !(isNestedType && parentIsT)) // in "class C { int a; struct S{}; };"  `S` cannot depend on `a` otherwise `a` is pulled out of C
                {
                    solver.AddDependency(disambiguated, lastSymbolAtCurrentLevel.top()); // make link:  ● 'g_fog' ← ● 'class C'
                }
                lastSymbolAtCurrentLevel.top() = disambiguated;
            }
            curDepth = symbolDepth;
            if (!IsGlobal(disambiguated.GetName()))
            {
                // establish vertical links
                std::string path;
                IdentifierUID parent;
                ForEachPathPart(
                    disambiguated.GetName(),
                    [&solver, &path, &parent, &isFunctionOrVariableOrType](PathPart part)
                    {
                        path = JoinPath(path, part.m_slice);
                        auto [isFunc, isVar, _] = isFunctionOrVariableOrType(path);
                        const IdentifierUID current{QualifiedNameView{path}};
                        const bool parentIsEmptyOrRoot = parent.IsEmpty() || parent.GetName() == "/";
                        if (!parentIsEmptyOrRoot)
                        {
                            if (isFunc)
                            {
                                // functions depends on containing-scope's content
                                // e.g.
                                //    SRG A {
                                //       T var;
                                //       void Func() { var=x; }  // Func depends on var
                                //    }
                                // since the Emitter will pull Func out, we need it to appear after var.
                                // and var will be defined by the mutated form of A.
                                //   A problem this poses, is if any var depends on Func.
                                //   Which would be possible through initializers. example:
                                //      SRG A {
                                //         int getzero();
                                //         int var = getzero();
                                //      }
                                //   This situation is ill-formed
                                solver.AddDependency(current, parent);
                            }
                            else if (!isVar)
                            {
                                // types cannot depend on their containing symbol, it's the other way around.
                                //    class A {
                                //       class B{ A a; }; // error (A is incomplete)
                                //    };
                                // however, since symbol migrations in the Emitter may pull B out,
                                // we need B to appear before A. therefore A depends on B.
                                solver.AddDependency(parent, current);
                            }
                        }
                        if (!(isFunc || isVar) || parentIsEmptyOrRoot)
                        {
                            parent = current;
                        }
                    });
            }
        }
        assert(solver.m_order.size() == m_elastic.m_order.size());
        solver.Solve();
        // restore the original names
        for (auto& id : solver.m_order)
        {
            id.m_name = Slice(id.m_name, 0, id.m_name.find_first_of(disambiguatorChar));
        }
        m_elastic.m_order = std::move(solver.m_order);
    }
} // end of namespace AZ::ShaderCompiler
