/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Memory/SystemAllocator.h>

#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Memory/AllocationRecords.h>
#include <AzCore/Memory/OSAllocator.h>

#include <AzCore/std/functional.h>

#include <AzCore/Debug/MemoryProfiler.h>
#include <AzCore/Debug/Profiler.h>
#include <memory>

// Please note that AZCORE_SYSTEM_ALLOCATOR
// are ONLY considered #defined for AzCore static library itself.
// Do not use them in a header file or any other file.
// If you need to change the system allocator behavior based on this define,
//  then override the function from IAllocator in the header (without depending on the define),
//  and then implement the different behavior in this cpp or another cpp in AzCore.

// HPHA uses the high performance heap allocator for system allocations
#define AZCORE_SYSTEM_ALLOCATOR_HPHA 1

// malloc uses basic OS malloc for system allocations.  This is useful when using ASAN or other memory checking such as the CRT debug heap.
// when ASAN is enabled by CMake, this is set by default.  See AZCORE_SYSTEM_ALLOCATOR_MALLOC in AzCore's CMakeLists.txt
#define AZCORE_SYSTEM_ALLOCATOR_MALLOC 2

#if !defined(AZCORE_SYSTEM_ALLOCATOR)
    // We are using here AZCORE_SYSTEM_ALLOCATOR_HPHA for the sake of passing unit tests.
    // But it's been found that, when using Vulkan, and working with levels that have
    // large amount of meshes, entering/Exiting game mode puts lost of stress in memory allocation that crashes
    // when using HPHA. With AZCORE_SYSTEM_ALLOCATOR_MALLOC crashes don't occur.
    // TODO: Review Github Issue #18597
    #define AZCORE_SYSTEM_ALLOCATOR AZCORE_SYSTEM_ALLOCATOR_HPHA
#endif

#if (AZCORE_SYSTEM_ALLOCATOR != AZCORE_SYSTEM_ALLOCATOR_HPHA) && (AZCORE_SYSTEM_ALLOCATOR != AZCORE_SYSTEM_ALLOCATOR_MALLOC)
    #error AZCORE_SYSTEM_ALLOCATOR is an invalid value, it needs to be either AZCORE_SYSTEM_ALLOCATOR_HPHA or AZCORE_SYSTEM_ALLOCATOR_MALLOC
#endif

#include <AzCore/Memory/HphaAllocator.h>

#if AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC
#include <AzCore/std/parallel/atomic.h>
#endif

#if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sanitizer/asan_interface.h>
#endif

namespace AZ
{
#if AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC
    namespace SystemAllocatorPrivate
    {
        // when using malloc, we track the number of allocated bytes directly instead of via a sub allocator.
        // note that there should only be one instance, ever, of system allocator, and it is always accessed via the environment
        // which ensures that the code below is always running in the same context (usually in o3dekernel shared library).
        static AZStd::atomic<SystemAllocator::size_type> g_AllocatedBytes = {0};

#if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
        struct AllocationHeader final
        {
            void* m_baseAddress = nullptr;
            SystemAllocator::size_type m_byteSize = 0;
            SystemAllocator::size_type m_allocationSize = 0;
        };

        AllocationHeader ReadAllocationHeader(void* pointer)
        {
            auto* header = reinterpret_cast<AllocationHeader*>(
                reinterpret_cast<std::uintptr_t>(pointer) - sizeof(AllocationHeader));
            __asan_unpoison_memory_region(header, sizeof(AllocationHeader));
            const AllocationHeader result = *header;
            __asan_poison_memory_region(header, sizeof(AllocationHeader));
            return result;
        }

        void* Allocate(SystemAllocator::size_type byteSize, SystemAllocator::align_type alignment)
        {
            SystemAllocator::align_type effectiveAlignment = alignment;
            if (effectiveAlignment < alignof(AllocationHeader))
            {
                effectiveAlignment = alignof(AllocationHeader);
            }

            const SystemAllocator::size_type allocationOverhead = sizeof(AllocationHeader) + effectiveAlignment - 1;
            if (byteSize > std::numeric_limits<SystemAllocator::size_type>::max() - allocationOverhead)
            {
                return nullptr;
            }

            const SystemAllocator::size_type allocationSize = byteSize + allocationOverhead;
            void* baseAddress = std::malloc(allocationSize);
            if (!baseAddress)
            {
                return nullptr;
            }

            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(baseAddress);
            const std::uintptr_t unaligned = base + sizeof(AllocationHeader);
            const std::uintptr_t aligned = (unaligned + effectiveAlignment - 1) & ~(effectiveAlignment - 1);
            auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
            std::construct_at(
                header,
                AllocationHeader{
                    .m_baseAddress = baseAddress,
                    .m_byteSize = byteSize,
                    .m_allocationSize = allocationSize,
                });

            const std::uintptr_t headerAddress = reinterpret_cast<std::uintptr_t>(header);
            if (base < headerAddress)
            {
                __asan_poison_memory_region(baseAddress, headerAddress - base);
            }
            __asan_poison_memory_region(header, sizeof(AllocationHeader));

            const std::uintptr_t allocationEnd = base + allocationSize;
            const std::uintptr_t payloadEnd = aligned + byteSize;
            if (payloadEnd < allocationEnd)
            {
                __asan_poison_memory_region(reinterpret_cast<void*>(payloadEnd), allocationEnd - payloadEnd);
            }

            return reinterpret_cast<void*>(aligned);
        }

        void Deallocate(void* pointer)
        {
            const AllocationHeader header = ReadAllocationHeader(pointer);
            __asan_unpoison_memory_region(header.m_baseAddress, header.m_allocationSize);
            std::free(header.m_baseAddress);
        }

        void* Reallocate(
            void* pointer,
            SystemAllocator::size_type newSize,
            SystemAllocator::align_type newAlignment)
        {
            if (newSize == 0)
            {
                if (pointer)
                {
                    Deallocate(pointer);
                }
                return nullptr;
            }

            if (!pointer)
            {
                return Allocate(newSize, newAlignment);
            }

            const SystemAllocator::size_type previousSize = ReadAllocationHeader(pointer).m_byteSize;
            void* newPointer = Allocate(newSize, newAlignment);
            if (!newPointer)
            {
                return nullptr;
            }

            SystemAllocator::size_type copySize = previousSize;
            if (newSize < copySize)
            {
                copySize = newSize;
            }
            std::memcpy(newPointer, pointer, copySize);
            Deallocate(pointer);
            return newPointer;
        }
#endif

        void RemoveAllocatedBytes(SystemAllocator::size_type byteSize)
        {
            SystemAllocator::size_type allocatedBytes = g_AllocatedBytes.load();
            while (allocatedBytes >= byteSize)
            {
                if (g_AllocatedBytes.compare_exchange_weak(allocatedBytes, allocatedBytes - byteSize))
                {
                    return;
                }
            }

            AZ_Assert(
                false,
                "SystemAllocator: Deallocating %zu bytes with only %zu bytes tracked!",
                byteSize,
                allocatedBytes);
        }

        SystemAllocator::size_type GetAllocatedSize(
            void* pointer,
            [[maybe_unused]] SystemAllocator::align_type alignment)
        {
#if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
            return ReadAllocationHeader(pointer).m_byteSize;
#else
            return AZ_OS_MSIZE(pointer, alignment);
#endif
        }
    };

#endif

    //////////////////////////////////////////////////////////////////////////
    AZ_TYPE_INFO_WITH_NAME_IMPL(SystemAllocator, "SystemAllocator", "{607C9CDF-B81F-4C5F-B493-2AD9C49023B7}");
    AZ_RTTI_NO_TYPE_INFO_IMPL(SystemAllocator, AllocatorBase);

    SystemAllocator::SystemAllocator()
    {
        AllocatorInstance<OSAllocator>::Get();
        Create();
        PostCreate();
    }

    //=========================================================================
    // ~SystemAllocator
    //=========================================================================
    SystemAllocator::~SystemAllocator()
    {
        PreDestroy();
        Destroy();
    }

    //=========================================================================
    // ~Create
    // [9/2/2009]
    //=========================================================================
    bool SystemAllocator::Create()
    {
        m_subAllocator = AZStd::make_unique<HphaSchema>();
        return true;
    }

    AllocatorDebugConfig SystemAllocator::GetDebugConfig()
    {
        return AllocatorDebugConfig()
            .StackRecordLevels(O3DE_STACK_CAPTURE_DEPTH)
            .UsesMemoryGuards()
            .MarksUnallocatedMemory()
            .ExcludeFromDebugging(false);
    }

    SystemAllocator::size_type SystemAllocator::NumAllocatedBytes() const
    {
#if (AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC)
        return SystemAllocatorPrivate::g_AllocatedBytes;
#else
        return m_subAllocator->NumAllocatedBytes();
 #endif
    }


    //=========================================================================
    // Allocate
    // [9/2/2009]
    //=========================================================================
    AllocateAddress SystemAllocator::allocate(size_type byteSize, size_type alignment)
    {
        if (byteSize == 0)
        {
            return AllocateAddress{};
        }

        AZ_Assert(byteSize > 0, "You can not allocate 0 or negative bytes!");
        AZ_Assert((alignment & (alignment - 1)) == 0, "Alignment must be power of 2!");

#if (AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC)
    #if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
        AllocateAddress address(SystemAllocatorPrivate::Allocate(byteSize, alignment), byteSize);
    #else
        AllocateAddress address(AZ_OS_MALLOC(byteSize, alignment), byteSize);
    #endif
        if (address)
        {
            SystemAllocatorPrivate::g_AllocatedBytes += SystemAllocatorPrivate::GetAllocatedSize(address.m_value, alignment);
        }
#else
        byteSize = MemorySizeAdjustedUp(byteSize);
        AllocateAddress address =
            m_subAllocator->allocate(byteSize, alignment);

        if (address == nullptr)
        {
            // Free all memory we can and try again!
            AllocatorManager::Instance().GarbageCollect();

            address = m_subAllocator->allocate(byteSize, alignment);
        }

        if (address == nullptr)
        {
            byteSize = MemorySizeAdjustedDown(byteSize); // restore original size
        }
#endif
        AZ_Assert(
            address != nullptr, "SystemAllocator: Failed to allocate %zu bytes aligned on %zu!", byteSize,
            alignment);

        AZ_PROFILE_MEMORY_ALLOC_EX(MemoryReserved, fileName, lineNum, address, byteSize, name);
        AZ_MEMORY_PROFILE(ProfileAllocation(address, byteSize, alignment, 1));

        return address;
    }

    //=========================================================================
    // DeAllocate
    // [9/2/2009]
    //=========================================================================
    auto SystemAllocator::deallocate(pointer ptr, size_type byteSize, [[maybe_unused]] size_type alignment) -> size_type
    {
        // It is valid to call "free" on a nullptr and it should produce no action.
        // Early out here to avoid calling something like AZ_OS_MSIZE, which may not be valid on nullptr.
        if (!ptr)
        {
            return 0;
        }

        #if (AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC)
            AZ_PROFILE_MEMORY_FREE(MemoryReserved, ptr);
            const size_type allocatedSize = SystemAllocatorPrivate::GetAllocatedSize(ptr, alignment);
            if (byteSize == 0)
            {
                byteSize = allocatedSize;
            }
            AZ_MEMORY_PROFILE(ProfileDeallocation(ptr, byteSize, alignment, nullptr));
            SystemAllocatorPrivate::RemoveAllocatedBytes(allocatedSize);
        #if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
            SystemAllocatorPrivate::Deallocate(ptr);
        #else
            AZ_OS_FREE(ptr);
        #endif
            return allocatedSize;
        #else
            byteSize = MemorySizeAdjustedUp(byteSize);
            AZ_PROFILE_MEMORY_FREE(MemoryReserved, ptr);
            AZ_MEMORY_PROFILE(ProfileDeallocation(ptr, byteSize, alignment, nullptr));
            return m_subAllocator->deallocate(ptr, byteSize, alignment);
        #endif
    }

    //=========================================================================
    // ReAllocate
    // [9/13/2011]
    //=========================================================================
    AllocateAddress SystemAllocator::reallocate(pointer ptr, size_type newSize, size_type newAlignment)
    {
        #if (AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC)
            AZ_PROFILE_MEMORY_FREE(MemoryReserved, ptr);
            size_type previousAllocatedSize = 0;
            if (ptr)
            {
                previousAllocatedSize = SystemAllocatorPrivate::GetAllocatedSize(ptr, newAlignment);
            }
        #if defined(AZCORE_ADDRESS_SANITIZER_ENABLED)
            AllocateAddress newAddress(SystemAllocatorPrivate::Reallocate(ptr, newSize, newAlignment), newSize);
        #else
            AllocateAddress newAddress(AZ_OS_REALLOC(ptr, newSize, newAlignment), newSize);
        #endif
            if (newAddress)
            {
                SystemAllocatorPrivate::RemoveAllocatedBytes(previousAllocatedSize);
                SystemAllocatorPrivate::g_AllocatedBytes +=
                    SystemAllocatorPrivate::GetAllocatedSize(newAddress.m_value, newAlignment);
            }
            else if (ptr && newSize == 0)
            {
                SystemAllocatorPrivate::RemoveAllocatedBytes(previousAllocatedSize);
            }
            [[maybe_unused]] const size_type allocatedSize = newSize;
        #else
            newSize = MemorySizeAdjustedUp(newSize);
            AZ_PROFILE_MEMORY_FREE(MemoryReserved, ptr);
            AllocateAddress newAddress = m_subAllocator->reallocate(ptr, newSize, newAlignment);
            [[maybe_unused]] const size_type allocatedSize = get_allocated_size(newAddress, 1);
        #endif

        AZ_PROFILE_MEMORY_ALLOC(MemoryReserved, newAddress, newSize, "SystemAllocator realloc");
        AZ_MEMORY_PROFILE(ProfileReallocation(ptr, newAddress, allocatedSize, newAlignment));

        return newAddress;
    }

    //=========================================================================
    //
    // [8/12/2011]
    //=========================================================================
    auto SystemAllocator::get_allocated_size(pointer ptr, align_type alignment) const -> size_type
    {
        if (!ptr)
        {
            return 0;
        }

        #if (AZCORE_SYSTEM_ALLOCATOR == AZCORE_SYSTEM_ALLOCATOR_MALLOC)
            return SystemAllocatorPrivate::GetAllocatedSize(ptr, alignment);
        #else
            size_type allocSize = MemorySizeAdjustedDown(m_subAllocator->get_allocated_size(ptr, alignment));
            return allocSize;
        #endif
    }

} // namespace AZ
