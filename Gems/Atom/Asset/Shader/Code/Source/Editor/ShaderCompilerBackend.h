/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RHI.Edit/ShaderBuildArguments.h>
#include <Atom/RHI.Edit/ShaderHardwareStage.h>
#include <Atom/RHI.Edit/ShaderPlatformInterface.h>
#include <Atom/RHI.Edit/ShaderTargetDescriptor.h>

#include <Editor/ShaderReflectionData.h>

namespace AssetBuilderSDK
{
    struct PlatformInfo;
}

namespace AZ::ShaderBuilder
{
    //! Everything a shader language backend needs to run its frontend once — one
    //! (RHI API, supervariant) combination of one shader.
    struct FrontendInput
    {
        AZStd::string_view m_builderName;

        //! Full path of the language source the .shader descriptor points at (.azsl, .slang, ...).
        AZStd::string_view m_shaderSourceFullPath;

        //! Output stem: <shader file name>[-<supervariant name>].
        AZStd::string_view m_stemName;

        AZStd::string_view m_tempDirPath;

        //! Project/gem include directories for source-level includes and module imports.
        AZStd::span<const AZStd::string> m_includePaths;

        //! Fully merged build arguments for the current (platform, API, .shader, supervariant) scope.
        const RHI::ShaderBuildArguments* m_buildArguments = nullptr;

        //! Entry points declared by the .shader descriptor, so the backend can reflect
        //! per-entry data (stage interfaces, attributes) during the frontend run.
        const MapOfStringToStageType* m_entryPoints = nullptr;

        RHI::ShaderPlatformInterface* m_shaderPlatformInterface = nullptr;
        const AssetBuilderSDK::PlatformInfo* m_platformInfo = nullptr;
    };

    //! One cached build artifact of a frontend run that the builder registers as a job product so
    //! downstream builders can fetch it by sub-id.
    struct FrontendSubProduct
    {
        AZStd::string m_path;

        //! Sub-product type composed into the product sub-id via RPI::ShaderAsset::MakeProductAssetSubId.
        uint32_t m_subProductType = 0;
    };

    //! Everything one frontend run produces: the language-neutral reflection contract plus the
    //! backend's cached artifacts.
    struct FrontendResult
    {
        AZStd::vector<FrontendSubProduct> m_subProducts;

        ShaderReflectionData m_reflection;

        //! The target-language source the frontend produced (HLSL today), consumed by stage compiles.
        AZStd::string m_targetSourcePath;
        AZStd::string m_targetSourceCode;
    };

    //! One entry point of one variant, ready for target code generation. The source path already
    //! has any variant option defines applied by the caller.
    struct StageInput
    {
        AZStd::string_view m_builderName;
        const AssetBuilderSDK::PlatformInfo* m_platformInfo = nullptr;
        RHI::ShaderPlatformInterface* m_shaderPlatformInterface = nullptr;

        AZStd::string_view m_sourcePath;
        AZStd::string_view m_entryPointName;
        RHI::ShaderHardwareStage m_stage = RHI::ShaderHardwareStage::Invalid;

        //! Module/include search roots for backends that recompile from source at stage time.
        AZStd::span<const AZStd::string> m_includePaths;

        AZStd::string_view m_tempDirPath;
        const RHI::ShaderBuildArguments* m_buildArguments = nullptr;
        bool m_useSpecializationConstants = false;
    };

    struct StageResult
    {
        RHI::ShaderPlatformInterface::StageDescriptor m_descriptor;
    };

    //! The language-specific middle of the shader build: source in, reflection and bytecode out.
    //! Everything around it — the supervariant loop, build-argument scoping, job products,
    //! render states and asset creation — is language-independent and lives in the builders.
    //! A new shader language ships as a new implementation broadcast on ShaderCompilerBackendBus
    //! by its own system component; the builders and the RHI need no edits.
    class IShaderCompilerBackend
    {
    public:
        virtual ~IShaderCompilerBackend() = default;

        virtual AZStd::string_view GetName() const = 0;

        //! Source file extensions this backend claims, each including the dot, e.g. ".azsl".
        //! The extensions are the sole dispatch key: the shader builder routes each .shader by the
        //! extension of its Source reference, and no two registered backends may claim the same one.
        virtual AZStd::span<const AZStd::string_view> GetSourceExtensions() const = 0;

        //! Capability negotiation lives on the language side (never on the RHI): can this
        //! backend produce the RHI's declared target? Gives no-target/unsupported combinations
        //! a clean skip/error path.
        virtual bool CanCompileTarget(const RHI::ShaderTargetDescriptor& targetDescriptor) const = 0;

        //! Appends the full paths of every source file @shaderSourceFullPath transitively
        //! depends on, plus the nonexistent candidate paths whose future appearance must
        //! retrigger the build (shadowing/missing includes and modules). CreateJobs registers
        //! them as source dependencies — the Asset Processor's only source-dependency channel —
        //! so each language scans with its own reference rules.
        virtual void EnumerateSourceDependencies(
            AZStd::string_view shaderSourceFullPath,
            AZStd::span<const AZStd::string> includePaths,
            AZStd::unordered_set<AZStd::string>& sourceDependencies) const = 0;

        //! Runs the frontend once per (API, supervariant): preprocess/parse/reflect.
        virtual AZ::Outcome<FrontendResult, AZStd::string> CompileFrontend(const FrontendInput& input) = 0;

        //! Compiles one entry point of one variant to target bytecode.
        virtual AZ::Outcome<StageResult, AZStd::string> CompileStage(const StageInput& input) = 0;
    };
} // namespace AZ::ShaderBuilder
