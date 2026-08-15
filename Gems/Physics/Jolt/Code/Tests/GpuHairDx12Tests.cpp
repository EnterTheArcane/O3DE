/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Platform/Windows/Dx12HairComputeSystem.h>

#include <GpuHairTestSupport.h>

#include <Jolt/Compute/DX12/ComputeSystemDX12Impl.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace Jolt
{
    namespace
    {
        class DirectDeviceHairComputeProvider final
            : public IHairComputeProvider
        {
        public:
            explicit DirectDeviceHairComputeProvider(
                ID3D12Device5& device)
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
                return Platform::Dx12::CreateHairComputeSystem(m_device);
            }

        private:
            ID3D12Device5& m_device;
        };
    } // namespace

    TEST(GpuHairDx12Tests, SimulatesAndReadsBackOnDx12ComputeBackend)
    {
        Microsoft::WRL::ComPtr<ID3D12Device5> device;
        const HRESULT createDeviceResult = D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(device.GetAddressOf()));
        if (FAILED(createDeviceResult))
        {
            GTEST_SKIP() << "No DirectX 12 device is available.";
        }

        DirectDeviceHairComputeProvider provider(*device.Get());
        Tests::SimulateAndValidateGpuHair(provider);
    }

    TEST(GpuHairDx12Tests, StandaloneFactoryCreatesComputeSystem)
    {
        const JPH::ComputeSystemResult computeSystem = JPH::CreateComputeSystemDX12();
        ASSERT_FALSE(computeSystem.HasError()) << computeSystem.GetError().c_str();
        EXPECT_TRUE(computeSystem.Get());
    }
} // namespace Jolt
