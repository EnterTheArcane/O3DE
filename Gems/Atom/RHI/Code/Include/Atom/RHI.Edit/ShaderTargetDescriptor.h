/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RHI.Edit/ShaderHardwareStage.h>

namespace AZ::RHI
{
    //! Target intermediate representation an RHI's shader pipeline consumes.
    enum class ShaderTargetFormat : uint32_t
    {
        //! No generalized target declared: the RHI participates only in the legacy AZSL
        //! compilation path (ShaderPlatformInterface::CompilePlatformInternal).
        None = 0,
        Dxil, //!< DirectX intermediate language (D3D12).
        Spirv, //!< SPIR-V (Vulkan).
        MetalShadingLanguage, //!< MSL source, consumed by an external Metal toolchain.
        MetalLibrary, //!< Compiled metallib.
        Wgsl, //!< WebGPU shading language.
        //! The RHI consumes reflection but no bytecode (the Null RHI): its shader assets carry
        //! real ShaderResourceGroup layouts, options and pipeline layout with empty stage
        //! functions. Distinct from None, which means "declares no target at all" and stays a
        //! hard error for a modern language backend — a reflection-only target is one every
        //! backend can serve, by running its frontend and emitting no code.
        ReflectionOnly,
    };

    //! Language-neutral description of the compilation target a ShaderPlatformInterface consumes.
    //!
    //! Immutable facts about the target live here, produced by
    //! ShaderPlatformInterface::GetShaderTargetDescriptor() — putting them in user-editable
    //! settings would permit nonsense like "Metal produces DXIL". Tunables (extra compiler
    //! arguments) flow through ShaderBuildArguments instead.
    //!
    //! Shader language backends consume this descriptor and map it onto their own compiler
    //! options; whether a given backend can produce a given target is the backend's answer
    //! (IShaderCompilerBackend::CanCompileTarget), never the RHI's.
    struct ShaderTargetDescriptor
    {
        //! Code generation conventions the produced bytecode must follow so it behaves
        //! identically to the bytecode the legacy AZSL pipeline produces for this RHI.
        struct Conventions
        {
            //! Negate the Y component of vertex-stage position outputs
            //! (Vulkan clip-space fixup; today's -fvk-invert-y).
            bool m_invertY = false;

            //! Use DirectX semantics for the fragment-stage position W component
            //! (today's -fvk-use-dx-position-w).
            bool m_useDxPositionW = false;

            //! Lay out constant buffers with DirectX packing rules (today's -fvk-use-dx-layout).
            bool m_useDxMemoryLayout = true;

            //! Assign binding indices that are unique across all resource types within one
            //! descriptor set (today's AZSLC --unique-idx).
            bool m_uniqueBindingIndicesPerSet = false;

            //! Enable 16-bit scalar types in generated code (today's -enable-16bit-types).
            bool m_enable16BitTypes = false;
        };

        //! An RHI without a generalized target participates only in the legacy AZSL path.
        bool IsValid() const
        {
            return m_format != ShaderTargetFormat::None;
        }

        //! Returns the compilation profile for @stage, or an empty view when the stage is
        //! not supported by this target.
        AZStd::string_view GetStageProfile(ShaderHardwareStage stage) const;

        //! Hash covering every field. Feeds build fingerprints of cached compiler products so
        //! a target change invalidates them.
        HashValue64 GetHash() const;

        ShaderTargetFormat m_format = ShaderTargetFormat::None;

        //! Compilation profile per hardware stage. A single profile string cannot describe
        //! existing RHIs: DX12 and Vulkan compile raster/compute stages as shader model 6.2
        //! but ray tracing as lib_6_3.
        AZStd::unordered_map<ShaderHardwareStage, AZStd::string> m_stageProfiles;

        Conventions m_conventions;

        //! Maximum bytes of root/push constants the pipeline layout supports
        //! (today's AZSLC --root-const value).
        uint32_t m_rootConstantCapacityInBytes = 0;

        //! Minimum alignment, in bytes, of SRG constant-buffer data
        //! (today's AZ_TRAIT_CONSTANT_BUFFER_ALIGNMENT prelude macro).
        uint32_t m_constantBufferAlignmentInBytes = 16;

        //! Ordered names of the post-compile bytecode steps this RHI applies to generated stage
        //! bytecode (e.g. DX12 specialization-constant patching, Metal spirv-cross translation).
        //! Informational until the language-neutral post-process hook lands.
        AZStd::vector<AZStd::string> m_postProcessSteps;
    };
}
