/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI.Reflect/Handle.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <CommonFiles/CommonTypes.h>

namespace AZ
{
    namespace ShaderBuilder
    {
        using ConstantBufferContainer = AZStd::vector<ConstantBufferData>;
        using StructContainer = AZStd::vector<StructData>;
        using SamplerContainer = AZStd::vector<SamplerSrgData>;
        using TextureContainer = AZStd::vector<TextureSrgData>;
        using BufferContainer = AZStd::vector<BufferSrgData>;
        using SrgConstantContainer = AZStd::vector<SrgConstantData>;

        struct SrgData
        {
            //friend bool operator == (const SrgData&, const SrgData&);

            AZStd::string m_name;
            AZStd::string m_containingFileName;

            // One SRG contains the ShaderVariantKey fallback structure
            // A size greater than 0 indicates that this SRG data is designated as fallback
            Name                    m_fallbackName;
            uint32_t                m_fallbackSize = 0;

            RHI::Handle<uint32_t>   m_bindingSlot;

            ConstantBufferContainer m_constantBuffers;
            SamplerContainer        m_samplers;
            StructContainer         m_structs;
            TextureContainer        m_textures;
            BufferContainer         m_buffers;
            SrgConstantContainer    m_srgConstantData;
            uint32_t                m_srgConstantDataRegisterId = RHI::UndefinedRegisterSlot;
            uint32_t                m_srgConstantDataSpaceId = RHI::UndefinedRegisterSlot;
        };

        struct FunctionData
        {
            AZStd::string m_returnType;
            AZStd::string m_name;
            AZStd::string m_parametersAndContents; // Everything else is lumped together

            // Indicates if the function declares any shader stage inputs or outputs by the use of semantics
            bool m_hasShaderStageVaryings = false;
            RHI::ShaderStageAttributeMap attributesList;
        };

        typedef AZStd::vector<SrgData> SrgDataContainer;
        typedef AZStd::vector<FunctionData> AzslFunctions;

        struct RootConstantData
        {
            RootConstantBinding     m_bindingInfo;
            SrgConstantContainer    m_constants;
        };

        //! Reflection data produced by compiling an AZSL source file.
        struct AzslData
        {
            explicit AzslData(const AZStd::string& sourceFullPath) : m_sourceFullPath(sourceFullPath) { }

            AZStd::string m_sourceFullPath;
            SrgDataContainer m_srgData;
            AzslFunctions m_functions;
        };
    } // ShaderBuilder
} // AZ
