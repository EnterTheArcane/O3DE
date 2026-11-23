/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <NvCloth/Callbacks.h>
#include <NvCloth/Factory.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>

#include <cstdio>
#include <cstdlib>
#include <new>

class Allocator final : public physx::PxAllocatorCallback
{
public:
    void* allocate(size_t size, const char*, const char*, int) override
    {
        return ::operator new(size, std::align_val_t(16));
    }

    void deallocate(void* block) override
    {
        ::operator delete(block, std::align_val_t(16));
    }
};

class ErrorCallback final : public physx::PxErrorCallback
{
public:
    void reportError(physx::PxErrorCode::Enum, const char* message, const char*, int) override
    {
        std::fputs(message, stderr);
        std::abort();
    }
};

int main()
{
    if (NvClothCompiledWithCudaSupport() != NVCLOTH_TEST_CUDA)
    {
        return 1;
    }
    Allocator allocator;
    ErrorCallback errors;
    nv::cloth::InitializeNvCloth(&allocator, &errors, nullptr, nullptr);
    auto* factory = NvClothCreateFactoryCPU();
    if (factory == nullptr)
    {
        return 1;
    }
    NvClothDestroyFactory(factory);
    return 0;
}
