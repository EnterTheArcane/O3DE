/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Module/Module.h>

namespace AZ
{
    namespace ShaderBuilder
    {
        class ShaderBuilderModule final
            : public AZ::Module
        {
        public:
            AZ_RTTI(ShaderBuilderModule, "{18F6276E-09B7-4C1D-B658-FADA2DB22148}", AZ::Module);

            ShaderBuilderModule() = default;
            ~ShaderBuilderModule() override = default;

            AZ::ComponentTypeList GetRequiredSystemComponents() const override
            {
                return AZ::ComponentTypeList();
            }
        };
    }
}

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Builders), AZ::ShaderBuilder::ShaderBuilderModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_AtomShader_Builders, AZ::ShaderBuilder::ShaderBuilderModule)
#endif
