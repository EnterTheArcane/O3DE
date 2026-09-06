/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/PlatformDef.h>

#include <CommonFiles/Preprocessor.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

#include <AzFramework/API/ApplicationAPI.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/Debug/TraceContext.h>

namespace AZ
{
    namespace ShaderBuilder
    {
        void PreprocessorOptions::Reflect(ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext->Class<PreprocessorOptions>()
                    ->Version(0)
                    ->Field("predefinedMacros", &PreprocessorOptions::m_predefinedMacros)
                    ->Field("projectIncludePaths", &PreprocessorOptions::m_projectIncludePaths)
                    ;
            }
        }

        void PreprocessorOptions::RemovePredefinedMacros(const AZStd::vector<AZStd::string>& macroNames)
        {
            for (const auto& macroName : macroNames)
            {
                m_predefinedMacros.erase(
                    AZStd::remove_if(
                        m_predefinedMacros.begin(), m_predefinedMacros.end(),
                        [&](const AZStd::string& predefinedMacro) {
                            //                                       Haystack,        needle,    bCaseSensitive
                            if (!AZ::StringFunc::StartsWith(predefinedMacro, macroName, true))
                            {
                                return false;
                            }
                            // If found, let's make sure it is not just a substring.
                            if (predefinedMacro.size() == macroName.size())
                            {
                                return true;
                            }
                            // The predefinedMacro can be a string like "macro=value". If we find '=' it is a match.
                            if (predefinedMacro.c_str()[macroName.size()] == '=')
                            {
                                return true;
                            }
                            return false;
                        }),
                    m_predefinedMacros.end());
            }
        }

        AZStd::string ResolveAzslHeaderPath(const char* header)
        {
            if (!header || !*header)
            {
                return {};
            }
            AZ::IO::Path path(header);
            if (path.IsRelative())
            {
                const char* executableFolder = nullptr;
                AZ::ComponentApplicationBus::BroadcastResult(executableFolder, &AZ::ComponentApplicationBus::Events::GetExecutableFolder);
                if (!executableFolder)
                {
                    return {};
                }
                path = AZ::IO::Path(executableFolder) / path;
            }
            return path.LexicallyNormal().Native();
        }

        AZStd::vector<AZStd::string> NormalizePreprocessorArguments(const AZStd::vector<AZStd::string>& arguments)
        {
            AZStd::vector<AZStd::string> result = arguments;
            const AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
            for (size_t i = 0; i < result.size(); ++i)
            {
                AZStd::string& argument = result[i];
                if (argument == "-I" || argument == "--include")
                {
                    if (i + 1 < result.size())
                    {
                        AZStd::string& value = result[++i];
                        AZ::IO::Path path(value);
                        if (path.IsRelative())
                        {
                            value = (projectPath / path).LexicallyNormal().Native();
                        }
                    }
                }
                else if (argument.starts_with("-I") && argument.size() > 2)
                {
                    AZ::IO::Path path(argument.substr(2));
                    if (path.IsRelative())
                    {
                        argument = "-I" + (projectPath / path).LexicallyNormal().Native();
                    }
                }
            }
            return result;
        }

        AZStd::vector<AZStd::string> AppendIncludePathsToArgumentList(const AZStd::vector<AZStd::string>& preprocessorArguments, AZStd::vector<AZStd::string> includePaths)
        {
            AZStd::vector<AZStd::string> newList = preprocessorArguments;
            for (const AZStd::string& folder : includePaths)
            {
                newList.push_back(AZStd::string::format("-I%s", folder.c_str()));
            }
            return newList;
        }

        AZStd::vector<AZStd::string> BuildListOfIncludeDirectories([[maybe_unused]] const char* builderName, const char* optionalIncludeFolder)
        {
            AZ_TraceContext("Init include-paths lookup options", "preprocessor");

            AZStd::vector<AZStd::string> includePaths;

            if (optionalIncludeFolder)
            {
                if (AZ::IO::SystemFile::Exists(optionalIncludeFolder))
                {
                    includePaths.emplace_back(AZStd::move(AZ::IO::Path(optionalIncludeFolder).LexicallyNormal().Native()));
                }
            }

            auto PathCompare = [](AZ::IO::PathView searchPath)
            {
                return [searchPath](AZStd::string_view includePathView)
                {
                    return searchPath == AZ::IO::PathView(includePathView);
                };
            };

            // Add the project path to list of include paths
            AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
            if (auto it = AZStd::find_if(includePaths.begin(), includePaths.end(), PathCompare(projectPath));
                it == includePaths.end())
            {
                includePaths.emplace_back(projectPath.c_str(), projectPath.Native().size());
            }

            // get the scan folders of the projects:
            bool success = true;
            AZStd::vector<AZStd::string> scanFoldersVector;
            AzToolsFramework::AssetSystemRequestBus::BroadcastResult(success,
                &AzToolsFramework::AssetSystemRequestBus::Events::GetScanFolders,
                scanFoldersVector);
            AZ_Warning(builderName, success, "Preprocessor option: Could not acquire a list of scan folders from the database.");

            // but while we transfer to the set, we're going to keep only folders where +/ShaderLib exists
            for (AZ::IO::Path shaderScanFolder : scanFoldersVector)
            {
                shaderScanFolder /= "ShaderLib";
                if (auto it = AZStd::find_if(includePaths.begin(), includePaths.end(), PathCompare(shaderScanFolder));
                    it == includePaths.end())
                {
                    // the folders constructed this fashion constitute the base of automatic include search paths
                    if (AZ::IO::SystemFile::Exists(shaderScanFolder.c_str()))
                    {
                        includePaths.emplace_back(AZStd::move(shaderScanFolder.LexicallyNormal().Native()));
                    }
                }
            }

            // finally the <engineroot>/Gems fallback
            AZ::IO::Path engineGemsFolder(AZStd::string_view{ AZ::Utils::GetEnginePath() });
            engineGemsFolder /= "Gems";
            if (auto it = AZStd::find_if(includePaths.begin(), includePaths.end(), PathCompare(engineGemsFolder));
                it == includePaths.end())
            {
                if (AZ::IO::SystemFile::Exists(engineGemsFolder.c_str()))
                {
                    includePaths.emplace_back(AZStd::move(engineGemsFolder.Native()));
                }
            }

            return includePaths;
        }

    } // namespace ShaderBuilder
} // namespace AZ
