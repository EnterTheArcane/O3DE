/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace AZ::ShaderBuilder
{
    //! The single authority for mapping Slang module references to files.
    //!
    //! Both consumers of module resolution share this class so they can never disagree:
    //! - the Asset Processor CreateJobs scanner, which must declare source dependencies
    //!   (CreateJobs is the AP's only source-dependency channel), and
    //! - the compile-time filesystem hook the Slang backend installs on its sessions.
    //!
    //! Resolution rules owned here:
    //! - dotted module names map to paths ("Atom.RPI.Prelude" -> "Atom/RPI/Prelude.slang");
    //! - '_' in a module name segment may match '-' in the filename (Slang's rule);
    //! - string-form references ("import \"file-name.slang\";") are used verbatim;
    //! - candidates are tried in the importing file's directory first, then each search
    //!   root in priority order.
    //!
    //! Shadow candidates: for a reference resolved from a lower-priority root, the
    //! nonexistent candidate paths in higher-priority roots are reported too, so callers can
    //! register them as source dependencies — a same-named module appearing in a
    //! higher-priority root later must retrigger the build. This mirrors what
    //! AppendListOfPossibleFutureLocations does for unresolved #includes in
    //! ShaderAssetBuilder.cpp.
    class SlangModuleResolver final
    {
    public:
        SlangModuleResolver() = default;
        explicit SlangModuleResolver(AZStd::vector<AZStd::string> orderedSearchRoots);

        //! Replaces the ordered list of search roots (highest priority first).
        void SetSearchRoots(AZStd::vector<AZStd::string> orderedSearchRoots);
        AZStd::span<const AZStd::string> GetSearchRoots() const;

        struct Resolution
        {
            //! Full path of the file the reference resolves to; empty when unresolved.
            AZStd::string m_resolvedPath;

            //! When resolved: the nonexistent candidate paths that would shadow
            //! m_resolvedPath if they appeared (higher-priority roots, alternate spellings).
            //! When unresolved: every candidate path where the module may appear in the future.
            AZStd::vector<AZStd::string> m_shadowCandidates;

            bool IsResolved() const
            {
                return !m_resolvedPath.empty();
            }
        };

        //! Resolves a module reference as written in source — either a dotted module name
        //! ("Atom.RPI.Prelude") or a string path ("Atom/RPI/Prelude.slang").
        //! @param moduleReference The reference, without the "import" keyword or quotes.
        //! @param importingFilePath Full path of the file containing the reference; its
        //!        directory is the highest-priority search location. May be empty.
        Resolution ResolveModule(
            AZStd::string_view moduleReference,
            AZStd::string_view importingFilePath) const;

        //! Extracts the module references from Slang source text: `import <name>;`,
        //! `import "<path>";`, `__include <name>;` and comma-separated lists thereof.
        //! Deliberately over-prescriptive (comments and disabled preprocessor branches are
        //! not understood) — over-reporting dependencies is safe, under-reporting is not,
        //! matching the philosophy of ShaderBuilderUtility::IncludedFilesParser.
        static AZStd::vector<AZStd::string> ParseModuleReferences(AZStd::string_view sourceText);

        //! Returns the relative file paths a dotted module name may map to, in match order
        //! ("Foo.bar_baz" -> {"Foo/bar_baz.slang", "Foo/bar-baz.slang"}).
        //! String-form references return the single verbatim path.
        //! Exposed for testability.
        static AZStd::vector<AZStd::string> GetRelativePathCandidates(AZStd::string_view moduleReference);

    private:
        AZStd::vector<AZStd::string> m_searchRoots;
    };
} // namespace AZ::ShaderBuilder
