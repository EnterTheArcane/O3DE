/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangSourceFileSystem.h"

#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/string/string_view.h>

#include <AzFramework/StringFunc/StringFunc.h>

namespace AZ::ShaderBuilder
{
    static bool IsUuidEqual(const SlangUUID& lhs, const SlangUUID& rhs)
    {
        return memcmp(&lhs, &rhs, sizeof(SlangUUID)) == 0;
    }

    //! Reference-counted blob over an owned string, handed to the compile session.
    class SourceBlob final : public ISlangBlob
    {
    public:
        explicit SourceBlob(AZStd::string content)
            : m_content(AZStd::move(content))
        {
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& uuid, void** outObject) override
        {
            if (IsUuidEqual(uuid, ISlangUnknown::getTypeGuid()) || IsUuidEqual(uuid, ISlangBlob::getTypeGuid()))
            {
                addRef();
                *outObject = static_cast<ISlangBlob*>(this);
                return SLANG_OK;
            }
            *outObject = nullptr;
            return SLANG_E_NO_INTERFACE;
        }

        SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
        {
            return ++m_referenceCount;
        }

        SLANG_NO_THROW uint32_t SLANG_MCALL release() override
        {
            const uint32_t remaining = --m_referenceCount;
            if (remaining == 0)
            {
                delete this;
            }
            return remaining;
        }

        SLANG_NO_THROW const void* SLANG_MCALL getBufferPointer() override
        {
            return m_content.data();
        }

        SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override
        {
            return m_content.size();
        }

    private:
        AZStd::string m_content;
        AZStd::atomic<uint32_t> m_referenceCount{1};
    };

    SlangSourceFileSystem::SlangSourceFileSystem(
        AZStd::vector<AZStd::string> injectedImportLines,
        AZStd::vector<AZStd::string> injectionExemptFileNames)
        : m_injectedImportLines(AZStd::move(injectedImportLines))
        , m_injectionExemptFileNames(AZStd::move(injectionExemptFileNames))
    {
    }

    void* SlangSourceFileSystem::GetInterface(const SlangUUID& uuid)
    {
        if (IsUuidEqual(uuid, ISlangUnknown::getTypeGuid())
            || IsUuidEqual(uuid, ISlangCastable::getTypeGuid())
            || IsUuidEqual(uuid, ISlangFileSystem::getTypeGuid()))
        {
            return static_cast<ISlangFileSystem*>(this);
        }
        return nullptr;
    }

    SlangResult SlangSourceFileSystem::queryInterface(const SlangUUID& uuid, void** outObject)
    {
        if (void* object = GetInterface(uuid))
        {
            addRef();
            *outObject = object;
            return SLANG_OK;
        }
        *outObject = nullptr;
        return SLANG_E_NO_INTERFACE;
    }

    uint32_t SlangSourceFileSystem::addRef()
    {
        return ++m_referenceCount;
    }

    uint32_t SlangSourceFileSystem::release()
    {
        const uint32_t remaining = --m_referenceCount;
        if (remaining == 0)
        {
            delete this;
        }
        return remaining;
    }

    void* SlangSourceFileSystem::castAs(const SlangUUID& uuid)
    {
        return GetInterface(uuid);
    }

    SlangResult SlangSourceFileSystem::loadFile(const char* path, ISlangBlob** outBlob)
    {
        *outBlob = nullptr;

        AZ::IO::SystemFile file;
        if (!file.Open(path, AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
        {
            return SLANG_E_NOT_FOUND;
        }
        AZStd::string content;
        content.resize_no_construct(file.Length());
        const AZ::IO::SystemFile::SizeType bytesRead = file.Read(content.size(), content.data());
        if (bytesRead != content.size())
        {
            return SLANG_FAIL;
        }

        AZStd::string fileName;
        AzFramework::StringFunc::Path::GetFullFileName(path, fileName);
        const bool isSlangSource = fileName.ends_with(".slang");
        const bool isInjectionExempt = AZStd::find(m_injectionExemptFileNames.begin(), m_injectionExemptFileNames.end(), fileName)
            != m_injectionExemptFileNames.end();

        if (isSlangSource && !isInjectionExempt && !m_injectedImportLines.empty())
        {
            AZStd::string forwardSlashedPath(path);
            AZStd::replace(forwardSlashedPath.begin(), forwardSlashedPath.end(), '\\', '/');

            AZStd::string injectedContent;
            for (const AZStd::string& importLine : m_injectedImportLines)
            {
                injectedContent += importLine;
                injectedContent += '\n';
            }
            injectedContent += AZStd::string::format("#line 1 \"%s\"\n", forwardSlashedPath.c_str());
            injectedContent += content;
            content = AZStd::move(injectedContent);
        }

        *outBlob = new SourceBlob(AZStd::move(content));
        return SLANG_OK;
    }
} // namespace AZ::ShaderBuilder
