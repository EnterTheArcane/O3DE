/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/GpuHairShaderLoader.h>

#include <GpuHairTestSupport.h>

#include <Jolt/Compute/VK/ComputeSystemVKImpl.h>

namespace Jolt
{
    namespace
    {
        class StandaloneHairComputeProvider final
            : public IHairComputeProvider
        {
        public:
            [[nodiscard]]
            HairComputeBackend GetBackend() const override
            {
                return HairComputeBackend::PlatformGpu;
            }

            [[nodiscard]]
            JPH::ComputeSystemResult CreateComputeSystem() override
            {
                JPH::ComputeSystemResult computeSystem = JPH::CreateComputeSystemVK();
                if (!computeSystem.HasError())
                {
                    computeSystem.Get()->mShaderLoader = LoadGpuHairShader;
                }

                return computeSystem;
            }
        };
    } // namespace

    TEST(GpuHairVulkanTests, SimulatesAndReadsBackOnVulkanComputeBackend)
    {
        StandaloneHairComputeProvider provider;
        Tests::SimulateAndValidateGpuHair(provider);
    }
} // namespace Jolt
