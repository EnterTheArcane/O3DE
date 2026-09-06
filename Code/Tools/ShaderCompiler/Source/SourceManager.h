/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/containers/span.h>

#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AZ::ShaderCompiler
{
    // Keep formatted diagnostics independent of source and compilation-unit lifetimes.
    class SourceError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    using BufferId = uint32_t;
    using SourceLocationId = uint32_t;
    using SourceInstanceId = uint32_t;

    struct TextSpan
    {
        BufferId m_buffer = 0;
        uint32_t m_offset = 0;
        uint32_t m_length = 0;
    };

    struct ResolvedSourceLocation
    {
        std::string_view m_file;
        size_t m_line = 0;
        size_t m_column = 0; // One-based Unicode code-point column.
    };

    class SourceManager;

    // A location crossing an API boundary carries its owner. Persistent tokens use only the ID.
    struct SourceLocation
    {
        const SourceManager* m_manager = nullptr;
        SourceLocationId m_id = 0;

        explicit operator bool() const
        {
            return m_manager && m_id;
        }

        ResolvedSourceLocation Resolve() const;

        std::string Notes() const;

        friend bool operator==(SourceLocation a, SourceLocation b)
        {
            return a.m_manager == b.m_manager && a.m_id == b.m_id;
        }

        friend bool operator!=(SourceLocation a, SourceLocation b)
        {
            return !(a == b);
        }

        friend std::ostream& operator<<(std::ostream& out, SourceLocation source)
        {
            return out << source.Resolve().m_line;
        }
    };

    class SourceManager
    {
    public:
        SourceManager();

        SourceManager(const SourceManager&) = delete;

        SourceManager& operator=(const SourceManager&) = delete;

        BufferId LoadFile(const std::string& path);

        BufferId AddSource(std::string name, std::string text);

        TextSpan StoreGenerated(std::string_view text);

        std::string_view Text(TextSpan span) const;

        std::string_view BufferText(BufferId buffer) const;

        SourceInstanceId EnterSource(BufferId buffer, std::string path, SourceLocationId includedAt = 0);

        SourceLocationId Locate(SourceInstanceId instance, uint32_t offset);

        SourceLocationId Expand(SourceLocationId spelling, SourceLocationId invocation, SourceLocationId second = 0);

        void Remap(SourceInstanceId instance, uint32_t offset, size_t line, std::string file);

        ResolvedSourceLocation Resolve(SourceLocationId location, bool spelling = false) const;

        std::string Notes(SourceLocationId location) const;

        SourceLocation Location(SourceLocationId id) const
        {
            return { this, id };
        }

        size_t SourceBytes() const
        {
            return m_sourceBytes;
        }

        size_t GeneratedBytes() const
        {
            return m_generatedBytes;
        }

        size_t FileReads() const
        {
            return m_fileReads;
        }

        size_t LocationCount() const
        {
            return m_locations.size() - 1;
        }

        size_t LocationBytes() const
        {
            return m_locations.capacity() * sizeof(LocationRecord);
        }

        size_t GeneratedCapacity() const
        {
            return m_generatedCapacity;
        }

    private:
        struct Buffer
        {
            std::string m_name;
            std::string m_bytes;
            std::vector<uint32_t> m_lines;
            std::vector<uint32_t> m_continuations;
        };

        struct LineRemap
        {
            uint32_t m_offset;
            size_t m_physicalLine;
            size_t m_line;
            std::string m_file;
        };

        struct Instance
        {
            BufferId m_buffer;
            std::string m_path;
            SourceLocationId m_includedAt;
            std::vector<LineRemap> m_remaps;
        };

        struct LocationRecord
        {
            SourceInstanceId m_instance = 0;
            uint32_t m_offset = 0;
            SourceLocationId m_spelling = 0;
            SourceLocationId m_invocation = 0;
            SourceLocationId m_second = 0;
        };

        BufferId RegisterBuffer(std::unique_ptr<Buffer> buffer);

        size_t PhysicalLine(BufferId buffer, uint32_t offset) const;

        std::vector<std::unique_ptr<Buffer>> m_buffers;
        std::vector<std::unique_ptr<Instance>> m_instances;
        std::vector<LocationRecord> m_locations;
        std::unordered_map<std::string, BufferId> m_files;
        BufferId m_generatedBlock = 0;
        uint32_t m_generatedUsed = 0;
        size_t m_sourceBytes = 0;
        size_t m_generatedBytes = 0;
        size_t m_generatedCapacity = 0;
        size_t m_fileReads = 0;
    };
} // namespace AZ::ShaderCompiler
