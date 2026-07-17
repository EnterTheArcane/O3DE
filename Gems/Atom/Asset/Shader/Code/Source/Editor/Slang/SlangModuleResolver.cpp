/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangModuleResolver.h"

#include <AzCore/IO/SystemFile.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/std/string/regex.h>

namespace AZ::ShaderBuilder
{
    SlangModuleResolver::SlangModuleResolver(AZStd::vector<AZStd::string> orderedSearchRoots)
        : m_searchRoots(AZStd::move(orderedSearchRoots))
    {
    }

    void SlangModuleResolver::SetSearchRoots(AZStd::vector<AZStd::string> orderedSearchRoots)
    {
        m_searchRoots = AZStd::move(orderedSearchRoots);
    }

    AZStd::span<const AZStd::string> SlangModuleResolver::GetSearchRoots() const
    {
        return m_searchRoots;
    }

    AZStd::vector<AZStd::string> SlangModuleResolver::GetRelativePathCandidates(AZStd::string_view moduleReference)
    {
        AZStd::vector<AZStd::string> candidates;
        if (moduleReference.empty())
        {
            return candidates;
        }

        // String-form reference: a verbatim (relative) file path, extension included.
        const bool isStringForm =
            moduleReference.contains('/')
            || moduleReference.contains('\\')
            || moduleReference.ends_with(".slang");
        if (isStringForm)
        {
            candidates.emplace_back(moduleReference);
            return candidates;
        }

        // Dotted module name: dots become directory separators.
        AZStd::string basePath(moduleReference);
        AZ::StringFunc::Replace(basePath, '.', '/');

        candidates.push_back(basePath + ".slang");

        // Slang also matches '_' in module names against '-' in filenames.
        if (basePath.contains('_'))
        {
            AZStd::string dashedPath = basePath;
            AZ::StringFunc::Replace(dashedPath, '_', '-');
            candidates.push_back(dashedPath + ".slang");
        }

        return candidates;
    }

    SlangModuleResolver::Resolution SlangModuleResolver::ResolveModule(
        AZStd::string_view moduleReference,
        AZStd::string_view importingFilePath) const
    {
        Resolution resolution;

        const AZStd::vector<AZStd::string> relativeCandidates = GetRelativePathCandidates(moduleReference);
        if (relativeCandidates.empty())
        {
            return resolution;
        }

        // Search locations in priority order: the importing file's directory, then the roots.
        AZStd::vector<AZStd::string> searchLocations;
        searchLocations.reserve(m_searchRoots.size() + 1);
        if (!importingFilePath.empty())
        {
            AZStd::string importingDirectory;
            if (AZ::StringFunc::Path::GetFullPath(importingFilePath.data(), importingDirectory))
            {
                searchLocations.push_back(AZStd::move(importingDirectory));
            }
        }
        for (const AZStd::string& searchRoot : m_searchRoots)
        {
            searchLocations.push_back(searchRoot);
        }

        for (const AZStd::string& location : searchLocations)
        {
            for (const AZStd::string& relativeCandidate : relativeCandidates)
            {
                AZStd::string fullPath;
                AZ::StringFunc::Path::Join(location.c_str(), relativeCandidate.c_str(), fullPath);
                if (resolution.IsResolved())
                {
                    // Everything after the match is lower priority and can never shadow it.
                    return resolution;
                }

                if (AZ::IO::SystemFile::Exists(fullPath.c_str()))
                {
                    resolution.m_resolvedPath = AZStd::move(fullPath);
                }
                else
                {
                    // Not found here: if the module resolves later (or never), this path
                    // would shadow (or satisfy) the reference if it appeared — report it so
                    // the caller registers it as a source dependency.
                    resolution.m_shadowCandidates.push_back(AZStd::move(fullPath));
                }
            }
        }

        return resolution;
    }

    AZStd::vector<AZStd::string> SlangModuleResolver::ParseModuleReferences(AZStd::string_view sourceText)
    {
        AZStd::vector<AZStd::string> references;

        // Matches: import A.B.C;  import "path/file.slang";  __include A.B;  import A, B;
        // Statements must be single-line (the reference list may not span newlines, or a
        // stray "import" in a comment would swallow real imports below it).
        // Over-prescriptive on purpose: matches inside comments or disabled preprocessor
        // branches are acceptable (extra dependencies are safe; missing ones are not).
        const AZStd::regex importRegex(R"~((?:^|;|\}|\s)(?:import|__include)\s+([^;\r\n]+);)~", AZStd::regex::ECMAScript);

        AZStd::string haystack(sourceText); // AZStd::regex iterates over null-terminated storage.
        auto matchesBegin = AZStd::sregex_iterator(haystack.begin(), haystack.end(), importRegex);
        auto matchesEnd = AZStd::sregex_iterator();
        for (auto matchIterator = matchesBegin; matchIterator != matchesEnd; ++matchIterator)
        {
            const AZStd::string referenceList = (*matchIterator)[1].str();

            // Comma-separated import lists resolve independently.
            AZStd::vector<AZStd::string> tokens;
            AZ::StringFunc::Tokenize(referenceList, tokens, ',');
            for (AZStd::string& token : tokens)
            {
                AZ::StringFunc::TrimWhiteSpace(token, true, true);
                // Strip string-form quotes.
                if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
                {
                    token = token.substr(1, token.size() - 2);
                }
                if (!token.empty())
                {
                    references.push_back(AZStd::move(token));
                }
            }
        }

        return references;
    }
} // namespace AZ::ShaderBuilder
