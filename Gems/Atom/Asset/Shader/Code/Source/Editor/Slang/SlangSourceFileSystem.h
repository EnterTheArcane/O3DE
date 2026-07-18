/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <slang.h>

namespace AZ::ShaderBuilder
{
    //! Serves source files to a Slang compile session, injecting the force-included Atom
    //! preamble into every .slang module it loads: the prelude imports that make the Atom
    //! vocabulary available everywhere with zero per-file imports, and the shader-options
    //! authoring macro definitions (module imports cannot propagate preprocessor definitions).
    //! A #line directive after the injected preamble keeps diagnostics on original file/line
    //! numbers.
    //!
    //! The force-included modules themselves (and anything else named in the exemption list) are
    //! served verbatim so injection cannot create import cycles.
    //!
    //! Instances are created with `new` and managed by COM reference counting
    //! (Slang::ComPtr::attach the initial reference).
    class SlangSourceFileSystem final : public ISlangFileSystem
    {
    public:
        //! @param injectedPreambleLines Complete source lines prepended to every served .slang
        //!                              file: import statements and macro definitions.
        //! @param injectionExemptFileNames File names (no directory) served without injection.
        SlangSourceFileSystem(
            AZStd::vector<AZStd::string> injectedPreambleLines,
            AZStd::vector<AZStd::string> injectionExemptFileNames);

        // ISlangUnknown
        SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& uuid, void** outObject) override;
        SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override;
        SLANG_NO_THROW uint32_t SLANG_MCALL release() override;

        // ISlangCastable
        SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID& uuid) override;

        // ISlangFileSystem
        SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(const char* path, ISlangBlob** outBlob) override;

    private:
        void* GetInterface(const SlangUUID& uuid);

        AZStd::vector<AZStd::string> m_injectedPreambleLines;
        AZStd::vector<AZStd::string> m_injectionExemptFileNames;
        AZStd::atomic<uint32_t> m_referenceCount{1};
    };
} // namespace AZ::ShaderBuilder
