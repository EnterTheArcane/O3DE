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
        //! `export static const` values composed at link time — variant builds.
        Baked,

        //! Specialization constants ([vk::constant_id]) — root variant on capable targets.
        SpecializationConstant,

        //! Statics initialized from the variant-key fallback constant buffer read.
        DynamicFallback,
    };

    namespace SlangOptionsModuleGenerator
    {
        //! Builds the finalized ShaderOptionGroupLayout from declarations: sequential deterministic
        //! packing in declaration order from bit 0, bit-compatible with AZSLC's assignment so
        //! ShaderVariantKey, variant trees and the RPI runtime are untouched.
        AZ::Outcome<RPI::Ptr<RPI::ShaderOptionGroupLayout>, AZStd::string> BuildShaderOptionGroupLayout(
            AZStd::span<const ShaderOptionDeclaration> declarations);

        //! Generates the options module for @mode: the declarations every use-site sees as bare
        //! identifiers (booleans as `bool`, enumerations and ranges as `i32`).
        //! Baked mode declares `extern static const` symbols satisfied by a values module at link
        //! time; SpecializationConstant assigns sequential [vk::constant_id] ids; DynamicFallback
        //! reads bit-packed values from @fallbackBufferName using the layout's key packing.
        AZStd::string GenerateOptionsModule(
            ShaderOptionLoweringMode mode,
            AZStd::string_view moduleName,
            const RPI::ShaderOptionGroupLayout& layout,
            AZStd::string_view fallbackBufferName);

        //! Generates the values module composed with baked-mode programs at link time: one
        //! `export static const` per option carrying the values selected in @optionGroup
        //! (defaults fill unspecified options).
        AZStd::string GenerateBakedValuesModule(
            AZStd::string_view moduleName,
            const RPI::ShaderOptionGroup& optionGroup);
    } // namespace SlangOptionsModuleGenerator
} // namespace AZ::ShaderBuilder
