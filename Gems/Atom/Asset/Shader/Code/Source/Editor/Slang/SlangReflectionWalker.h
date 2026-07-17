/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Editor/ShaderReflectionData.h>
#include <Atom/RHI.Edit/ShaderTargetDescriptor.h>

#include <slang.h>

namespace AZ::ShaderBuilder::SlangReflectionWalker
{
    //! Walks the reflection of a linked Slang program into the language-neutral reflection
    //! contract: ParameterBlock instances become ShaderResourceGroups (loose members land in the
    //! implicit SRG-constants buffer), [AtomShaderResourceGroupMember]-tagged globals are grouped
    //! into their logical group (Bindless), static samplers are rebuilt from [AtomStaticSampler]
    //! attribute values, and per-entry stage interfaces come from entry-point reflection.
    //!
    //! Stage masks are conservative scaffolding for now: every resource lists every entry point
    //! as a dependent function. Exact per-entry masks (IMetadata) land with the parity milestone.
    //!
    //! The caller must hold the compiler lock and keep the linked program alive for the duration.
    AZ::Outcome<ShaderReflectionData, AZStd::string> BuildReflectionData(
        slang::ShaderReflection* programLayout,
        RHI::ShaderTargetFormat targetFormat,
        const MapOfStringToStageType& shaderEntryPoints);
} // namespace AZ::ShaderBuilder::SlangReflectionWalker
