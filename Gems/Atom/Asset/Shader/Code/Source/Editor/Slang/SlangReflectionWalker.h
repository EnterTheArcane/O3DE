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
    //! Per-resource dependent functions are exact: each entry point's IMetadata is queried for
    //! whether the resource's binding location is used, so downstream stage masks match what the
    //! generated code actually references. When a metadata query is unavailable the walker falls
    //! back to listing every entry point (conservative, safe, but not parity-grade).
    //!
    //! The caller must hold the compiler lock and keep the linked program alive for the duration.
    //! @param entryPointNamesInOrder Entry names in composition order (the entry-point index
    //!        order of the linked program), e.g. ProgramCompilation::m_entryPointNames.
    AZ::Outcome<ShaderReflectionData, AZStd::string> BuildReflectionData(
        slang::IComponentType* linkedProgram,
        RHI::ShaderTargetFormat targetFormat,
        AZStd::span<const AZStd::string> entryPointNamesInOrder);
} // namespace AZ::ShaderBuilder::SlangReflectionWalker
