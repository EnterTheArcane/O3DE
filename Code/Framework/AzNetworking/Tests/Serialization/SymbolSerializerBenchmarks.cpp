/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifdef HAVE_BENCHMARK

#include <AzNetworking/Serialization/NetworkInputSerializer.h>

#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/containers/array.h>

#include <benchmark/benchmark.h>

namespace UnitTest
{
    namespace
    {
        void BM_SymbolSerializerRaw(benchmark::State& state)
        {
            const AZ::Symbol symbol{AZStd::string_view{"Benchmark.Symbol.Component.Property"}};
            AZStd::array<AZ::u8, 2048> buffer;

            for ([[maybe_unused]] auto iteration : state)
            {
                AZ::Symbol serializedSymbol = symbol;
                AzNetworking::NetworkInputSerializer serializer(
                    buffer.data(),
                    static_cast<AZ::u32>(buffer.size()));
                benchmark::DoNotOptimize(
                    static_cast<AzNetworking::ISerializer&>(serializer).Serialize(serializedSymbol, "Symbol"));
            }
            state.SetItemsProcessed(state.iterations());
        }
        BENCHMARK(BM_SymbolSerializerRaw);
    } // namespace
} // namespace UnitTest

#endif
