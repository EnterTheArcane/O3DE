/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzslData.h>
#include <CommonFiles/CommonTypes.h>
#include <Editor/ShaderReflectionData.h>

namespace AZ::ShaderBuilder::AzslcReflectionAdapter
{
    //! Converts the AZSLC SRG parse output to language-neutral SRG reflection entries, preserving
    //! the hash-relevant layout-construction order within each descriptor category.
    //! Returns false when a resource has a type the RHI vocabulary cannot express.
    bool ConvertSrgDataToReflection(
        AZStd::string_view builderName,
        const SrgDataContainer& srgDataContainer,
        AZStd::vector<ShaderResourceGroupReflection>& srgReflections);

    //! Converts the AZSLC binding-dependency dataset to per-SRG binding reflections keyed by SRG name.
    void ConvertBindingDependenciesToReflection(
        const BindingDependencies& bindingDependencies,
        AZStd::unordered_map<AZStd::string, ShaderResourceGroupBindingReflection>& srgBindings);

    RootConstantsReflection ConvertRootConstantDataToReflection(const RootConstantData& rootConstantData);

    AZStd::vector<ShaderFunctionReflection> ConvertFunctionsToReflection(const AzslFunctions& functions);

    AZStd::vector<RPI::ShaderOptionDescriptor> ConvertShaderOptionsToReflection(const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout);

    //! Parses the AZSLC ia/om reflection JSON files for the given entry points and fills the
    //! per-entry stage interfaces of @reflectionData.
    bool PopulateStageInterfacesFromJsonFiles(
        AZStd::string_view builderName,
        const AZStd::string& preprocessedSourcePath,
        const AZStd::string& tempDirPath,
        const AZStd::string& iaJsonPath,
        const AZStd::string& omJsonPath,
        const MapOfStringToStageType& shaderEntryPoints,
        ShaderReflectionData& reflectionData);

    //! Assembles the complete language-neutral reflection contract from the legacy structures one
    //! AZSLC frontend run produced.
    AZ::Outcome<ShaderReflectionData, AZStd::string> BuildReflectionData(
        AZStd::string_view builderName,
        const AzslData& azslData,
        const RPI::ShaderOptionGroupLayout& shaderOptionGroupLayout,
        bool usesSpecializationConstants,
        const BindingDependencies& bindingDependencies,
        const RootConstantData& rootConstantData,
        const AZStd::string& iaJsonPath,
        const AZStd::string& omJsonPath,
        const MapOfStringToStageType& shaderEntryPoints,
        const AZStd::string& tempDirPath);
} // namespace AZ::ShaderBuilder::AzslcReflectionAdapter
