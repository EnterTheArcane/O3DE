/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/RTTI/RTTIMacros.h>

namespace AZ
{
    class ReflectContext;

    namespace ShaderBuilder
    {
        //! Object to store preprocessor options, as will be passed in the command line.
        struct PreprocessorOptions final
        {
            AZ_RTTI(PreprocessorOptions, "{684181FC-7372-49FC-B69C-1FF510A29621}");
            AZ_CLASS_ALLOCATOR(PreprocessorOptions, AZ::SystemAllocator);

            static void Reflect(AZ::ReflectContext* context);

            //! passed as -I folder1 -I folder2...
            //! folders are relative to the dev folder of the project
            AZStd::vector<AZStd::string> m_projectIncludePaths;

            //! Each string is of the type "name[=value]"
            //! passed as -Dmacro1[=value1] -Dmacro2 ... to AZSLC.
            AZStd::vector<AZStd::string> m_predefinedMacros;

            //! Removes all macros from @m_predefinedMacros that appear in @macroNames
            void RemovePredefinedMacros(const AZStd::vector<AZStd::string>& macroNames);

            //! if needed, we may add configurations like
            //!   "keep comments" or "don't predefine non-standard macros"
            //!   or "output diagnostics to std.err" or "enable digraphs/trigraphs"...
        };

        //! @param builderName: Used for debugging.
        //! @param optionalIncludeFolder: If not null, will be added at the beginning of the returned list of include folders.
        //! @returns A list of fully qualified directory paths that will be given to the c-preprocessor to find the included files in .azsl files.
        AZStd::vector<AZStd::string> BuildListOfIncludeDirectories(const char* builderName, const char* optionalIncludeFolder = nullptr);

        // @returns A new list of command arguments for the C-Preprocessor where each string in @includePaths
        //     is appended to @preprocessorArguments as "-I<path>".
        AZStd::vector<AZStd::string> AppendIncludePathsToArgumentList(const AZStd::vector<AZStd::string>& preprocessorArguments, AZStd::vector<AZStd::string> includePaths);

        //! Resolve configured -I/--include paths relative to the project without changing argument boundaries.
        AZStd::vector<AZStd::string> NormalizePreprocessorArguments(const AZStd::vector<AZStd::string>& arguments);
        //! Resolve an RHI header from the executable directory, matching platform-header staging.
        AZStd::string ResolveAzslHeaderPath(const char* header);

    } // namespace ShaderBuilder
} // AZ
