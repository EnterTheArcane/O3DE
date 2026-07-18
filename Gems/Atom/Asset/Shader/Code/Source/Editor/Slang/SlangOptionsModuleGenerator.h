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
        Name m_name;
        RPI::ShaderOptionType m_type = RPI::ShaderOptionType::Boolean;

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
        //! designated ShaderResourceGroup constant.
        DynamicFallback,
    };

    namespace SlangOptionsModuleGenerator
    {
        //! Name prefix of the per-option accessor functions the ATOM_OPTION macros declare
        //! `extern` and the generated implementation modules `export`. The accessor ABI is always
        //! `int <AccessorNamePrefix><option name>()` — the authored property casts the result back
        //! to the authored type, so implementation modules never need visibility into
        //! author-declared enum types.
        static constexpr AZStd::string_view AccessorNamePrefix = "AtomOptionImpl_";

        //! Name of the exported getter the ATOM_VARIANT_FALLBACK macro generates in the authored
        //! module, and that DynamicFallback implementation modules declare `extern`.
        static constexpr AZStd::string_view FallbackKeyGetterName = "Atom_GetShaderVariantKeyFallback";

        //! The shader-options authoring macros, injected as preamble text into every .slang
        //! module a compile session loads (module imports cannot propagate preprocessor
        //! definitions). The [AtomOption]/[AtomOptionRange]/[AtomVariantFallback] attributes they
        //! reference come from the force-included Atom.RPI.Prelude import.
        AZStd::span<const AZStd::string_view> GetAuthoringMacroLines();

        //! Everything the ATOM_OPTION discovery walk finds across a session's loaded modules.
        struct DiscoveredShaderOptions
        {
            //! In module-load order, then declaration order inside each module — the packing order.
            AZStd::vector<ShaderOptionDeclaration> m_declarations;

            //! ShaderVariantKey fallback designation from ATOM_VARIANT_FALLBACK; both empty when
            //! no fallback is designated.
            AZStd::string m_fallbackShaderResourceGroupName;
            AZStd::string m_fallbackMemberName;
        };

        //! Walks the declaration reflection of every module loaded in @session for
        //! [AtomOption]-attributed accessor functions and the [AtomVariantFallback] getter.
        //! Enumeration option values resolve against enum declarations found in the same walk.
        AZ::Outcome<DiscoveredShaderOptions, AZStd::string> DiscoverShaderOptions(slang::ISession* session);

        //! Builds the finalized ShaderOptionGroupLayout from declarations: sequential deterministic
        //! packing in declaration order from bit 0, bit-compatible with AZSLC's assignment so
        //! ShaderVariantKey, variant trees and the RPI runtime are untouched.
        AZ::Outcome<RPI::Ptr<RPI::ShaderOptionGroupLayout>, AZStd::string> BuildShaderOptionGroupLayout(
            AZStd::span<const ShaderOptionDeclaration> declarations);

        //! Generates the accessor implementation module composed with the program at link time:
        //! one `export int AtomOptionImpl_<name>()` body per option.
        //! SpecializationConstant assigns sequential [vk::constant_id] ids initialized to the
        //! defaults; DynamicFallback extracts each option's bits from the 128-bit key returned by
        //! the extern fallback getter. Baked values come from GenerateBakedValuesModule instead.
        AZStd::string GenerateImplementationModule(
            ShaderOptionLoweringMode mode,
            AZStd::string_view moduleName,
            const RPI::ShaderOptionGroupLayout& layout);

        //! Generates the accessor implementation module for Baked lowering: each accessor returns
        //! the link-time constant value selected in @optionGroup (defaults fill unspecified
        //! options), so codegen specializes per variant.
        AZStd::string GenerateBakedValuesModule(
            AZStd::string_view moduleName,
            const RPI::ShaderOptionGroup& optionGroup);
    } // namespace SlangOptionsModuleGenerator
} // namespace AZ::ShaderBuilder
