/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Half.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <array>

#if defined(HAVE_BENCHMARK)
namespace UnitTest
{
    static void Half_EncodeFloat(benchmark::State& state)
    {
        float value = 0.3333f;
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(value);
            benchmark::DoNotOptimize(AZ::Half(value));
        }
    }
    BENCHMARK(Half_EncodeFloat);

    static void Half_EncodeDouble(benchmark::State& state)
    {
        double value = 0.3333;
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(value);
            benchmark::DoNotOptimize(AZ::Half(value));
        }
    }
    BENCHMARK(Half_EncodeDouble);

    static void Half_Decode(benchmark::State& state)
    {
        AZ::Half value(0.3333f);
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(value);
            benchmark::DoNotOptimize(value.ToFloat());
        }
    }
    BENCHMARK(Half_Decode);

    static void Half_EncodeArray(benchmark::State& state)
    {
        std::array<float, 4096> inputs;
        std::array<AZ::Half, 4096> outputs;
        for (size_t index = 0; index < inputs.size(); ++index)
        {
            inputs[index] = static_cast<float>(index) / 3.0f;
        }
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(inputs.data());
            for (size_t index = 0; index < inputs.size(); ++index)
            {
                outputs[index] = AZ::Half(inputs[index]);
            }
            benchmark::DoNotOptimize(outputs.data());
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(inputs.size()));
    }
    BENCHMARK(Half_EncodeArray);

    static void Half_DecodeArray(benchmark::State& state)
    {
        std::array<AZ::Half, 4096> inputs;
        std::array<float, 4096> outputs;
        for (size_t index = 0; index < inputs.size(); ++index)
        {
            inputs[index] = AZ::Half::FromBits(static_cast<AZ::u16>(index));
        }
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(inputs.data());
            for (size_t index = 0; index < inputs.size(); ++index)
            {
                outputs[index] = inputs[index].ToFloat();
            }
            benchmark::DoNotOptimize(outputs.data());
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(inputs.size()));
    }
    BENCHMARK(Half_DecodeArray);

    static void Half_CompoundChain(benchmark::State& state)
    {
        AZ::Half initial(0.5f);
        const AZ::Half scale(0.99f);
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(initial);
            AZ::Half value = initial;
            for (int operation = 0; operation < 32; ++operation)
            {
                value *= scale;
            }
            benchmark::DoNotOptimize(value);
        }
        state.SetItemsProcessed(state.iterations() * 32);
    }
    BENCHMARK(Half_CompoundChain);

    static void Half_FloatAccumulator(benchmark::State& state)
    {
        AZ::Half initial(0.5f);
        const AZ::Half scale(0.99f);
        for ([[maybe_unused]] auto _ : state)
        {
            benchmark::DoNotOptimize(initial);
            float value = initial;
            for (int operation = 0; operation < 32; ++operation)
            {
                value *= scale;
            }
            benchmark::DoNotOptimize(AZ::Half(value));
        }
        state.SetItemsProcessed(state.iterations() * 32);
    }
    BENCHMARK(Half_FloatAccumulator);
}
#endif
