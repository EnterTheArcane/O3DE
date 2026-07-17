/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <slang.h>

namespace AZ
{
    class ReflectContext;
}

namespace AZ::ShaderBuilder
{
    //! The cached form of one Slang frontend run: every module loaded in the session, serialized
    //! in load order. A serialized Slang module does not contain its imports, so the whole
    //! closure is required to relink without source. The fingerprint identifies the compiler,
    //! target and schema that produced the bundle; consumers must reject-and-recompile on any
    //! mismatch — never load optimistically.
    struct SlangModuleClosureBundle final
    {
        AZ_TYPE_INFO(SlangModuleClosureBundle, "{5D1A9C3E-7B42-4F80-9E16-A3C8D5F00B21}");
        AZ_CLASS_ALLOCATOR(SlangModuleClosureBundle, AZ::SystemAllocator);

        static void Reflect(ReflectContext* context);

        static constexpr uint32_t CurrentSchemaVersion = 1;

        struct Module final
        {
            AZ_TYPE_INFO(Module, "{E4B6A2D8-0C95-47F1-B3A7-6D28E9C4F513}");

            AZStd::string m_name;
            AZStd::string m_path;
            AZStd::vector<uint8_t> m_serializedModule;
        };

        uint32_t m_schemaVersion = CurrentSchemaVersion;

        //! IGlobalSession::getBuildTagString() of the compiler that produced the bundle.
        AZStd::string m_compilerBuildTag;

        //! RHI::ShaderTargetFormat the session targeted.
        uint32_t m_targetFormat = 0;

        //! Modules in session load order; reloading must preserve it.
        AZStd::vector<Module> m_modules;
    };

    //! Serializes every module loaded in @session into a bundle. The caller must hold the
    //! compiler lock.
    AZ::Outcome<SlangModuleClosureBundle, AZStd::string> BuildModuleClosureBundle(
        slang::ISession* session,
        AZStd::string_view compilerBuildTag,
        uint32_t targetFormat);
} // namespace AZ::ShaderBuilder
