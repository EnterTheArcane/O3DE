/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "MetaUtils.h"

#include "StreamableInterface.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#define AZ_STRINGIFY(x) #x
#define AZ_TOSTRING(x) AZ_STRINGIFY(x)
#define AZ_FILELINE __FILE__ ":" AZ_TOSTRING(__LINE__)

using namespace std::literals::string_view_literals;

namespace AZ
{
    // exception type of VisitFirstNonNull
    struct AllNull : std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    //! Type-heterogeneity-preserving multi pointer object single visitor.
    //! Returns whatever the passed functor would.
    //! Throws if all passed objects are null.
    template <typename Lambda, typename T>
    std::invoke_result_t<Lambda, T*> VisitFirstNonNull(Lambda functor, T* object) noexcept(false)
    {
        if (object)
        {
            return functor(object);
        }
        throw AllNull{"no non-null object passed"};
    }

    template <typename Lambda, typename T, typename... TOther>
    std::invoke_result_t<Lambda, T*> VisitFirstNonNull(Lambda functor, T* object, TOther*... rest) noexcept(false)
    {
        if (object)
        {
            return functor(object);
        }
        else
        {
            return VisitFirstNonNull(functor, rest...);
        }
    }

    //! Create substring views of views. Works like python slicing operator [n:m] with limited modulo semantics.
    //! what I ultimately desire is the range v.3 feature eg `letters[{2,end-2}]`
    //! http://ericniebler.com/2014/12/07/a-slice-of-python-in-c/
    inline constexpr std::string_view Slice(const std::string_view& in, int64_t st, int64_t end)
    {
        auto inSSize = (int64_t)in.size();
        if (inSSize == 0)
        {
            return in;
        }

        while (end < 0)
        {
            end = end + inSSize + 1;
        }

        while (st < 0)
        {
            st = st + inSSize + 1;
        }
        if (end > inSSize)
        {
            end = end % inSSize;
        }

        if (st > inSSize)
        {
            st = st % inSSize;
        }
        return in.substr(st, end - st);
    }

    //static_assert(Slice("0123", 1, 2) == "1");  // visual studio 2017 can't run this. gcc 7 works. and VCpre2018 works https://godbolt.org/z/_Ycv6G
    static_assert(Slice("0123", 1, 1) == "");
    static_assert(Slice("0123", 0, 1) == "0");
    static_assert(Slice("0123", 0, 4) == "0123");
    static_assert(Slice("0123", 0, -1) == "0123");
    static_assert(Slice("0123", 0, 0) == "");
    //static_assert(Slice("0123", -3, -2) == "2");  // VS is incapable of getting this right either. (proof that it should, as dynamic assert below at //*)
    //static_assert(Slice("0123", -2, -1) == "3");
    static_assert(Slice("0123", -1, -0) == "");

    //! ability to create size_t literals
    //! waiting for Working Group to get their stuff together https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/tGoPjUeHlKo
    inline constexpr std::size_t operator ""_sz(unsigned long long n)
    {
        return n;
    }

    template <class Container>
    Container Split(const std::string& str, std::string_view delims = " ")
    {
        Container cont;

        std::size_t current, previous = 0;
        current = str.find_first_of(delims);
        while (current != std::string::npos)
        {
            if (current > previous)
            {
                cont.push_back(str.substr(previous, current - previous));
            }
            previous = current + 1;
            current = str.find_first_of(delims, previous);
        }
        cont.push_back(str.substr(previous, current - previous));
        return cont;
    }

    inline std::string_view GetFileNameWithoutExtension(std::string_view fileName)
    {
        size_t lastIndex = fileName.find_last_of(".");
        if (lastIndex == -1)
        {
            lastIndex = fileName.size();
        }

        return fileName.substr(0, lastIndex);
    }

    //! debug-build asserted dyn_cast, otherwise, release-build static_cast (idea from boost library)
    template <typename To, typename From>
    To polymorphic_downcast(From instance)
    {
        static_assert(std::is_reference_v<To> || std::is_pointer_v<To>, "can't use non-ref/ptr types fot polymorphic casts");
        static_assert(std::is_reference_v<From> || std::is_pointer_v<From>, "can't use non-ref/ptr types fot polymorphic casts");
#if !defined(NDEBUG)
        assert(dynamic_cast<To>(instance));
#endif
        return static_cast<To>(instance);
    }

    //! surround a string with a prefix and a suffix
    inline std::string Decorate(std::string_view prefix, std::string_view body, std::string_view suffix)
    {
        std::stringstream ss;
        ss << prefix.data();
        ss << body.data();
        ss << suffix.data();
        return ss.str();
    }

    //! 2 arguments version in case both sides are the same
    inline std::string Decorate(std::string_view prefixAndSuffix, std::string_view body)
    {
        return Decorate(prefixAndSuffix, body, prefixAndSuffix);
    }

    //! reverse the effect of a symmetrical decoration
    inline std::string_view Undecorate(std::string_view decoration, std::string_view body)
    {
        auto indexStart = body.starts_with(decoration) ? decoration.length() : 0;
        auto indexEnd = body.ends_with(decoration) ? body.length() - decoration.length() : body.length();
        return Slice(body, indexStart, indexEnd);
    }

    //! Erase-Remove algorithm which removes all whitespaces from a string.
    inline std::string RemoveWhitespaces(std::string haystack)
    {
        std::erase_if(
            haystack,
            [](unsigned char c)
            {
                return std::isspace(c);
            });
        return haystack;
    }

    inline bool IsAllWhitespaces(std::string_view s)
    {
        return std::all_of(
            s.begin(),
            s.end(),
            [&](char c)
            {
                return std::isspace(c);
            });
    }

    //! tells whether a position in a string is surrounded by round braces
    //! e.g. true  for arguments {"a(b)", 2}
    //! e.g. true  for arguments {"a()", 1}  by convention
    //! e.g. false for arguments {"a()", 2}  by convention
    //! e.g. false for arguments {"a(b)", 0}
    //! e.g. false for arguments {"a(b)c", 4}
    //! e.g. false for arguments {"a(b)c(d)", 4}
    //! e.g. true  for arguments {"a((b)c(d))", 5}
    inline bool WithinMatchedParentheses(std::string_view haystack, size_t charPosition)
    {
        const auto hayLen = haystack.length();
        // we don't even need to be looking at the left side, we suppose balanced construction.
        // let's evaluate the level of nesting at the asked pos.
        int nesting = 0;
        for (size_t pos = 0; pos <= charPosition && pos < hayLen; ++pos)
        {
            nesting += haystack[pos] == '('
                           ? 1
                           : (haystack[pos] == ')' ? -1 : 0);
        }
        return nesting > 0;
    }

    //! replace all occurrences of substring `sub` with substring `to` within haystack.
    //!  e.g: Replace("aaa#aaa", "#", "_") gives-> "aaa_aaa"
    inline std::string Replace(std::string haystack, std::string_view sub, std::string_view to)
    {
        decltype(sub.length()) pos = 0;
        auto lFrom = sub.length();
        auto lTo = to.length();
        while (((pos = haystack.find(sub, pos)) != std::string::npos))
        {
            haystack.replace(pos, lFrom, to);
            pos += lTo;
        }
        return haystack;
    }

    //! this one is inspired by the docopt utilities. trims whitespace by default, but can be used to trim quotes.
    constexpr inline std::string_view Trim(std::string_view haystack, std::string_view toTrim = " \t\n")
    {
        const auto strEnd = haystack.find_last_not_of(toTrim);
        if (strEnd == std::string::npos)
        {
            return {};
        }
        haystack.remove_suffix(haystack.size() - strEnd - 1);

        const auto strBegin = haystack.find_first_not_of(toTrim);
        haystack.remove_prefix(strBegin);

        return haystack;
    }

    template <typename Iter>
    std::string Join(Iter begin, Iter end, std::string_view separator = "")
    {
        if (!(begin != end))
        {
            return "";
        }

        std::stringstream ss;
        Streamable&& wrap{MakeOStreamStreamable{ss}};
        wrap << *begin;

        auto aggregate = [&wrap, &separator](auto s)
        {
            wrap << separator.data() << s;
        };
        std::for_each(++begin, end, aggregate);

        return ss.str();
    }

    //! argument in rangeV3-style version:
    template <typename Container>
    std::string Join(const Container& c, std::string_view separator = "")
    {
        return Join(c.begin(), c.end(), separator);
    }

    //! closest possible form of python's `in` keyword
    template <typename Element, typename Container>
    bool IsIn(const Element& element, const Container& container)
    {
        return std::find(container.begin(), container.end(), element) != container.end();
    }

    enum class CopyIfPolicy
    {
        ForAll,
        InterruptAtFirstFalse,
    };

    //! inserts elements into the output iterator if they pass a predicate
    template <typename InputIterator, typename Predicate, typename OutputIterator>
    void CopyIf(
        InputIterator begin,
        InputIterator end,
        Predicate pred,
        OutputIterator out,
        CopyIfPolicy policy)
    {
        for (auto it = begin; it != end; ++it)
        {
            if (pred(*it))
            {
                *out = *it;
            }
            else if (policy == CopyIfPolicy::InterruptAtFirstFalse)
            {
                break;
            }
        }
    }

    inline std::string Unescape(std::string_view escapedText)
    {
        std::stringstream out;
        auto it = escapedText.begin();
        while (it != escapedText.end())
        {
            char c = *it;
            if (c == '\\' && (it + 1) != escapedText.end())
            {
                ++it;
                char next = *it;
                switch (next)
                {
                case '\\': c = '\\';
                    break;
                case 'a': c = '\a';
                    break;
                case 'b': c = '\b';
                    break;
                case 'f': c = '\f';
                    break;
                case 'n': c = '\n';
                    break;
                case 'r': c = '\r';
                    break;
                case 't': c = '\t';
                    break;
                case 'v': c = '\v';
                    break;
                case '"': c = '\"';
                    break;
                default:
                    out << '\\';
                    c = next;
                    break;
                }
            }
            out << c;
            ++it;
        }
        return out.str();
    }

    template <typename T>
    inline std::string ToString(T anything)
    {
        std::ostringstream oss;
        oss << anything;
        return oss.str();
    }

    inline std::string&& ToString(std::string&& s)
    {
        return std::forward<std::string&&>(s);
    }

    inline const std::string& ToString(const std::string& s)
    {
        return s;
    }

    inline std::string ToString(const char* const s)
    {
        return s;
    }

    inline std::string ToString(std::string_view sv)
    {
        return std::string{sv};
    }

    template <typename... Types>
    std::string ConcatString(Types&&... args)
    {
        std::string ret;
        (ret += ... += ToString(args));
        return ret;
    }

    template <typename... Args>
    std::string FormatString(const char* format, Args... args)
    {
        int size = snprintf(nullptr, 0, format, args...) + 1; // Extra space for '\0'
        assert(size > 0);
        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, format, args...);
        return std::string(buf.get());
    }

    // Is-One-Of will check if a variable is equal to any of the values listed on the other parameters.
    // Example: IsOneOf(variableKind, Function, Enumeration) is short for: variableKind == Function || variableKind == Enumeration.
    // 2 arguments count: recursion terminal overload.
    template <typename T, typename U>
    bool IsOneOf(T value, U tocheck)
    {
        return value == tocheck;
    }

    // Any argument count version
    template <typename T, typename U, typename... Args>
    bool IsOneOf(T value, U khead, Args... tail)
    {
        return value == khead || IsOneOf(value, tail...);
    }

    // shortcut for dynamic cast to open the opportunity to migrate to adhoc rtti later
    template <typename DestT, typename SrcT>
    DestT As(SrcT source)
    {
        static_assert((std::is_pointer_v<SrcT> && std::is_pointer_v<DestT>) || (std::is_reference_v<SrcT> && std::is_reference_v<DestT>));
        return dynamic_cast<DestT>(source);
    }

    // shortcut of As<> != nullptr
    template <typename DestT, typename SrcT>
    bool Is(SrcT source)
    {
        using PtrDestT = std::conditional_t<std::is_pointer_v<DestT>, DestT, DestT*>;
        using ConsistentDestT = std::conditional_t<std::is_pointer_v<SrcT>, PtrDestT, DestT>;
        return As<ConsistentDestT>(source) != nullptr;
    }

    // tail case
    template <typename Deduced>
    bool DynamicTypeIsAnyOf(Deduced* base)
    {
        return false;
    }

    // checks if a passed pointer to an instance, is-base-or-derived-of any one of the given types in the template argument list.
    // verify that As< at-least-one-of-args >(base) returns non-null
    template <typename Head, typename... Tail, typename Deduced>
    bool DynamicTypeIsAnyOf(Deduced* base)
    {
        return Is<Head>(base) || DynamicTypeIsAnyOf<Tail...>(base);
    }

    template <typename EnumTypeTemplateParam>
    struct Flag
    {
        template <class T>
        using SubEnumType = typename T::EnumType;

        // transform the input parameter into the underlying enum. in the case that the client instantiated the Flag from a MAKE_REFLECTABLE_ENUM_POWER macro.
        // or just keep the original client type.
        using EnumType = DetectedOr_t<EnumTypeTemplateParam, SubEnumType, EnumTypeTemplateParam>;

        static_assert(std::is_enum_v<EnumType>);

        Flag()
        {
            if constexpr (typename DetectedOr<EnumTypeTemplateParam, SubEnumType, EnumTypeTemplateParam>::value_t())
            {
#ifndef NDEBUG
                using It = typename EnumTypeTemplateParam::Iterator;
                assert(*(++It{ EnumType(4) }) == 8); // verify that the reflectable enum is a power enum (flag-able).
#endif
            }
        }

        Flag(EnumType e)
            : m_value{static_cast<UnderlyingT>(e)}
        {
        }

        friend Flag operator &(Flag f, EnumType e)
        {
            return EnumType(f.m_value & static_cast<UnderlyingT>(e));
        }

        friend Flag operator &(Flag lhs, Flag rhs)
        {
            return Flag(EnumType(lhs.m_value & rhs.m_value));
        }

        friend Flag operator |(Flag f, EnumType e)
        {
            return EnumType(f.m_value | static_cast<UnderlyingT>(e));
        }

        friend Flag& operator &=(Flag& f, EnumType e)
        {
            f.m_value &= static_cast<UnderlyingT>(e);
            return f;
        }

        friend Flag& operator &=(Flag& f, const Flag& f2)
        {
            f.m_value &= f2.m_value;
            return f;
        }

        friend Flag& operator |=(Flag& f, EnumType e)
        {
            f.m_value |= static_cast<UnderlyingT>(e);
            return f;
        }

        friend Flag& operator |=(Flag& f, const Flag& f2)
        {
            f.m_value |= f2.m_value;
            return f;
        }

        friend Flag operator ~(const Flag& a_f)
        {
            return Flag(EnumType(~a_f.m_value));
        }

        bool operator ==(const Flag& rhs) const
        {
            return m_value == rhs.m_value;
        }

        bool operator !=(const Flag& rhs) const
        {
            return m_value != rhs.m_value;
        }

        explicit operator bool() const
        {
            return m_value != 0;
        }

        bool AsBool() const
        {
            return operator bool();
        }

        bool IsEmpty() const
        {
            return m_value == 0;
        }

        using UnderlyingT = std::underlying_type_t<EnumType>;
        UnderlyingT m_value = 0;
    };

    inline std::string_view GetCurrentOsName()
    {
#if defined(_WIN64) || defined(_WIN32)
        return "Windows"sv;
#elif defined (__APPLE__)
        return "Mac"sv;
#elif defined (__linux__)
        return "Linux"sv;
#elif defined(__unix__) || defined(_POSIX_VERSION)
        return "Unix"sv;
#elif
        return "UNKNOWN"sv;
#endif
    }

    //! Log(N) query to find the first immediately lower or equal element in a map's keys
    template <typename T, typename U>
    auto Infimum(const std::map<T, U>& ctr, T query)
    {
        auto it = ctr.upper_bound(query);
        return it == ctr.begin() ? ctr.end() : --it;
    }

    //! Log(N) disjointed segments belong query
    //! You can represent segments as you wish, as long as:
    //!   you provide the predicate to determine belonging.
    //!   map-keys are segment start points.
    //!   segments don't overlap.
    //! returns: iterator to found interval key, or cend()
    template <typename T, typename U, typename IntervalCheckPredicate>
    auto FindIntervalInDisjointSet(const std::map<T, U>& ctr, const T& query, IntervalCheckPredicate&& isInIntervalPredicate)
    {
        auto inf = Infimum(ctr, query);
        bool isInInterval = inf != ctr.end() && isInIntervalPredicate(query, inf->second);
        return isInInterval ? inf : ctr.end();
    }

    //! Log(N) disjointed segments belong query
    //! segments are represented by their start points in the key, and last point in values. segments can't overlap.
    //! returns: iterator to found interval key, or cend()
    template <typename T, typename U>
    auto FindIntervalInDisjointSet(const std::map<T, U>& ctr, const T& query)
    {
        return FindIntervalInDisjointSet(
            ctr,
            query,
            [](T q, U last)
            {
                return q <= last;
            });
    }

    template <typename T>
    struct Interval
    {
        bool IsEmpty() const
        {
            return b < a;
        }

        bool operator==(const Interval& rhs) const
        {
            return a == rhs.a && b == rhs.b;
        }

        bool operator<(const Interval& rhs) const
        {
            return a < rhs.a || (a == rhs.a && b < rhs.b);
        }

        T a = (T)0;
        T b = (T)-1;
    };

    //! In case of potential overlaps (not disjointed), this structure can support "is in" queries
    template <typename T>
    struct IntervalCollection
    {
        using IntervalT = Interval<T>;

        void Add(IntervalT i)
        {
            m_obfirsts.emplace_back(i);
        }

        //! doesn't respect RAII but for the sake of performance and convenience this is easier this way
        void Seal()
        {
            m_oblasts = m_obfirsts;
            std::sort(
                m_obfirsts.begin(),
                m_obfirsts.end(),
                [](auto i1, auto i2)
                {
                    return i1.a < i2.a;
                });
            std::sort(
                m_oblasts.begin(),
                m_oblasts.end(),
                [](auto i1, auto i2)
                {
                    return i1.b < i2.b;
                });
            m_sealed = true;
        }

        //! Retrieve the subset of intervals activated by a point (query)
        std::set<IntervalT> GetIntervalsSurrounding(T query) const
        {
            assert(m_sealed);
            // find the "set" of intervals starting before:
            auto firstsSubEnd = std::lower_bound(
                m_obfirsts.begin(),
                m_obfirsts.end(),
                query,
                [=](auto interv, T q)
                {
                    return interv.a <= q;
                });

            // find the "set" of intervals ending after:
            static std::vector<IntervalT> endAfter;
            endAfter.clear();
            CopyIf(
                m_oblasts.rbegin(),
                m_oblasts.rend(),
                // reverse iteration
                [=](auto interv)
                {
                    return interv.b >= query;
                },
                std::back_inserter(endAfter),
                CopyIfPolicy::InterruptAtFirstFalse);
            // for set_intersection to work, the less<> predicate has to work for both ranges
            std::sort(endAfter.begin(), endAfter.end());

            std::set<IntervalT> result;
            std::set_intersection(
                m_obfirsts.begin(),
                firstsSubEnd,
                endAfter.begin(),
                endAfter.end(),
                std::inserter(result, result.end()));
            return result;
        }

        //! Get the interval surrounding query that has the closest start point to query.
        //! In case of an interval collection representing a tree, that is,
        //! each overlapping interval is fully contained in the bigger one,
        //! the closest start is guaranteed to be the most "leaf" interval.
        //! This is useful for scopes.
        IntervalT GetClosestIntervalSurrounding(T query) const
        {
            auto bag = GetIntervalsSurrounding(query);
            return bag.empty() ? IntervalT{-1, -2} : *bag.rbegin();
        }

        std::vector<IntervalT> m_obfirsts; // ordered by "firsts"
        std::vector<IntervalT> m_oblasts; // ordered by "lasts"
        bool m_sealed = false;
    };

    template <typename Deduced>
    decltype(auto) CastToRValueReference(Deduced&& value)
    {
        return static_cast<std::remove_reference_t<Deduced>&&>(value);
    }

    //! add a missing operator for convenience and shortness of code
    inline bool operator ==(std::string_view lhs, char rhs)
    {
        return lhs.length() == 1 && lhs[0] == rhs;
    }

    inline std::string ToLower(std::string_view original)
    {
        std::string result;
        auto len = original.length();
        result.resize(len);
        for (int i = 0; i < len; ++i)
        {
            result[i] = static_cast<char>(tolower(original[i]));
        }
        return result;
    }

    inline bool IsPowerOfTwo(uint32_t value)
    {
        return (value > 0) && !(value & (value - 1));
    }

    //! Insert rhs at the end of lhs
    template <typename T>
    void AppendVector(std::vector<T>& lhs, const std::vector<T>& rhs)
    {
        using std::begin, std::end;
        lhs.insert(end(lhs), begin(rhs), end(rhs));
    }

    //! Stable algorithm to uniquify elements of a vector (preserving order).
    //! Solution Mohammed Hossain/Yuri https://stackoverflow.com/a/34341344/893406
    template <typename T>
    size_t RemoveDuplicatesKeepOrder(std::vector<T>& vec)
    {
        std::unordered_set<T> seen;
        auto newEnd = std::remove_if(
            vec.begin(),
            vec.end(),
            [&seen](const T& value)
            {
                if (seen.find(value) != std::end(seen))
                {
                    return true;
                }

                seen.insert(value);
                return false;
            });
        vec.erase(newEnd, vec.end());
        return vec.size();
    }

    //! Append rhs to lhs and remove duplicates
    template <typename T>
    void StableMerge(std::vector<T>& lhs, const std::vector<T>& rhs)
    {
        AppendVector(lhs, rhs);
        RemoveDuplicatesKeepOrder(lhs);
    }

    //! Conditional swap algorithm
    template <typename T>
    void SwapIf(T&& a, T&& b, bool condition)
    {
        if (condition)
        {
            std::swap(std::forward<T>(a), std::forward<T>(b));
        }
    }
}

#ifndef NDEBUG

namespace AZ::Tests
{
    inline void DoAsserts2()
    {
        DoAsserts3();

        assert(Slice("0123", 1, -1) == "123");
        assert(Slice("0123", 1, 2) == "1");
        assert(Slice("0123", -3, -2) == "2"); //*
        assert(Slice("0123", -2, -1) == "3");

        assert(Unescape(R"(my beautiful string\non two lines.)") == "my beautiful string\non two lines.");
        assert(Unescape(R"(just\ a backslash)") == R"(just\ a backslash)");
        assert(Unescape(R"(real \\backslash)") == "real \\backslash");
        assert(Unescape(R"(unrecog \j escape)") == R"(unrecog \j escape)");
        assert(Unescape(R"(at end\)") == R"(at end\)");
        assert(Unescape(R"(with \"quotes\")") == R"(with "quotes")");

        assert("bleu"sv.starts_with("bl"));
        assert(!"0bleu"sv.starts_with("bl"));
        assert("bleu"sv.starts_with(""sv));
        assert(!""sv.starts_with("0"));

        assert(!""sv.ends_with("0"));
        assert(!"stuff"sv.ends_with("0"));
        assert(""sv.ends_with(""sv));
        assert("0"sv.ends_with("0"));
        assert("00"sv.ends_with("0"));
        assert("nice"sv.ends_with(""sv));
        assert("nice"sv.ends_with("e"));
        assert("nice"sv.ends_with("ce"));
        assert("nice"sv.ends_with("nice"));

        // initializer list
        auto list = {"nice", "things", "come", "to", "an", "end"};
        assert(Decorate("", Join(list.begin(), list.end(), ", "), ".") == "nice, things, come, to, an, end.");

        assert(Undecorate("///", Decorate("///", "/my nice string/")) == "/my nice string/"sv);
        assert(Undecorate("\"", "non decorated") == "non decorated"sv);
        assert(Undecorate("\"", "\"decorated\"") == "decorated"sv);

        // or collection
        std::set<std::string> myset = {"zz", "mm", "aa", "bb"};
        assert(Join(myset.begin(), myset.end()) == "aabbmmzz");

        assert(Trim("\"stuff\"", "\"") == "stuff"sv);
        assert(Trim(" stuff ") == "stuff"sv);
        assert(Trim(" stuff ", ".") == " stuff "sv);
        assert(Trim("  ", " ") == ""sv);

        auto unevenSplit = Split<std::vector<std::string>>("A, BB, CCC, DDDD", ", ");
        assert(Split<std::vector<std::string>>("A, BB, CCC, DDDD", ", ")[0] == "A");
        assert(Split<std::vector<std::string>>("A, BB, CCC, DDDD", ", ")[1] == "BB");
        assert(Split<std::vector<std::string>>("A, BB, CCC, DDDD", ", ")[2] == "CCC");
        assert(Split<std::vector<std::string>>("A, BB, CCC, DDDD", ", ")[3] == "DDDD");
        auto halfSplit = Split<std::vector<std::string>>("A, BB, CCC, DDDD", "C");
        assert(halfSplit[0] == "A, BB, ");
        assert(halfSplit[1] == ", DDDD");

        {
            using Map = std::map<int, int>;
            Map intervals{{2, 5}, {8, 9}};

            auto red = Infimum(intervals, 4);
            assert(red->first == 2);
            auto green = Infimum(intervals, 6);
            assert(green->first == 2);
            auto pink = Infimum(intervals, 8);
            assert(pink->first == 8);
            auto yellow = Infimum(intervals, 1);
            assert(yellow == intervals.end());
            auto larger_than_all = Infimum(intervals, 15);
            assert(larger_than_all->first == 8);
            assert(FindIntervalInDisjointSet(intervals, 4) != intervals.end());
            assert(FindIntervalInDisjointSet(intervals, 6) == intervals.end());
            assert(FindIntervalInDisjointSet(intervals, 8) != intervals.end());
            assert(FindIntervalInDisjointSet(intervals, 1) == intervals.end());

            auto high = Infimum(intervals, 20);
            assert(high->first == 8);
        }

        assert(Replace("Srg/A", "/", "::") == "Srg::A");

        assert(WithinMatchedParentheses("a(b)", 2));
        assert(WithinMatchedParentheses("a()", 1));
        assert(!WithinMatchedParentheses("a()", 2));
        assert(!WithinMatchedParentheses("a(b)", 0));
        assert(!WithinMatchedParentheses("a(b)c", 4));
        assert(!WithinMatchedParentheses("a(b)c(d)", 4));
        assert(WithinMatchedParentheses("a((b)c(d))", 5));

        assert(IsIn("hibou", std::initializer_list<const char*>{ "chouette", "hibou", "jay" }));
        assert(!IsIn("hibou", std::initializer_list<const char*>{ "chouette", "jay" }));

        Interval<int> intvs[] = {{0, 10}, {1, 5}, {3, 3}, {7, 9}, {12, 15}};
        IntervalCollection<int> ic;
        std::for_each(
            std::begin(intvs),
            std::end(intvs),
            [&](auto i)
            {
                ic.Add(i);
            });
        ic.Seal();
        assert(ic.GetClosestIntervalSurrounding(-3).IsEmpty());
        assert((ic.GetClosestIntervalSurrounding(0) == Interval<int>{0,10}));
        assert((ic.GetClosestIntervalSurrounding(1) == Interval<int>{1,5}));
        assert((ic.GetClosestIntervalSurrounding(3) == Interval<int>{3,3}));
        assert((ic.GetClosestIntervalSurrounding(4) == Interval<int>{1,5}));
        assert((ic.GetClosestIntervalSurrounding(6) == Interval<int>{0,10}));
        assert((ic.GetClosestIntervalSurrounding(5) == Interval<int>{1,5}));
        assert((ic.GetClosestIntervalSurrounding(7) == Interval<int>{7,9}));
        assert((ic.GetClosestIntervalSurrounding(9) == Interval<int>{7,9}));
        assert(ic.GetClosestIntervalSurrounding(11).IsEmpty());
        assert((ic.GetClosestIntervalSurrounding(13) == Interval<int>{12,15}));
        assert(ic.GetClosestIntervalSurrounding(16).IsEmpty());
    }
}
#endif
