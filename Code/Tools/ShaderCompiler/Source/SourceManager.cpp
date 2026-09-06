/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SourceManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace AZ::ShaderCompiler
{
    namespace
    {
        uint32_t CheckedSize(size_t size)
        {
            if (size >= std::numeric_limits<uint32_t>::max())
            {
                throw std::runtime_error("shader source storage exceeds the 32-bit location limit");
            }
            return static_cast<uint32_t>(size);
        }
    } // namespace

    SourceManager::SourceManager()
    {
        m_buffers.emplace_back();
        m_instances.emplace_back();
        m_locations.emplace_back();
    }

    BufferId SourceManager::AddSource(std::string name, std::string text)
    {
        CheckedSize(text.size());
        Buffer buffer;
        buffer.name = std::move(name);
        buffer.bytes = std::move(text);
        return RegisterBuffer(std::move(buffer));
    }

    BufferId SourceManager::RegisterBuffer(Buffer buffer)
    {
        buffer.lines.push_back(0);
        for (size_t i = 0; i < buffer.bytes.size(); ++i)
        {
            const unsigned char first = static_cast<unsigned char>(buffer.bytes[i]);
            if (first >= 128)
            {
                size_t count = 0;
                if (first >= 0xc2 && first <= 0xdf)
                {
                    count = 1;
                }
                else if (first >= 0xe0 && first <= 0xef)
                {
                    count = 2;
                }
                else if (first >= 0xf0 && first <= 0xf4)
                {
                    count = 3;
                }
                bool valid = count && buffer.bytes.size() - i - 1 >= count;
                uint32_t value = first & (0x7f >> count);
                for (size_t j = 1; valid && j <= count; ++j)
                {
                    unsigned char c = static_cast<unsigned char>(buffer.bytes[i + j]);
                    valid = (c & 0xc0) == 0x80;
                    value = (value << 6) | (c & 0x3f);
                }
                uint32_t minimumValue = 0x10000;
                if (count == 1)
                {
                    minimumValue = 0x80;
                }
                else if (count == 2)
                {
                    minimumValue = 0x800;
                }
                if (!valid || value < minimumValue || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                {
                    throw std::runtime_error("invalid UTF-8 at byte " + std::to_string(i) + " in " + buffer.name);
                }
                i += count;
                continue;
            }
            if (buffer.bytes[i] == '\r')
            {
                if (i + 1 < buffer.bytes.size() && buffer.bytes[i + 1] == '\n')
                {
                    ++i;
                }
                buffer.lines.push_back(CheckedSize(i + 1));
            }
            else if (buffer.bytes[i] == '\n')
            {
                buffer.lines.push_back(CheckedSize(i + 1));
            }
        }
        m_sourceBytes += buffer.bytes.size();
        BufferId id = CheckedSize(m_buffers.size());
        m_buffers.push_back(std::move(buffer));
        return id;
    }

    BufferId SourceManager::LoadFile(const std::string& path)
    {
        const std::string identity = std::filesystem::canonical(path).generic_string();
        std::unordered_map<std::string, BufferId>::iterator existingFile = m_files.find(identity);
        if (existingFile != m_files.end())
        {
            return existingFile->second;
        }
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw std::runtime_error("cannot open shader source: " + path);
        }
        const std::streampos size = stream.tellg();
        if (size < 0)
        {
            throw std::runtime_error("cannot determine shader source size: " + path);
        }
        Buffer buffer;
        buffer.name = path;
        buffer.bytes.resize(CheckedSize(static_cast<size_t>(size)));
        stream.seekg(0);
        if (!stream.read(buffer.bytes.data(), static_cast<std::streamsize>(buffer.bytes.size())))
        {
            throw std::runtime_error("cannot read shader source: " + path);
        }
        const BufferId id = RegisterBuffer(std::move(buffer));
        m_files.emplace(identity, id);
        ++m_fileReads;
        return id;
    }

    std::string SourceManager::BufferText(BufferId buffer) const
    {
        if (!buffer || buffer >= m_buffers.size())
        {
            throw std::out_of_range("invalid source buffer ID");
        }
        return m_buffers[buffer].bytes;
    }

    SourceInstanceId SourceManager::EnterSource(BufferId buffer, std::string path, SourceLocationId includedAt)
    {
        if (!buffer || buffer >= m_buffers.size())
        {
            throw std::out_of_range("invalid source buffer ID");
        }
        const SourceInstanceId id = CheckedSize(m_instances.size());
        m_instances.push_back({ buffer, std::move(path), includedAt, {} });
        return id;
    }

    SourceLocationId SourceManager::Locate(SourceInstanceId instance, uint32_t offset)
    {
        if (!instance || instance >= m_instances.size() || offset > m_buffers[m_instances[instance].buffer].bytes.size())
        {
            throw std::out_of_range("source location is outside its include instance");
        }
        SourceLocationId id = CheckedSize(m_locations.size());
        m_locations.push_back({ instance, offset, 0, 0, 0 });
        return id;
    }

    SourceLocationId SourceManager::Expand(SourceLocationId spelling, SourceLocationId invocation, SourceLocationId second)
    {
        if (spelling >= m_locations.size() || invocation >= m_locations.size() || second >= m_locations.size())
        {
            throw std::out_of_range("invalid expansion location ID");
        }
        SourceLocationId id = CheckedSize(m_locations.size());
        m_locations.push_back({ 0, 0, spelling, invocation, second });
        return id;
    }

    size_t SourceManager::PhysicalLine(BufferId buffer, uint32_t offset) const
    {
        const std::vector<uint32_t>& lines = m_buffers.at(buffer).lines;
        return static_cast<size_t>(std::upper_bound(lines.begin(), lines.end(), offset) - lines.begin());
    }

    void SourceManager::Remap(SourceInstanceId instance, uint32_t offset, size_t line, std::optional<std::string> file)
    {
        if (!instance || instance >= m_instances.size() || !line || offset > m_buffers[m_instances[instance].buffer].bytes.size())
        {
            throw std::out_of_range("line remap is outside its include instance");
        }
        Instance& source = m_instances.at(instance);
        if (!source.remaps.empty() && offset < source.remaps.back().offset)
        {
            throw std::invalid_argument("line remaps must follow source order");
        }
        if (!file)
        {
            if (source.remaps.empty())
            {
                file = source.path;
            }
            else
            {
                file = source.remaps.back().file;
            }
        }
        source.remaps.push_back({ offset, PhysicalLine(source.buffer, offset), line, std::move(*file) });
    }

    ResolvedSourceLocation SourceManager::Resolve(SourceLocationId location, bool spelling) const
    {
        if (!location)
        {
            return {};
        }
        while (m_locations.at(location).invocation)
        {
            if (spelling)
            {
                location = m_locations[location].spelling;
            }
            else
            {
                location = m_locations[location].invocation;
            }
        }
        const LocationRecord& loc = m_locations[location];
        if (!loc.instance)
        {
            return {};
        }
        const Instance& source = m_instances.at(loc.instance);
        size_t line = PhysicalLine(source.buffer, loc.offset);
        const Buffer& buffer = m_buffers[source.buffer];
        size_t column = 1;
        size_t lineStart = buffer.lines[line - 1];
        if (!lineStart && buffer.bytes.compare(0, 3, "\xef\xbb\xbf") == 0 && loc.offset >= 3)
        {
            lineStart = 3;
        }
        for (size_t i = lineStart; i < loc.offset; ++i)
        {
            // Count UTF-8 leading bytes, not continuation bytes. The lexer validates encoding.
            if ((static_cast<unsigned char>(buffer.bytes[i]) & 0xc0) != 0x80)
            {
                ++column;
            }
        }
        std::string file = source.path;
        std::vector<LineRemap>::const_iterator it = std::upper_bound(
            source.remaps.begin(),
            source.remaps.end(),
            loc.offset,
            [](uint32_t offset, const LineRemap& remap)
            {
                return offset < remap.offset;
            });
        if (it != source.remaps.begin())
        {
            --it;
            file = it->file;
            line = it->line + line - it->physicalLine;
        }
        return { file, line, column };
    }

    std::string SourceManager::Notes(SourceLocationId location) const
    {
        std::string result;
        size_t depth = 0;
        while (location && depth++ < 32)
        {
            const LocationRecord& record = m_locations.at(location);
            SourceLocationId next = record.invocation;
            if (next)
            {
                const ResolvedSourceLocation definition = Resolve(record.spelling, true);
                result += "\n" + std::string(definition.file) + "(" + std::to_string(definition.line) + "," +
                    std::to_string(definition.column) + ") : note: expanded from here";
            }
            else if (record.instance)
            {
                next = m_instances[record.instance].includedAt;
                if (next)
                {
                    const ResolvedSourceLocation included = Resolve(next);
                    result += "\n" + std::string(included.file) + "(" + std::to_string(included.line) + "," +
                        std::to_string(included.column) + ") : note: included from here";
                }
            }
            location = next;
        }
        return result;
    }

    ResolvedSourceLocation SourceLocation::Resolve() const
    {
        if (*this)
        {
            return manager->Resolve(id);
        }
        return {};
    }
    std::string SourceLocation::Notes() const
    {
        if (*this)
        {
            return manager->Notes(id);
        }
        return {};
    }
} // namespace AZ::ShaderCompiler
