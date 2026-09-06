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
                throw std::runtime_error("shader source storage exceeds the 32-bit span limit");
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
        std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>();
        buffer->m_name = std::move(name);
        buffer->m_bytes = std::move(text);
        return RegisterBuffer(std::move(buffer));
    }

    BufferId SourceManager::RegisterBuffer(std::unique_ptr<Buffer> buffer)
    {
        buffer->m_lines.push_back(0);
        for (size_t i = 0; i < buffer->m_bytes.size(); ++i)
        {
            const unsigned char first = static_cast<unsigned char>(buffer->m_bytes[i]);
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
                bool valid = count && buffer->m_bytes.size() - i - 1 >= count;
                uint32_t value = first & (0x7f >> count);
                for (size_t j = 1; valid && j <= count; ++j)
                {
                    unsigned char c = static_cast<unsigned char>(buffer->m_bytes[i + j]);
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
                    throw SourceError("invalid UTF-8 at byte " + std::to_string(i) + " in " + buffer->m_name);
                }
                for (size_t j = 1; j <= count; ++j)
                {
                    buffer->m_continuations.push_back(CheckedSize(i + j));
                }
                i += count;
                continue;
            }
            if (buffer->m_bytes[i] == '\r')
            {
                if (i + 1 < buffer->m_bytes.size() && buffer->m_bytes[i + 1] == '\n')
                {
                    ++i;
                }
                buffer->m_lines.push_back(CheckedSize(i + 1));
            }
            else if (buffer->m_bytes[i] == '\n')
            {
                buffer->m_lines.push_back(CheckedSize(i + 1));
            }
        }
        m_sourceBytes += buffer->m_bytes.size();
        BufferId id = CheckedSize(m_buffers.size());
        m_buffers.push_back(std::move(buffer));
        return id;
    }

    BufferId SourceManager::LoadFile(const std::string& path)
    {
        std::error_code error;
        const std::string identity = std::filesystem::canonical(path, error).generic_string();
        if (error)
        {
            throw SourceError("cannot resolve shader source: " + path + ": " + error.message());
        }
        std::unordered_map<std::string, BufferId>::iterator existingFile = m_files.find(identity);
        if (existingFile != m_files.end())
        {
            return existingFile->second;
        }
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw SourceError("cannot open shader source: " + path);
        }
        const std::streampos size = stream.tellg();
        if (size < 0)
        {
            throw SourceError("cannot determine shader source size: " + path);
        }
        std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>();
        buffer->m_name = path;
        buffer->m_bytes.resize(CheckedSize(static_cast<size_t>(size)));
        stream.seekg(0);
        if (!stream.read(buffer->m_bytes.data(), static_cast<std::streamsize>(buffer->m_bytes.size())))
        {
            throw SourceError("cannot read shader source: " + path);
        }
        const BufferId id = RegisterBuffer(std::move(buffer));
        m_files.emplace(identity, id);
        ++m_fileReads;
        return id;
    }

    TextSpan SourceManager::StoreGenerated(std::string_view text)
    {
        uint32_t size = CheckedSize(text.size());
        if (!m_generatedBlock || m_buffers[m_generatedBlock]->m_bytes.size() - m_generatedUsed < size)
        {
            std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>();
            buffer->m_name = "<generated>";
            buffer->m_bytes.resize(std::max<size_t>(64 * 1024, size));
            m_generatedCapacity += buffer->m_bytes.size();
            m_generatedBlock = CheckedSize(m_buffers.size());
            m_buffers.push_back(std::move(buffer));
            m_generatedUsed = 0;
        }
        TextSpan span{ m_generatedBlock, m_generatedUsed, size };
        std::copy(text.begin(), text.end(), m_buffers[m_generatedBlock]->m_bytes.begin() + m_generatedUsed);
        m_generatedUsed += size;
        m_generatedBytes += size;
        return span;
    }

    std::string_view SourceManager::Text(TextSpan span) const
    {
        const std::string_view bytes = BufferText(span.m_buffer);
        if (span.m_offset > bytes.size() || span.m_length > bytes.size() - span.m_offset)
        {
            throw std::out_of_range("source span is outside its buffer");
        }
        return bytes.substr(span.m_offset, span.m_length);
    }

    std::string_view SourceManager::BufferText(BufferId buffer) const
    {
        if (!buffer || buffer >= m_buffers.size())
        {
            throw std::out_of_range("invalid source buffer ID");
        }
        return m_buffers[buffer]->m_bytes;
    }

    SourceInstanceId SourceManager::EnterSource(BufferId buffer, std::string path, SourceLocationId includedAt)
    {
        BufferText(buffer); // Validate the buffer before publishing an include instance.
        if (m_buffers[buffer]->m_lines.empty())
        {
            throw std::invalid_argument("generated spelling storage is not a source file");
        }
        const SourceInstanceId id = CheckedSize(m_instances.size());
        m_instances.push_back(std::make_unique<Instance>(Instance{ buffer, std::move(path), includedAt, {} }));
        return id;
    }

    SourceLocationId SourceManager::Locate(SourceInstanceId instance, uint32_t offset)
    {
        if (!instance || instance >= m_instances.size() || offset > BufferText(m_instances[instance]->m_buffer).size())
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
        const std::vector<uint32_t>& lines = m_buffers.at(buffer)->m_lines;
        return static_cast<size_t>(std::upper_bound(lines.begin(), lines.end(), offset) - lines.begin());
    }

    void SourceManager::Remap(SourceInstanceId instance, uint32_t offset, size_t line, std::string file)
    {
        if (!instance || instance >= m_instances.size() || !line || offset > BufferText(m_instances[instance]->m_buffer).size())
        {
            throw std::out_of_range("line remap is outside its include instance");
        }
        Instance& source = *m_instances.at(instance);
        if (!source.m_remaps.empty() && offset < source.m_remaps.back().m_offset)
        {
            throw std::invalid_argument("line remaps must follow source order");
        }
        if (file.empty())
        {
            if (source.m_remaps.empty())
            {
                file = source.m_path;
            }
            else
            {
                file = source.m_remaps.back().m_file;
            }
        }
        source.m_remaps.push_back({ offset, PhysicalLine(source.m_buffer, offset), line, std::move(file) });
    }

    ResolvedSourceLocation SourceManager::Resolve(SourceLocationId location, bool spelling) const
    {
        if (!location)
        {
            return {};
        }
        while (m_locations.at(location).m_invocation)
        {
            if (spelling)
            {
                location = m_locations[location].m_spelling;
            }
            else
            {
                location = m_locations[location].m_invocation;
            }
        }
        const LocationRecord& loc = m_locations[location];
        if (!loc.m_instance)
        {
            return {};
        }
        const Instance& source = *m_instances.at(loc.m_instance);
        size_t line = PhysicalLine(source.m_buffer, loc.m_offset);
        const Buffer& buffer = *m_buffers[source.m_buffer];
        size_t lineStart = buffer.m_lines[line - 1];
        if (!lineStart && buffer.m_bytes.compare(0, 3, "\xef\xbb\xbf") == 0 && loc.m_offset >= 3)
        {
            lineStart = 3;
        }
        const auto first = std::lower_bound(buffer.m_continuations.begin(), buffer.m_continuations.end(), lineStart);
        const auto last = std::lower_bound(first, buffer.m_continuations.end(), loc.m_offset);
        const size_t column = loc.m_offset - lineStart - static_cast<size_t>(last - first) + 1;
        std::string_view file = source.m_path;
        std::vector<LineRemap>::const_iterator it = std::upper_bound(
            source.m_remaps.begin(),
            source.m_remaps.end(),
            loc.m_offset,
            [](uint32_t offset, const LineRemap& remap)
            {
                return offset < remap.m_offset;
            });
        if (it != source.m_remaps.begin())
        {
            --it;
            file = it->m_file;
            line = it->m_line + line - it->m_physicalLine;
        }
        return { file, line, column };
    }

    std::string SourceManager::Notes(SourceLocationId location) const
    {
        constexpr size_t noteLimit = 32;
        std::string result;
        std::vector<SourceLocationId> pending{ location };
        std::vector<SourceLocationId> visited;
        std::vector<ResolvedSourceLocation> reported;
        const auto addNote = [&](SourceLocationId origin, bool expansion)
        {
            const auto resolved = Resolve(origin, expansion);
            if (!resolved.m_line
                || reported.size() == noteLimit
                || std::any_of(
                    reported.begin(),
                    reported.end(),
                    [&](const auto& previous)
                    {
                        return previous.m_file == resolved.m_file
                            && previous.m_line == resolved.m_line
                            && previous.m_column == resolved.m_column;
                    }))
            {
                return;
            }
            reported.push_back(resolved);
            result += "\n"
                + std::string(resolved.m_file)
                + "("
                + std::to_string(resolved.m_line)
                + ","
                + std::to_string(resolved.m_column)
                + ") : note: ";
            if (expansion)
            {
                result += "expanded from here";
            }
            else
            {
                result += "included from here";
            }
        };
        while (!pending.empty() && visited.size() < noteLimit && reported.size() < noteLimit)
        {
            location = pending.back();
            pending.pop_back();
            if (!location || std::find(visited.begin(), visited.end(), location) != visited.end())
            {
                continue;
            }
            visited.push_back(location);
            const LocationRecord& record = m_locations.at(location);
            if (record.m_invocation)
            {
                addNote(record.m_spelling, true);
                addNote(record.m_second, true);
                pending.push_back(record.m_invocation);
                pending.push_back(record.m_second);
                pending.push_back(record.m_spelling);
            }
            else if (record.m_instance)
            {
                const auto includedAt = m_instances[record.m_instance]->m_includedAt;
                if (includedAt)
                {
                    addNote(includedAt, false);
                    pending.push_back(includedAt);
                }
            }
        }
        return result;
    }

    ResolvedSourceLocation SourceLocation::Resolve() const
    {
        if (*this)
        {
            return m_manager->Resolve(m_id);
        }
        return {};
    }

    std::string SourceLocation::Notes() const
    {
        if (*this)
        {
            return m_manager->Notes(m_id);
        }
        return {};
    }
} // namespace AZ::ShaderCompiler
