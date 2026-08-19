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
        constexpr AZStd::string_view Spelling{"FrequentlyUsedDynamicSymbol"};
        benchmark::DoNotOptimize(Symbol{Spelling});

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(Symbol{Spelling});
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, DynamicCacheHit);

    BENCHMARK_DEFINE_F(SymbolBenchmarkFixture, ColdTableInsertion)(::benchmark::State& state)
    {
        constexpr AZStd::string_view Spelling{"ColdTableSymbol"};
        const u64 hash = XXH3_64bits(Spelling.data(), Spelling.size());

        for ([[maybe_unused]] auto iteration : state)
        {
            Internal::SymbolTable table;
            benchmark::DoNotOptimize(table.Intern(Spelling, hash));
        }
        state.SetItemsProcessed(state.iterations());
    }
    BENCHMARK_REGISTER_F(SymbolBenchmarkFixture, ColdTableInsertion);

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
