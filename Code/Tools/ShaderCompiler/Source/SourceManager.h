/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <cstdint>
#include <ostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace AZ::ShaderCompiler
{
    using BufferId = uint32_t;
    using SourceLocationId = uint32_t;
    using SourceInstanceId = uint32_t;

    struct ResolvedSourceLocation
    {
        std::string file;
        size_t line = 0;
        size_t column = 0; // One-based Unicode code-point column.
    };

    class SourceManager;

    // A location crossing an API boundary carries its owner. Persistent tokens use only the ID.
    struct SourceLocation
    {
        const SourceManager* manager = nullptr;
        SourceLocationId id = 0;

        explicit operator bool() const
        {
            return manager && id;
        }

        ResolvedSourceLocation Resolve() const;
        std::string Notes() const;

        friend bool operator==(SourceLocation a, SourceLocation b)
        {
            return a.manager == b.manager && a.id == b.id;
        }

        friend bool operator!=(SourceLocation a, SourceLocation b)
        {
            return !(a == b);
        }

        friend std::ostream& operator<<(std::ostream& out, SourceLocation source)
        {
            return out << source.Resolve().line;
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
        std::string BufferText(BufferId buffer) const;

        SourceInstanceId EnterSource(BufferId buffer, std::string path, SourceLocationId includedAt = 0);
        SourceLocationId Locate(SourceInstanceId instance, uint32_t offset);
        SourceLocationId Expand(SourceLocationId spelling, SourceLocationId invocation, SourceLocationId second = 0);
        void Remap(SourceInstanceId instance, uint32_t offset, size_t line, std::optional<std::string> file);

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

    private:
        struct Buffer
        {
            std::string name;
            std::string bytes;
            std::vector<uint32_t> lines;
        };

        struct LineRemap
        {
            uint32_t offset;
            size_t physicalLine;
            size_t line;
            std::string file;
        };

        struct Instance
        {
            BufferId buffer;
            std::string path;
            SourceLocationId includedAt;
            std::vector<LineRemap> remaps;
        };

        struct LocationRecord
        {
            SourceInstanceId instance = 0;
            uint32_t offset = 0;
            SourceLocationId spelling = 0;
            SourceLocationId invocation = 0;
            SourceLocationId second = 0;
        };

        BufferId RegisterBuffer(Buffer buffer);
        size_t PhysicalLine(BufferId buffer, uint32_t offset) const;

        std::vector<Buffer> m_buffers;
        std::vector<Instance> m_instances;
        std::vector<LocationRecord> m_locations;
        std::unordered_map<std::string, BufferId> m_files;
        size_t m_sourceBytes = 0;
        size_t m_fileReads = 0;
    };
} // namespace AZ::ShaderCompiler
