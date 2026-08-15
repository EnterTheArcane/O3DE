/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/PlatformHairComputeProvider.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace Jolt
{
    class GpuHairModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(GpuHairModule, "{15260E20-BDAD-49D0-8BFB-8A64231F865F}", AZ::Module);
        AZ_CLASS_ALLOCATOR(GpuHairModule, AZ::SystemAllocator);

        GpuHairModule()
        {
            if (!AZ::Interface<IHairComputeProvider>::Get())
            {
                AZ::Interface<IHairComputeProvider>::Register(&m_provider);
                m_registered = true;
            }
        }

        ~GpuHairModule() override
        {
            if (m_registered)
            {
                AZ::Interface<IHairComputeProvider>::Unregister(&m_provider);
            }
        }

    private:
        PlatformHairComputeProvider m_provider;
        bool m_registered = false;
    };
} // namespace Jolt

AZ_DECLARE_MODULE_CLASS(Gem_Jolt_GpuHair, Jolt::GpuHairModule)
