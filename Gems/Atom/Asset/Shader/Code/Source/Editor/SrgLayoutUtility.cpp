/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include "SrgLayoutUtility.h"

#include <Editor/Azslc/AzslcReflectionAdapter.h>
#include <Editor/ShaderReflectionData.h>

namespace AZ
{
    namespace ShaderBuilder
    {
        namespace SrgLayoutUtility
        {
            bool LoadShaderResourceGroupLayouts(
                const char* builderName,
                const SrgDataContainer& resourceGroups,
                RPI::ShaderResourceGroupLayoutList& srgLayoutList)
            {
                // The AZSLC parse output converts to the language-neutral reflection contract, and the
                // layouts are built from it by the same shared converter every language backend uses
                ShaderReflectionData reflectionData;
                if (!AzslcReflectionAdapter::ConvertSrgDataToReflection(builderName, resourceGroups, reflectionData.m_shaderResourceGroups))
                {
                    return false;
                }

                auto layoutsOutcome = BuildShaderResourceGroupLayouts(reflectionData);
                if (!layoutsOutcome.IsSuccess())
                {
                    AZ_Error(builderName, false, "%s", layoutsOutcome.GetError().c_str());
                    return false;
                }

                for (RHI::Ptr<RHI::ShaderResourceGroupLayout>& srgLayout : layoutsOutcome.GetValue())
                {
                    srgLayoutList.push_back(AZStd::move(srgLayout));
                }

                return true;
            }
        }  // namespace SrgLayoutUtility
    } // namespace ShaderBuilder
} // AZ
