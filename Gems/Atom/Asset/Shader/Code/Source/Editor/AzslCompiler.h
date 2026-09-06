/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Reflect/Base.h>
#include <AzCore/Outcome/Outcome.h>
#include "AzslData.h"
#include "ShaderBuilderUtility.h"

namespace AZ
{
    namespace RPI
    {
        class ShaderOptionGroupLayout;
    }

    namespace RHI
    {
        class ShaderPlatformInterface;
    }

    namespace ShaderBuilder
    {
        class AzslCompiler
        {
        public:
            //! AzslCompiler constructor
            //! @param inputFilePath      The original AZSL source file to compile. AZSLC performs preprocessing.
            AzslCompiler(const AZStd::string& inputFilePath, const AZStd::string& tempFolder);

            //! Valid partial reports return known paths so missing headers can trigger rediscovery.
            Outcome<AZStd::vector<AZStd::string>, AZStd::string> DiscoverDependencies(
                const AZStd::vector<AZStd::string>& arguments) const;

            static Outcome<AZStd::vector<AZStd::string>, AZStd::string> ParseDependencyReport(
                AZStd::string_view json, AZ::u32 exitCode);

            //! compile with --full and generate all .json files
            //! @param outputFile="" will use the input as base path.
            Outcome<ShaderBuilderUtility::AzslSubProducts::Paths> EmitFullData(
                const AZStd::vector<AZStd::string>& azslcArguments,
                const AZStd::string& outputFile = "") const;

            //! make sense of a --ia json document and fill up the function data
            bool ParseIaPopulateFunctionData(const rapidjson::Document& input, AzslFunctions& functionData) const;
            //! make sense of a --ia json document and fill up the struct data
            bool ParseIaPopulateStructData(const rapidjson::Document& input, const AZStd::string& vertexEntryName, StructData& outStructData) const;
            //! make sense of a --om json document and fill up the struct data
            bool ParseOmPopulateStructData(const rapidjson::Document& input, const AZStd::string& fragmentShaderName, StructData& outStructData) const;
            //! make sense of a --srg json document and fill up the srg data container
            bool ParseSrgPopulateSrgData(const rapidjson::Document& input, SrgDataContainer& outSrgData) const;
            //! make sense of a --option json document and fill up the shader option group layout
            bool ParseOptionsPopulateOptionGroupLayout(const rapidjson::Document& input, RPI::Ptr<RPI::ShaderOptionGroupLayout>& shaderOptionGroupLayout, bool& outSpecializationConstants) const;
            //! make sense of a --bindingdep json documment and fill up the binding dependencies object
            bool ParseBindingdepPopulateBindingDependencies(const rapidjson::Document& input, BindingDependencies& bindingDependencies) const;
            //! make sense of a --srg json document and fill up the root constant data
            bool ParseSrgPopulateRootConstantData(const rapidjson::Document& input, RootConstantData& rootConstantData) const;

        protected:
            bool Compile(
                const AZStd::vector<AZStd::string>& compilerParams,
                const AZStd::string& outputFilePath) const;

            AZStd::string m_inputFilePath;
            AZStd::string m_tempFolder;
        };
    }
}
