/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once


#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ostream>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::literals::string_view_literals;

namespace AZ::ShaderCompiler
{
    //! In AZSL Qualified means fully qualified.
    //! Let's make strong types for those, to get build-time incompatibility safety.
    //! Instead of using strings for naked name storage, we use these structures to store symbols.
    //! This way, we have a sort of "tainted string" concept, that will prevent us to break contract invariants in many places.
    //! Picture a sort of BOOST_STRONG_TYPEDEF. But that keeps compatibility with a 'view' type
    struct QualifiedName : std::string
    {
        // Need to declare explicitly a default constructor, since the declaration of a copy constructor hereunder would otherwise delete it.
        QualifiedName() = default;

        // Provide implicit compatibility from string_view for sheer convenience
        QualifiedName(std::string_view sv) : std::string(sv) {}

        friend size_t hash_value(const QualifiedName& arg) { return std::hash<std::string>{}(arg); }
    };

    //! In AZSL, Unqualified is a relative symbol. (can have some scope resolution operator, but not starting from global)
    //! Please refer to the comments in QualifiedName struct. They apply the same.
    struct UnqualifiedName : std::string
    {
        UnqualifiedName() = default;

        UnqualifiedName(std::string_view sv) : std::string(sv) {}

        friend size_t hash_value(const UnqualifiedName& arg) { return std::hash<std::string>{}(arg); }
    };

    // These 2 asserts verify that the whole exercise is functioning
    static_assert( !std::is_convertible< QualifiedName, UnqualifiedName >::value );
    static_assert( !std::is_convertible< UnqualifiedName, QualifiedName >::value );
    // Hereunder is the problem underlined by the nonexplicit conversion operator, we created a path,
    // that the compiler can take from one to another during explicit construction.
    // Ideally I want these asserts to verify. But we need to create the painful unordered requirements.
    // This "weakness" is not a massive problem in practice, since wrong-typed argument passing is still protected.
    //static_assert( !std::is_constructible< QualifiedName, UnqualifiedName >::value );
    //static_assert( !std::is_constructible< UnqualifiedName, QualifiedName >::value );
}

template<>
struct std::hash<AZ::ShaderCompiler::QualifiedName>
{
    size_t operator()(const AZ::ShaderCompiler::QualifiedName& qn) const noexcept
    {
        return hash_value(qn);
    }
};

template<>
struct std::hash<AZ::ShaderCompiler::UnqualifiedName>
{
    size_t operator()(const AZ::ShaderCompiler::UnqualifiedName& qn) const noexcept
    {
        return hash_value(qn);
    }
};

namespace AZ::ShaderCompiler
{
    // Please refer to the comments around the struct QualifiedName, for explanation about its raison d'etre.
    struct QualifiedNameView : std::string_view
    {
        constexpr QualifiedNameView() = default;

        template <typename... Args>
        explicit constexpr QualifiedNameView(Args&&... args) : std::string_view(std::forward<Args>(args)...) {}

        QualifiedNameView(const QualifiedName& qn) : std::string_view(static_cast<const std::string&>(qn)) {}

        friend size_t hash_value(const QualifiedNameView& arg) { return std::hash<std::string_view>{}(arg); }
    };

    struct UnqualifiedNameView : std::string_view
    {
        constexpr UnqualifiedNameView() = default;

        template <typename... Args>
        explicit constexpr UnqualifiedNameView(Args&&... args) : std::string_view(std::forward<Args>(args)...) {}

        UnqualifiedNameView(const UnqualifiedName& qn) : std::string_view(static_cast<const std::string&>(qn)) {}

        friend size_t hash_value(const UnqualifiedNameView& arg) { return std::hash<std::string_view>{}(arg); }
    };

    static_assert(!std::is_convertible< QualifiedNameView, UnqualifiedNameView >::value);
    static_assert(!std::is_convertible< UnqualifiedNameView, QualifiedNameView >::value);
}

template<>
struct std::hash<AZ::ShaderCompiler::QualifiedNameView>
{
    size_t operator()(const AZ::ShaderCompiler::QualifiedNameView& qn) const noexcept
    {
        return hash_value(qn);
    }
};

template<>
struct std::hash<AZ::ShaderCompiler::UnqualifiedNameView>
{
    size_t operator()(const AZ::ShaderCompiler::UnqualifiedNameView& qn) const noexcept
    {
        return hash_value(qn);
    }
};

namespace AZ::ShaderCompiler
{
    inline bool IsRooted(std::string_view path)
    {
        return StartsWith(path, "/") || StartsWith(path, "?");
    }

    //! from "/func(/f2(?int), ?float)" return "/func"
    inline std::string RemoveMatchedParenthesis(std::string_view name)
    {
        std::string result;
        int level = 0;
        for (auto character : name)
        {
            if (character == '(')
            {
                ++level;
            }
            if (level == 0)
            {
                result += character;
            }
            if (level > 0 && character == ')')
            {
                --level;
            }
        }
        return result;
    }

    //! "thing()more" will be untouched.
    //! "thing()" will become "thing"
    inline std::string_view RemoveLastParenthesisGroup(std::string_view name)
    {
        int level = 0;
        size_t startOfLastGroup = name.size();
        // find matched parenthesis going backward
        for (auto iter = name.rbegin(); iter != name.rend(); ++iter)
        {
            bool close = *iter == '(';  // since we go backward ( is the end of a group
            bool open = *iter == ')';
            level = open ? level + 1 : (close ? level - 1 : level);
            if (level <= 0)
            {
                startOfLastGroup = name.size() - std::distance(name.rbegin(), iter) - (close ? 1 : 0); // eat the closing '('
                break;
            }
        }
        return Slice(name, 0, startOfLastGroup);
    }

    //! Mutate "stuff()" to "stuff_vd_"
    //"        "thing(?int)" to "thing_?int_"
    inline std::string FlattenParenthesisGroups(std::string_view name)
    {
        auto ret = std::regex_replace(std::string(name.data(), name.size()), std::regex("\\(\\)"), "_vd");
        ret = std::regex_replace(ret, std::regex("[\\(\\)]"), "_");
        return ret;
    }

    inline bool IsLeaf(std::string_view name)
    {
        return RemoveMatchedParenthesis(name).find("/") == std::string::npos;
    }

    //! String replace of AZIR separators (mangled scope separators)
    inline std::string ReplaceSeparators(std::string name, std::string_view replacement)
    {
        if (IsRooted(name))
        {   // remove leading slash/questionmark to lighten output
            name = Slice(name, 1, -1);
        }
        return std::regex_replace(name, std::regex("\\/"), replacement.data());
    }

    static inline const char* Underscore = "_";

    enum FlattenStrategy
    {
        PreserveArgumentsUnicity,  //!< x(a) will be x_a
        CollapseArguments          //!< x(a) will be x
    };
    //! can be used at emission to flatten symbols into the global scope.
    //! symbol names are stored with path separators. like such:
    //! name = "/MyStruct/m_myVar"
    //! returns: "MyStruct_m_myVar"
    inline std::string Flatten(std::string name, FlattenStrategy strategy)
    {
        name = std::regex_replace(name, std::regex("\\?"), "");
        return ReplaceSeparators(strategy == PreserveArgumentsUnicity ? FlattenParenthesisGroups(name) : RemoveMatchedParenthesis(name), Underscore);
    }

    //! "?int" to "int" (partial unmangling)
    inline std::string_view RemoveFloatingMark(std::string_view name)
    {
        return StartsWith(name, "?") ? Slice(name, 1, -1) : name;
    }

    //! Change from AZIR form to AZSLang form
    //! eg "/SRG/mem" to "::SRG::mem"
    inline std::string UnMangle(std::string name)
    {
        std::string stdName = std::regex_replace(name, std::regex("/"), "::");
        stdName = std::regex_replace(stdName, std::regex("\\?"), "");
        name = RemoveMatchedParenthesis(stdName);
        return name;
    }

    //! Attempt to re-mangle a text-form idExpression but no guarantee about validity as an IdentifierUID. Notably predefined won't get their '?'
    //! If you have a proper AST context, use ExtractNameFromIdExpression instead.
    //! eg "SRG::mem" to "SRG/mem"
    inline std::string ReMangle(std::string name)
    {
        return std::regex_replace(name, std::regex("::"), "/");
    }

    //! in: `anyname` in AzIR format
    //! split a qualified name and return the last name element
    //! e.g. "/Stem/Nested/Leaf" returns "Leaf"
    //! Important note: This does not provide a valid unqualified name, lookup-able from a given scope.
    //!                 It is merely a string based chopping, that can be used for leaf name comparisons when useful.
    //! If you want the correct context-valid unqualifying feature, use the SymbolAggregator's FindLeastQualifiedName
    inline UnqualifiedNameView ExtractLeaf(std::string_view anyname)
    {
        auto lastSlash = anyname.rfind("/");
        while (WithinMatchedParentheses(anyname, lastSlash))
        {
            lastSlash = anyname.rfind("/", lastSlash-1);
        }
        if (lastSlash == std::string::npos)
        {   // if there is no slash at all, or is not rooted
            return UnqualifiedNameView{RemoveFloatingMark(anyname)};
        }
        return UnqualifiedNameView{RemoveFloatingMark(Slice(anyname, lastSlash + 1, -1))};
    }

    //! true  if anywhere in the input, a parenthesis appear
    //!       even in the middle, e.g:  "/A/f(/?int)/b"
    inline bool ArgumentDecorationExists(std::string_view name)
    {
        return name.find("(") != std::string::npos;
    }

    //! true  if the leaf of the input has this sort of form "fun(/T,?int)"
    //! false if e.g "/X"; e.g "/F(/T)/a"
    inline bool IsLeafDecoratedByArguments(std::string_view name)
    {
        return !name.empty() && (name.back() == ')' || ArgumentDecorationExists({ExtractLeaf(name)}));
    }

    inline size_t CountParameters(std::string_view mangledFunctionName)
    {
        auto leaf = ExtractLeaf(mangledFunctionName);
        return EndsWith(leaf, "()") || !EndsWith(leaf, ")")  // no-arg function or not-a-function
            ? 0
            : 1 + std::count(leaf.begin(), leaf.end(), ',');  // this isn't robust if types may hold comma (and it's the case for generics and function-types)
    }

    enum class JoinPolicy
    {
        EmptyMeansRoot,  // empty stem is joined as a root
        EmptyMeansEmpty  // empty stem is nil and doesn't participate in join
    };
    //! return "A/B" if passed stem="A/" and leaf="B"
    //!        "A/B" if passed stem="A"  and leaf="B"
    //!        "/A"  if passed stem=""   and leaf="A" (if policy is EmptyMeansRoot)
    //!        "A"   if passed stem=""   and leaf="A" (if policy is EmptyMeansEmpty)
    inline std::string JoinPath(std::string_view stem, std::string_view leaf, JoinPolicy policy = JoinPolicy::EmptyMeansRoot)
    {
        if ((stem.empty() || leaf.empty() ) && policy == JoinPolicy::EmptyMeansEmpty)
        {
            return std::string{stem.empty() ? leaf : stem};
        }

        assert(!IsRooted(leaf)); // violation of contract that leaf must not be rooted.
        if (EndsWith(stem, "/"))
        {
            return std::string{stem}.append(leaf);
        }
        else
        {
            return std::string{stem}.append("/").append(leaf);
        }
    }

    //! will return "/"     from "/dir"
    //! or          "/dir/" from "/dir/leaf"
    //! or          "/dir/" from "/dir/leaf/"
    //! or          ""      from "dir"
    //! or          "/X/"   from "/X/Get(/?int)"
    //! Note:       "/"     from "/"  (bump against the root)
    //! this function is fundamentally the dual of ExtractLeaf in the sense that it gets you the other side.
    inline std::string_view LevelUp(std::string_view path)
    {
        if (path == "/")
        {   // special case
            return "/";
        }

        if (EndsWith(path, "/"))
        {   // remove this nuisance early, to canonicalize input.
            // -1 is the end, -2 is 1 earlier than end. (think python slices)
            path = Slice(path, 0, -2);
        }
        size_t lastSlash = path.rfind("/");
        while (WithinMatchedParentheses(path, lastSlash))
        {
            lastSlash = path.rfind("/", lastSlash - 1);
        }
        if (lastSlash == std::string::npos)
        {   // no slash at all
            return "";
        }
        return Slice(path, 0, lastSlash + 1);
    }

    //! Has the same semantics than LevelUp but cleans up the trailing slash when there is a name left other than root.
    //! example: "/dir" from "/dir/leaf"
    inline std::string_view GetParentName(std::string_view path)
    {
        auto oneUp = LevelUp(path);
        if (oneUp != "/" && EndsWith(oneUp, "/"))
        {
            return Slice(oneUp, 0, oneUp.length() - 1);
        }
        return oneUp;
    }

    //! Overload for when you work with QualifiedNameView type
    inline QualifiedNameView GetParentName(QualifiedNameView path)
    {
        return QualifiedNameView{GetParentName(std::string_view{path})};
    }

    //! Overload for when you work with QualifiedName type
    inline QualifiedNameView GetParentName(const QualifiedName& path)
    {
        return QualifiedNameView{GetParentName(QualifiedNameView{path})};
    }

    struct PathPart
    {
        std::string_view m_slice;
        size_t      m_sliceBegin;
        size_t      m_sliceLen;
    };

    inline bool IsGlobal(QualifiedNameView sym)
    {
        return GetParentName(sym) == "/";
    }

    //! When there is no need to create the split path in memory
    //! FunctionObject will be fed a PathPart as parameter, and called for each part.
    //! The behavior is the same as SplitPath function, please refer to it for behavior document.
    template <typename FunctionObject>
    void ForEachPathPart(std::string_view path, FunctionObject action)
    {
        size_t start = 0;
        size_t pathLen = path.length();
        // character by character. cp=character position
        for (size_t cp = 0; cp < pathLen; ++cp)
        {
            bool isLast = cp + 1 == path.length();
            bool isSlash = path[cp] == '/' && !WithinMatchedParentheses(path, cp);
            if (isSlash || isLast)
            {
                size_t count = cp - start + (isLast && !isSlash ? 1 : 0);
                action(PathPart{path.substr(start, count), start, count});
                start = cp + 1;
                if (isSlash && isLast) // we need to call again for the end part, to signal it exists but is empty
                {
                    action(PathPart{path.substr(start, 0), start, 0});
                }
            }
        }
    }

    //! returns true if supposedChild is contained in supposedParent
    //! example: IsParent("/ns/bag", "/ns/bag/member") == true
    inline bool IsParent(std::string_view supposedParent, std::string_view supposedChild)
    {
        return supposedChild.size() > supposedParent.size() && supposedParent == supposedChild.substr(0, supposedParent.size());
    }

    //! examples ["", "A", "Leaf"] from "/A/Leaf"
    //!          ["A", "Leaf"] from "A/Leaf"
    //!          ["A", "Leaf", ""] from "A/Leaf/"
    inline std::vector<std::string_view> SplitPath(std::string_view path)
    {
        const size_t numSlashes = std::count(path.begin(), path.end(), '/'); // in the case of "/X(/a)" numSlashes overshoots, so we only use it for reservation
        std::vector<std::string_view> split;
        split.reserve(numSlashes);
        ForEachPathPart(path, [&split](const PathPart& ppart)
                        {
                            split.push_back(ppart.m_slice);
                        });
        return split;
    }

    //! counts the number of slashes.
    //! so "" is depth 0, "/sym" is depth 1, "/ns/x" is depth 2
    //! however "/" and "/ns/" are invalid input. because they are ambiguous. fix your input first.
    inline size_t GetSymbolDepth(std::string_view mangledName)
    {
        assert(!EndsWith(mangledName, "/"));  // principle of least astonishment -> make an error of this case
        return std::count(mangledName.begin(), mangledName.end(), '/');
    }

    //! Simple function joining scope and leaf. example: returns "/gfx/device" if you pass "/gfx" and "device"
    //! the only advantage over directly using JoinPath, is input validation (asserts), explicit verbosity, and helped casting.
    //! This is not the function you need if you are doing a lookup for a symbol, from a site that references something already declared.
    //! In that case you need the SymbolTable::LookupSymbol function.
    //! This function is for constructing new names from declaration sites.
    inline QualifiedName MakeFullyQualified(QualifiedNameView scope, UnqualifiedNameView name)
    {
        assert(IsRooted(scope));
        assert(IsLeaf(name));
        return QualifiedName{JoinPath(scope, name)};
    }

    //! produce a string of the form "(/t1,/t2,/t3)"
    //! where `begin` and `end` represents a range over resolved types fully qualified names (FQN)
    template< typename IteratorType >
    inline std::string CreateDecorationOfFunction(IteratorType begin, IteratorType end)
    {
        static_assert(std::is_same_v<typename std::iterator_traits<IteratorType>::value_type, QualifiedNameView> || std::is_same_v<typename std::iterator_traits<IteratorType>::value_type, QualifiedName>);
        return Decorate("(", Join(begin, end, ","), ")");
    }

    //! The key to any symbol
    struct IdentifierUID
    {
        UnqualifiedName GetNameLeaf() const
        {
            return ExtractLeaf(m_name);
        }

        QualifiedNameView GetName() const
        {
            return m_name;
        }

        bool IsEmpty() const
        {
            return m_name.empty();
        }

        void Clear()
        {
            m_name.clear();
        }

        //! Returns true if @otherUid is parent symbol of @this.
        //! e.g: "/MySrg" is parent of "/MySrg/m_color"
        bool IsParent(const IdentifierUID& otherUid) const
        {
            if (m_name.find(otherUid.m_name) != 0)
            {
                return false;
            }
            const size_t separatorPosition = otherUid.m_name.size();
            return (m_name.find("/", separatorPosition, 1) == separatorPosition);
        }

        // for hash table keying:
        friend bool operator == (const IdentifierUID& lhs, const IdentifierUID& rhs)
        {
            return lhs.m_name == rhs.m_name;
        }

        friend bool operator != (const IdentifierUID& lhs, const IdentifierUID& rhs)
        {
            return !operator==(lhs, rhs);
        }

        friend size_t hash_value(const IdentifierUID& arg)
        {
            return hash_value(arg.m_name); /*ADL*/
        }

        // for ordered container insertion:
        friend bool operator < (const IdentifierUID& lhs, const IdentifierUID& rhs)
        {
            return lhs.m_name < rhs.m_name;
        }

        friend std::ostream& operator << (std::ostream& stream, const IdentifierUID& id)
        {
            stream << id.m_name.c_str();
            return stream;
        }

        QualifiedName m_name;  // mangled symbol's absolute unique name. example: "/sdk/gfx/Device" or "?float"
    };
}

template<>
struct std::hash<AZ::ShaderCompiler::IdentifierUID>
{
    size_t operator()(const AZ::ShaderCompiler::IdentifierUID& arg) const noexcept
    {
        return hash_value(arg);
    }
};

template<>
struct std::equal_to<AZ::ShaderCompiler::IdentifierUID>
{
    bool operator()(const AZ::ShaderCompiler::IdentifierUID& lhs, const AZ::ShaderCompiler::IdentifierUID& rhs) const
    {
        return lhs == rhs;
    }
};

namespace AZ::ShaderCompiler
{
    // helper function to search over a collection, for a match on the leaf of a symbol.
    template <typename ContainerOfIdUID>
    bool ContainsSameLeafName(const ContainerOfIdUID& ctr, UnqualifiedNameView uqName)
    {
        assert(!IsRooted(uqName));
        return Contains(ctr.begin(), ctr.end(), [&](decltype(*ctr.begin()) elem) {return elem.GetNameLeaf() == uqName; });
    }
}

#ifndef NDEBUG
namespace AZ::Tests
{
    inline void Func(AZ::ShaderCompiler::UnqualifiedName un) {}

    inline void DoAsserts5()
    {
        using namespace AZ::ShaderCompiler;

        assert(ExtractLeaf("/") == "");
        assert(ExtractLeaf("/A/B") == "B");
        assert(ExtractLeaf("/A/B/") == "");
        assert(ExtractLeaf("A") == "A");
        //assert(ExtractLeaf("A/B") == "A/B");
        assert(ExtractLeaf("A/B") == "B");
        assert(ExtractLeaf("?int") == "int");
        assert(ExtractLeaf("/X/func(/A)") == "func(/A)");
        assert(ExtractLeaf("func(/A)") == "func(/A)");
        assert(ExtractLeaf("func(/A)/param") == "param");
        assert(ExtractLeaf("/func(/A)/param") == "param");

        assert(LevelUp("/dir") == "/");
        assert(LevelUp("/dir/leaf") == "/dir/");
        assert(LevelUp("/dir/leaf/") == "/dir/");
        assert(LevelUp("dir") == "");
        assert(LevelUp("/") == "/");
        assert(LevelUp("/X/Get(/?int, /func())") == "/X/");
        assert(LevelUp("/X/Get(/?int, /func())/lambda()") == "/X/Get(/?int, /func())/");

        assert(GetParentName("/dir/leaf") == "/dir");
        assert(GetParentName("/dir/leaf/") == "/dir");
        assert(GetParentName("/global") == "/");
        assert(GetParentName("/") == "/");
        assert(GetParentName("leaf") == "");
        assert(GetParentName("?leaf") == "");

        using namespace std::literals::string_view_literals;

#if defined(_MSC_VER) && _MSC_VER >= 1910
        // Visual Studio passes the string views here, other compilers will not
        assert(SplitPath("/A/Leaf") == (std::vector{ ""sv, "A"sv, "Leaf"sv }));
        assert(SplitPath("A/Leaf") == (std::vector{ "A"sv, "Leaf"sv }));
        assert(SplitPath("Leaf") == (std::vector{ "Leaf"sv }));
        assert(SplitPath("A/Leaf/") == (std::vector{ "A"sv, "Leaf"sv, ""sv }));
        assert(SplitPath("/") == (std::vector{ ""sv, ""sv }));
        assert(SplitPath("/X(/a)/f") == (std::vector{ ""sv, "X(/a)"sv, "f"sv }));
        assert(SplitPath("X(/f(/t))/g") == (std::vector{ "X(/f(/t))"sv, "g"sv }));
#endif

        assert(JoinPath("A/", "B") == "A/B");
        assert(JoinPath("A", "B") == "A/B");
        assert(JoinPath("", "A") == "/A");
        assert(JoinPath("", "A", JoinPolicy::EmptyMeansEmpty) == "A");

        assert(UnMangle("/func(?int, f2())") == "::func");
        assert(UnMangle("/class/member") == "::class::member");
        assert(UnMangle("?matrix4x4") == "matrix4x4");

        assert(IsLeafDecoratedByArguments("/func()"));
        assert(IsLeafDecoratedByArguments("/func(?int)"));
        assert(!IsLeafDecoratedByArguments("/func"));
        assert(!IsLeafDecoratedByArguments("func"));
        assert(!IsLeafDecoratedByArguments("/A/func(/T)/b"));
        assert(!IsLeafDecoratedByArguments("A/func(/T)/b"));
        assert(IsLeafDecoratedByArguments("A/func(/T)/g(?int)"));

        assert(RemoveLastParenthesisGroup("func()more") == "func()more");
        assert(RemoveLastParenthesisGroup("func()") == "func");
        assert(RemoveLastParenthesisGroup("func()()") == "func()");
        assert(RemoveLastParenthesisGroup("func(()())") == "func");
        assert(RemoveLastParenthesisGroup("func(blah)") == "func");
        assert(RemoveLastParenthesisGroup("func(blah)/more") == "func(blah)/more");
        assert(RemoveLastParenthesisGroup("func(blah())/more") == "func(blah())/more");
        assert(RemoveLastParenthesisGroup("func(blah())") == "func");

        assert(CountParameters("/f(?int)") == 1);
        assert(CountParameters("/f()") == 0);
        assert(CountParameters("/f(?int, ?float)") == 2);
        assert(CountParameters("/f(/A/B/C, /D/E/F, ?half)") == 3);
        assert(CountParameters("?vector<half,3>") == 0);

        QualifiedName qn;
        // moderately shameful that this compiles:
        UnqualifiedName uqn{ qn };  // mitigation c.f. comment around the is_constructible earlier in this file
        // desired:   (uncomment to verify)
        // UnqualifiedName uqn2 = qn;  Error C2440 'initializing': cannot convert from 'AZ::ShaderCompiler::QualifiedName' to 'AZ::ShaderCompiler::UnqualifiedName'

        // desired:   (uncomment to verify)
        // Func(qn);  Error C2664: cannot convert argument 1 from 'AZ::ShaderCompiler::QualifiedName' to 'AZ::ShaderCompiler::UnqualifiedName'
    }
}
#endif
