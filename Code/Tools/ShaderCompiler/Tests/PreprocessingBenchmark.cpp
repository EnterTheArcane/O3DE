/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PreprocessingTokenSource.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace AZ::ShaderCompiler;

// This standalone measurement process counts ordinary C++ allocations.
// The source, macro, token and provenance containers use this allocation path.
static std::atomic<size_t> allocations{ 0 };

void* operator new(size_t size)
{
    ++allocations;
    size_t allocationSize = 1;
    if (size)
    {
        allocationSize = size;
    }
    if (void* memory = std::malloc(allocationSize))
    {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void* operator new[](size_t size)
{
    return ::operator new(size);
}

void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}

static size_t PeakResidentBytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    {
        return counters.PeakWorkingSetSize;
    }
    return 0;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
    {
        return 0;
    }
#ifdef __APPLE__
    return static_cast<size_t>(usage.ru_maxrss);
#else
    return static_cast<size_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}

int main(int argc, char** argv)
{
    try
    {
        CompilationUnit unit;
        PreprocessRequest request;
        if (argc > 1)
        {
            request.m_sourcePath = argv[1];
            for (int i = 2; i < argc; ++i)
            {
                std::string_view argument = argv[i];
                if (argument == "--include" && i + 1 < argc)
                {
                    request.m_forcedIncludes.emplace_back(argv[++i]);
                }
                else if (argument.substr(0, 2) == "-I")
                {
                    request.m_includeDirectories.emplace_back(argument.substr(2));
                }
                else if (argument.substr(0, 2) == "-D")
                {
                    request.m_macros.push_back({ false, std::string(argument.substr(2)) });
                }
                else
                {
                    throw std::invalid_argument("benchmark accepts a root file, -Idirectory, -Ddefinition and --include file");
                }
            }
        }
        else
        {
            std::string text = "#define BODY ";
            for (size_t i = 0; i < 100; ++i)
            {
                text += "very_long_shared_identifier + ";
            }
            text += "0\n";
            for (size_t i = 0; i < 1000; ++i)
            {
                text += "BODY;\n";
            }
            request.m_source = unit.m_sources.AddSource("benchmark", std::move(text));
        }

        const size_t before = allocations.load();
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        const PreprocessResult result = unit.Preprocess(request);
        const size_t preprocessAllocations = allocations.load() - before;
        const std::chrono::steady_clock::time_point preprocessed = std::chrono::steady_clock::now();
        PreprocessingTokenSource source(unit);
        antlr4::CommonTokenStream tokens(&source);
        tokens.fill();
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        std::cout
            << "{\"source_bytes\":" << unit.m_sources.SourceBytes() << ",\"generated_spelling_bytes\":" << unit.m_sources.GeneratedBytes()
            << ",\"generated_capacity_bytes\":" << unit.m_sources.GeneratedCapacity()
            << ",\"preprocessing_token_bytes\":" << result.m_tokens.size_bytes() << ",\"antlr_token_count\":" << tokens.size()
            << ",\"antlr_wrapper_bytes\":" << tokens.size() * sizeof(SourceToken)
            << ",\"location_capacity_bytes\":" << unit.m_sources.LocationBytes() << ",\"file_reads\":" << unit.m_sources.FileReads()
            << ",\"preprocessing_allocations\":" << preprocessAllocations
            << ",\"frontend_allocations\":" << allocations.load() - before
            << ",\"adapter_allocations\":" << allocations.load() - before - preprocessAllocations
            << ",\"peak_process_resident_bytes\":" << PeakResidentBytes()
            << ",\"preprocessing_ms\":" << std::chrono::duration<double, std::milli>(preprocessed - start).count()
            << ",\"adapter_ms\":" << std::chrono::duration<double, std::milli>(end - preprocessed).count() << "}\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
