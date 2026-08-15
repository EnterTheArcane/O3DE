/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Platform/Metal/HairComputeSystem.h>

#include <GpuHairTestSupport.h>

#include <Jolt/Compute/MTL/ComputeSystemMTLImpl.h>

#include <Metal/Metal.h>

namespace Jolt
{
    namespace
    {
        class DirectDeviceHairComputeProvider final
            : public IHairComputeProvider
        {
        public:
            explicit DirectDeviceHairComputeProvider(
                void* device)
                : m_device(device)
            {
            }

            [[nodiscard]]
            HairComputeBackend GetBackend() const override
            {
                return HairComputeBackend::PlatformGpu;
            }

            [[nodiscard]]
            JPH::ComputeSystemResult CreateComputeSystem() override
            {
                return Platform::Metal::CreateHairComputeSystem(m_device);
            }

        private:
            void* m_device = nullptr;
        };
    } // namespace

    TEST(GpuHairMetalTests, SimulatesAndReadsBackOnMetalComputeBackend)
    {
        @autoreleasepool
        {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (!device)
            {
                GTEST_SKIP() << "No Metal device is available.";
            }

            DirectDeviceHairComputeProvider provider(device);
            Tests::SimulateAndValidateGpuHair(provider);
            [device release];
        }
    }

    TEST(GpuHairMetalTests, StandaloneFactoryCreatesComputeSystem)
    {
        @autoreleasepool
        {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (!device)
            {
                GTEST_SKIP() << "No Metal device is available.";
            }
            [device release];

            const JPH::ComputeSystemResult computeSystem = JPH::CreateComputeSystemMTL();
            ASSERT_FALSE(computeSystem.HasError()) << computeSystem.GetError().c_str();
            EXPECT_TRUE(computeSystem.Get());
        }
    }
} // namespace Jolt
