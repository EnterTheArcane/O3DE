/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#if defined(HAVE_BENCHMARK)

#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/containers/array.h>

namespace UnitTest
{
    namespace
    {
        struct ByteDeltaValue final
        {
            ByteDeltaValue()
            {
                for (size_t index = 0; index < m_bytes.size(); ++index)
                {
                    m_bytes[index] = static_cast<AZ::u8>(index);
                }
            }

            bool Serialize(AzNetworking::ISerializer& serializer)
            {
                AZ::u32 size = static_cast<AZ::u32>(m_bytes.size());
                return serializer.SerializeBytes(
                    m_bytes.data(),
                    static_cast<AZ::u32>(m_bytes.size()),
                    false,
                    size,
                    "Bytes");
            }

            AZStd::array<AZ::u8, 512> m_bytes;
        };

        struct SymbolDeltaValue final
        {
            bool Serialize(AzNetworking::ISerializer& serializer)
            {
                return serializer.Serialize(m_symbol, "Symbol");
            }

            AZ::Symbol m_symbol;
        };

        void BM_DeltaSerializerBytesUnchanged(benchmark::State& state)
        {
            ByteDeltaValue base;
            ByteDeltaValue current;
            for ([[maybe_unused]] auto iteration : state)
            {
                AzNetworking::SerializerDelta delta;
                AzNetworking::DeltaSerializerCreate serializer(delta);
                benchmark::DoNotOptimize(serializer.CreateDelta(base, current));
            }
            state.SetItemsProcessed(state.iterations());
        }
        BENCHMARK(BM_DeltaSerializerBytesUnchanged);

        void BM_DeltaSerializerBytesChanged(benchmark::State& state)
        {
            ByteDeltaValue base;
            ByteDeltaValue current;
            current.m_bytes.back() ^= 0xFF;
            for ([[maybe_unused]] auto iteration : state)
            {
                AzNetworking::SerializerDelta delta;
                AzNetworking::DeltaSerializerCreate serializer(delta);
                benchmark::DoNotOptimize(serializer.CreateDelta(base, current));
            }
            state.SetItemsProcessed(state.iterations());
        }
        BENCHMARK(BM_DeltaSerializerBytesChanged);

        void BM_DeltaSerializerSymbolUnchanged(benchmark::State& state)
        {
            const AZ::Symbol symbol{AZStd::string_view{"Benchmark.Symbol.Component.Property"}};
            SymbolDeltaValue base{symbol};
            SymbolDeltaValue current{symbol};
            for ([[maybe_unused]] auto iteration : state)
            {
                AzNetworking::SerializerDelta delta;
                AzNetworking::DeltaSerializerCreate serializer(delta);
                benchmark::DoNotOptimize(serializer.CreateDelta(base, current));
            }
            state.SetItemsProcessed(state.iterations());
        }
        BENCHMARK(BM_DeltaSerializerSymbolUnchanged);

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

#endif // HAVE_BENCHMARK
