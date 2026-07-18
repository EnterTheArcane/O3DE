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

#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroupLayout.h>

#include <slang.h>

namespace AZ::ShaderBuilder
{
    //! One shader option as authored. The builder owns packing: key offsets and sizes are
    //! produced deterministically from the declaration order, never authored.
    struct ShaderOptionDeclaration
    {
        //! The runtime name: what the ShaderOptionGroupLayout, .materialtype references and
        //! variant lists use. The interface method name, unless [AtomOptionAlias] overrides it.
        Name m_name;

        //! The interface requirement's method name — the name generated implementations define.
        AZStd::string m_methodName;

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
    //! the builder generates a different implementation struct per mode (feasibility gate 2).
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
        //! One [AtomOptions] extern struct and the interface it conforms to: the anchor a
        //! generated implementation module exports a struct for.
        struct DiscoveredOptionsProvider
        {
            AZStd::string m_structName;
            AZStd::string m_interfaceName;

            //! Name of the module declaring the struct; the generated module imports it.
            AZStd::string m_declaringModuleName;

            //! Indices into DiscoveredShaderOptions::m_declarations of this provider's options.
            AZStd::vector<size_t> m_declarationIndices;
        };

        //! Everything the options discovery walk finds across a session's loaded modules.
        struct DiscoveredShaderOptions
        {
            //! In module-load order, then declaration order inside each interface — the packing order.
            AZStd::vector<ShaderOptionDeclaration> m_declarations;

            AZStd::vector<DiscoveredOptionsProvider> m_providers;

            //! ShaderVariantKey fallback designation from the [AtomVariantFallback] member
            //! attribute: the ParameterBlock variable name (the runtime ShaderResourceGroup name)
            //! and the uint4 member holding the key. Both empty when no fallback is designated.
            AZStd::string m_fallbackShaderResourceGroupName;
            AZStd::string m_fallbackMemberName;

            //! Name of the module declaring the fallback ParameterBlock; the generated dynamic
            //! implementation imports it to read the member.
            AZStd::string m_fallbackDeclaringModuleName;
        };

        //! Walks every module loaded in @session for [AtomOptions] extern structs, resolves the
        //! interface each conforms to, and reads the option set off the interface requirements:
        //! name and type from the requirement itself, default from [AtomOption], integer range
        //! from [AtomOptionRange], enumeration values from the return type's enum declaration.
        //! Also finds the [AtomVariantFallback] ShaderResourceGroup member.
        AZ::Outcome<DiscoveredShaderOptions, AZStd::string> DiscoverShaderOptions(slang::ISession* session);

        //! Builds the finalized ShaderOptionGroupLayout from declarations: sequential deterministic
        //! packing in declaration order from bit 0, bit-compatible with AZSLC's assignment so
        //! ShaderVariantKey, variant trees and the RPI runtime are untouched.
        AZ::Outcome<RPI::Ptr<RPI::ShaderOptionGroupLayout>, AZStd::string> BuildShaderOptionGroupLayout(
            AZStd::span<const ShaderOptionDeclaration> declarations);

        //! Generates the implementation module composed with the program at link time: one
        //! `export struct <provider> : <interface>` per discovered provider, each static method
        //! returning its option's value for the mode. SpecializationConstant assigns sequential
        //! [vk::constant_id] ids initialized to the defaults; DynamicFallback extracts each
        //! option's bits from the designated fallback member, read directly through an import of
        //! the declaring module. Baked values come from GenerateBakedValuesModule instead.
        AZStd::string GenerateImplementationModule(
            ShaderOptionLoweringMode mode,
            AZStd::string_view moduleName,
            const DiscoveredShaderOptions& discovered,
            const RPI::ShaderOptionGroupLayout& layout);

        //! Generates the implementation module for Baked lowering: each accessor returns the
        //! link-time constant value selected in @optionGroup (defaults fill unspecified options),
        //! so codegen specializes per variant.
        AZStd::string GenerateBakedValuesModule(
            AZStd::string_view moduleName,
            const DiscoveredShaderOptions& discovered,
            const RPI::ShaderOptionGroup& optionGroup);
    } // namespace SlangOptionsModuleGenerator
} // namespace AZ::ShaderBuilder
