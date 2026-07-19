/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RHI.Edit/ShaderTargetDescriptor.h>

#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroupLayout.h>

#include <slang.h>

namespace AZ::ShaderBuilder
{
    //! One shader option as authored. The builder owns packing: key offsets and sizes are
    //! produced deterministically from the declaration order, never authored.
    struct ShaderOptionDeclaration
    {
        //! The option function's name, which is also the runtime name: what the
        //! ShaderOptionGroupLayout, .materialtype references and variant lists use.
        Name m_name;

        RPI::ShaderOptionType m_type = RPI::ShaderOptionType::Boolean;

        //! The option's Slang type spelling ("bool", "int", or the enum's name), used in
        //! generated implementation signatures.
        AZStd::string m_typeText;

        //! Enumeration values in declaration order (Enumeration type only).
        AZStd::vector<Name> m_enumValues;

        //! Inclusive range (IntegerRange type only).
        int32_t m_minValue = 0;
        int32_t m_maxValue = 0;

        //! Default value name; empty selects the first value.
        Name m_defaultValue;
    };

    //! The three lives of one authored option (spec D6). Use-sites are identical in every mode;
    //! the builder generates a different implementation module per mode (feasibility gate 2).
    enum class ShaderOptionLoweringMode
    {
        //! Accessors return link-time constant values — variant builds.
        Baked,

        //! Accessors return specialization constants ([vk::constant_id]) — root variant on
        //! capable targets.
        SpecializationConstant,

        //! Accessors extract their bits from the ShaderVariantKey fallback read out of the
        //! designated ShaderResourceGroup member.
        DynamicFallback,
    };

    namespace SlangOptionsModuleGenerator
    {
        //! Everything the options discovery walk finds across a session's loaded modules.
        struct DiscoveredShaderOptions
        {
            //! In module-load order, then declaration order inside each module — the packing order.
            AZStd::vector<ShaderOptionDeclaration> m_declarations;

            //! Names of the modules declaring option functions, unique, in module-load order; the
            //! generated implementation module imports each (for the extern declarations and any
            //! enum option types).
            AZStd::vector<AZStd::string> m_declaringModuleNames;

            //! ShaderVariantKey fallback designation from the [AtomVariantFallback] member
            //! attribute: the ParameterBlock variable name (the runtime ShaderResourceGroup name)
            //! and the uint4 member holding the key. Both empty when no fallback is designated.
            AZStd::string m_fallbackShaderResourceGroupName;
            AZStd::string m_fallbackMemberName;

            //! Name of the module declaring the fallback ParameterBlock; the generated dynamic
            //! implementation imports it to read the member.
            AZStd::string m_fallbackDeclaringModuleName;
        };

        //! Walks every module loaded in @session for [AtomOption]-attributed extern functions and
        //! reads the option set off them: name and type from the function itself, default from
        //! the attribute argument (0 when omitted), integer range from [AtomRange], enumeration
        //! values from the return type's enum declaration. Also finds the [AtomVariantFallback]
        //! ShaderResourceGroup member.
        AZ::Outcome<DiscoveredShaderOptions, AZStd::string> DiscoverShaderOptions(slang::ISession* session);

        //! Builds the finalized ShaderOptionGroupLayout from declarations: sequential deterministic
        //! packing in declaration order from bit 0, bit-compatible with AZSLC's assignment so
        //! ShaderVariantKey, variant trees and the RPI runtime are untouched.
        AZ::Outcome<RPI::Ptr<RPI::ShaderOptionGroupLayout>, AZStd::string> BuildShaderOptionGroupLayout(
            AZStd::span<const ShaderOptionDeclaration> declarations);

        //! Generates the implementation module composed with the program at link time: one
        //! `export` function per discovered option, returning its option's value for the mode.
        //! DynamicFallback extracts each option's bits from the designated fallback member, read
        //! directly through an import of the declaring module. SpecializationConstant is
        //! target-specific: Spirv uses native
        //! [vk::constant_id] constants with sequential ids; Dxil routes each specialization id
        //! through the compiler service's HLSL-prelude volatile read so the id survives into the
        //! DXIL as the patchable dword dxsc.exe expects (PostProcessStage runs the patch).
        //! Baked values come from GenerateBakedValuesModule instead.
        AZStd::string GenerateImplementationModule(
            ShaderOptionLoweringMode mode,
            RHI::ShaderTargetFormat targetFormat,
            AZStd::string_view moduleName,
            const DiscoveredShaderOptions& discovered,
            const RPI::ShaderOptionGroupLayout& layout);

        //! Generates the implementation module for Baked lowering — one variant's option values.
        //! Each option the variant pins returns its link-time constant value, so codegen
        //! specializes; options the variant leaves unpinned keep the dynamic fallback read
        //! (AZSL variant semantics: unpinned options stay runtime-switchable), which requires
        //! the discovered [AtomVariantFallback] designation — callers must reject partially
        //! specified variants of shaders without one.
        AZStd::string GenerateBakedValuesModule(
            AZStd::string_view moduleName,
            const DiscoveredShaderOptions& discovered,
            const RPI::ShaderOptionGroup& optionGroup);
    } // namespace SlangOptionsModuleGenerator
} // namespace AZ::ShaderBuilder
