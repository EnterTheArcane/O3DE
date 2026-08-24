/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolTable.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Symbol/SymbolLiteral.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <xxhash.h>

namespace AZ::SymbolBenchmarks
{
    class SymbolBenchmarkFixture
        : public UnitTest::AllocatorsBenchmarkFixture
    {
    };

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, CachedLiteral)(::benchmark::State& state)
    {
        using namespace AZ::Literals;

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize("FrequentlyUsedSymbol"_sym);
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, CachedLiteral);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, DynamicCacheHit)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Value{"FrequentlyUsedDynamicSymbol"};
        benchmark::DoNotOptimize(Symbol{Value});

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol{Value});
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, DynamicCacheHit);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, DynamicCacheHitContended)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Value{"FrequentlyUsedContendedSymbol"};
        benchmark::DoNotOptimize(Symbol{Value});

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol{Value});
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, DynamicCacheHitContended)->Threads(1)->Threads(8)->Threads(32);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, Equality)(::benchmark::State& state)
    {
        const Symbol lhs{"EqualitySymbol"};
        const Symbol rhs{"EqualitySymbol"};

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(lhs == rhs);
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, Equality);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, Hash)(::benchmark::State& state)
    {
        const Symbol symbol{"HashSymbol"};

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(SymbolHash{}(symbol));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, Hash);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, FindHit)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Value{"FrequentlyFoundSymbol"};
        benchmark::DoNotOptimize(Symbol{Value});

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol::Find(Value));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, FindHit)->Threads(1)->Threads(8)->Threads(32);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, FindMiss)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Value{"SymbolBenchmarkMissingValue"};

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol::Find(Value));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, FindMiss);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, Validate)(::benchmark::State& state)
    {
        const AZStd::string value(static_cast<size_t>(state.range(0)), 'v');

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol::IsValid(value));
        }
        state.SetBytesProcessed(state.iterations() * value.size());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, Validate)->Arg(8)->Arg(64)->Arg(1023);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, MaximumLengthDynamicHit)(::benchmark::State& state)
    {
        const AZStd::string value(Symbol::MaxStringSize, 'm');
        benchmark::DoNotOptimize(Symbol{value});

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol{value});
        }
        state.SetBytesProcessed(state.iterations() * value.size());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, MaximumLengthDynamicHit);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, ColdTableInsertion)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Value{"ColdTableSymbol"};
        const u64 hash = XXH3_64bits(Value.data(), Value.size());

        for ([[maybe_unused]] auto iteration : state)
        {
            Internal::SymbolTable table;
            benchmark::DoNotOptimize(table.Intern(Value, hash));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, ColdTableInsertion);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, PopulatedTableFind)(::benchmark::State& state)
    {
        const size_t valueCount = static_cast<size_t>(state.range(0));
        AZStd::vector<AZStd::string> values;
        values.reserve(valueCount);
        Internal::SymbolTable table;
        for (size_t index = 0; index < valueCount; ++index)
        {
            values.emplace_back(AZStd::string::format("PopulatedSymbol%zu", index));
            const AZStd::string_view value = values.back();
            benchmark::DoNotOptimize(table.Intern(value, XXH3_64bits(value.data(), value.size())));
        }

        const AZStd::string_view target = values[valueCount / 2];
        const u64 hash = XXH3_64bits(target.data(), target.size());
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(table.Find(target, hash));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, PopulatedTableFind)->Arg(16)->Arg(256)->Arg(4096);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, CollisionFind)(::benchmark::State& state)
    {
        constexpr u64 Hash = 0xC0111510;
        constexpr size_t ValueCount = 64;
        AZStd::array<AZStd::string, ValueCount> values;
        Internal::SymbolTable table;
        for (size_t index = 0; index < ValueCount; ++index)
        {
            values[index] = AZStd::string::format("CollisionBenchmark%zu", index);
            benchmark::DoNotOptimize(table.Intern(values[index], Hash));
        }

        const AZStd::string_view target = values.back();
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(table.Find(target, Hash));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, CollisionFind);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, Xxh3)(::benchmark::State& state)
    {
        AZStd::array<u8, 1024> input{};
        for (size_t index = 0; index < input.size(); ++index)
        {
            input[index] = static_cast<u8>(index * 37 + 11);
        }

        const size_t length = static_cast<size_t>(state.range(0));
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(XXH3_64bits(input.data(), length));
        }
        state.SetBytesProcessed(state.iterations() * length);
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, Xxh3)->Arg(8)->Arg(24)->Arg(64)->Arg(128)->Arg(240)->Arg(241)->Arg(1023);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, SeededXxh3)(::benchmark::State& state)
    {
        AZStd::array<u8, 1024> input{};
        for (size_t index = 0; index < input.size(); ++index)
        {
            input[index] = static_cast<u8>(index * 37 + 11);
        }

        constexpr XXH64_hash_t Seed = 0x0123456789ABCDEFull;
        const size_t length = static_cast<size_t>(state.range(0));
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(XXH3_64bits_withSeed(input.data(), length, Seed));
        }
        state.SetBytesProcessed(state.iterations() * length);
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, SeededXxh3)->Arg(8)->Arg(24)->Arg(64)->Arg(128)->Arg(240)->Arg(241)->Arg(1023);
} // namespace AZ::SymbolBenchmarks
