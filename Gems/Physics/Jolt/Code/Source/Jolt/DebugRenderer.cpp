/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/DebugRenderer.h>

#ifdef JPH_DEBUG_RENDERER

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/sort.h>

#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Jolt
{
    namespace
    {
        static_assert(std::is_standard_layout_v<DebugVertex>);
        static_assert(std::is_trivially_copyable_v<DebugVertex>);
        static_assert(std::is_standard_layout_v<JPH::DebugRenderer::Vertex>);
        static_assert(std::is_trivially_copyable_v<JPH::DebugRenderer::Vertex>);
        static_assert(sizeof(DebugVertex) == sizeof(JPH::DebugRenderer::Vertex));
        static_assert(offsetof(DebugVertex, m_position) == offsetof(JPH::DebugRenderer::Vertex, mPosition));
        static_assert(offsetof(DebugVertex, m_normal) == offsetof(JPH::DebugRenderer::Vertex, mNormal));
        static_assert(offsetof(DebugVertex, m_uv) == offsetof(JPH::DebugRenderer::Vertex, mUV));
        static_assert(offsetof(DebugVertex, m_colorRgba8) == offsetof(JPH::DebugRenderer::Vertex, mColor));
        static_assert(offsetof(JPH::DebugRenderer::Triangle, mV) == 0);
        static_assert(sizeof(JPH::DebugRenderer::Triangle) == sizeof(DebugVertex) * 3);

        template<class Type>
        [[nodiscard]]
        int CompareValue(
            const Type& left,
            const Type& right)
        {
            if (left < right)
            {
                return -1;
            }
            if (right < left)
            {
                return 1;
            }
            return 0;
        }

        [[nodiscard]]
        int CompareFloatBits(
            const float left,
            const float right)
        {
            return CompareValue(
                std::bit_cast<AZ::u32>(left),
                std::bit_cast<AZ::u32>(right));
        }

        [[nodiscard]]
        int CompareDoubleBits(
            const double left,
            const double right)
        {
            return CompareValue(
                std::bit_cast<AZ::u64>(left),
                std::bit_cast<AZ::u64>(right));
        }

        [[nodiscard]]
        int ComparePosition(
            const WorldPosition& left,
            const WorldPosition& right)
        {
            int comparison = CompareDoubleBits(left.m_x, right.m_x);
            if (comparison == 0)
            {
                comparison = CompareDoubleBits(left.m_y, right.m_y);
            }
            if (comparison == 0)
            {
                comparison = CompareDoubleBits(left.m_z, right.m_z);
            }
            return comparison;
        }

        [[nodiscard]]
        int CompareColor(
            const AZ::Color& left,
            const AZ::Color& right)
        {
            int comparison = CompareFloatBits(left.GetR(), right.GetR());
            if (comparison == 0)
            {
                comparison = CompareFloatBits(left.GetG(), right.GetG());
            }
            if (comparison == 0)
            {
                comparison = CompareFloatBits(left.GetB(), right.GetB());
            }
            if (comparison == 0)
            {
                comparison = CompareFloatBits(left.GetA(), right.GetA());
            }
            return comparison;
        }

        [[nodiscard]]
        int CompareGeometryConfiguration(
            const DebugGeometryConfiguration& left,
            const DebugGeometryConfiguration& right)
        {
            for (AZ::u32 row = 0; row < 3; ++row)
            {
                for (AZ::u32 column = 0; column < 3; ++column)
                {
                    const int comparison = CompareFloatBits(
                        left.m_basis.GetElement(row, column),
                        right.m_basis.GetElement(row, column));
                    if (comparison != 0)
                    {
                        return comparison;
                    }
                }
            }

            int comparison = CompareColor(left.m_modelColor, right.m_modelColor);
            if (comparison == 0)
            {
                comparison = ComparePosition(left.m_translation, right.m_translation);
            }
            if (comparison == 0)
            {
                comparison = CompareValue(left.m_cullMode, right.m_cullMode);
            }
            if (comparison == 0)
            {
                comparison = CompareValue(left.m_drawMode, right.m_drawMode);
            }
            if (comparison == 0)
            {
                comparison = CompareValue(left.m_castsShadow, right.m_castsShadow);
            }
            return comparison;
        }

        template<class Type>
        [[nodiscard]]
        int CompareContiguousData(
            const AZStd::vector<Type>& left,
            const AZStd::vector<Type>& right)
        {
            const size_t sharedSize = AZStd::min(left.size(), right.size());
            if (sharedSize > 0)
            {
                const int comparison = std::memcmp(
                    left.data(),
                    right.data(),
                    sharedSize * sizeof(Type));
                if (comparison != 0)
                {
                    return comparison;
                }
            }
            return CompareValue(left.size(), right.size());
        }

        [[nodiscard]]
        int CompareBatch(
            const DebugBatch& left,
            const DebugBatch& right)
        {
            int comparison = CompareContiguousData(left.m_vertices, right.m_vertices);
            if (comparison == 0)
            {
                comparison = CompareContiguousData(left.m_indices, right.m_indices);
            }
            return comparison;
        }

        [[nodiscard]]
        AZ::Color FromNativeColor(
            const JPH::ColorArg color)
        {
            constexpr float InverseChannelMaximum = 1.0f / 255.0f;
            return {
                static_cast<float>(color.r) * InverseChannelMaximum,
                static_cast<float>(color.g) * InverseChannelMaximum,
                static_cast<float>(color.b) * InverseChannelMaximum,
                static_cast<float>(color.a) * InverseChannelMaximum,
            };
        }

        [[nodiscard]]
        AZ::Color FromRgba8(
            const AZ::u32 colorRgba8)
        {
            AZ::Color color;
            color.FromU32(colorRgba8);
            return color;
        }

        [[nodiscard]]
        AZ::Vector3 FromNativeVector(
            const JPH::Vec3Arg value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        WorldPosition FromNativePosition(
            const JPH::RVec3Arg position,
            const WorldPosition& origin)
        {
            return {
                .m_x = static_cast<double>(position.GetX()) + origin.m_x,
                .m_y = static_cast<double>(position.GetY()) + origin.m_y,
                .m_z = static_cast<double>(position.GetZ()) + origin.m_z,
            };
        }

        [[nodiscard]]
        DebugCullMode FromNativeCullMode(
            const JPH::DebugRenderer::ECullMode mode)
        {
            switch (mode)
            {
            case JPH::DebugRenderer::ECullMode::CullBackFace:
                return DebugCullMode::BackFace;
            case JPH::DebugRenderer::ECullMode::CullFrontFace:
                return DebugCullMode::FrontFace;
            case JPH::DebugRenderer::ECullMode::Off:
                return DebugCullMode::None;
            }

            AZ_Assert(false, "Unhandled native debug cull mode.");
            return DebugCullMode::None;
        }

        [[nodiscard]]
        DebugDrawMode FromNativeDrawMode(
            const JPH::DebugRenderer::EDrawMode mode)
        {
            switch (mode)
            {
            case JPH::DebugRenderer::EDrawMode::Solid:
                return DebugDrawMode::Solid;
            case JPH::DebugRenderer::EDrawMode::Wireframe:
                return DebugDrawMode::Wireframe;
            }

            AZ_Assert(false, "Unhandled native debug draw mode.");
            return DebugDrawMode::None;
        }
    } // namespace

    void DebugBatch::AddRef()
    {
        ++m_referenceCount;
    }

    void DebugBatch::Release()
    {
        if (--m_referenceCount == 0)
        {
            delete this;
        }
    }

    void DebugCapture::Configure(
        const DebugCaptureConfiguration& configuration)
    {
        m_configuration = configuration;
        Begin();
        m_geometry.set_capacity(configuration.m_geometryCapacity);
        m_lines.set_capacity(configuration.m_lineCapacity);
        m_texts.set_capacity(configuration.m_textCapacity);
        m_textBytes.set_capacity(configuration.m_textByteCapacity);
        m_triangles.set_capacity(configuration.m_triangleCapacity);
    }

    void DebugCapture::Begin()
    {
        m_geometry.clear();
        m_lines.clear();
        m_texts.clear();
        m_textBytes.clear();
        m_triangles.clear();
        m_statistics = {};
    }

    void DebugCapture::End()
    {
        AZStd::sort(
            m_geometry.begin(),
            m_geometry.end(),
            [](const GeometryRecord& left, const GeometryRecord& right)
            {
                int comparison = CompareGeometryConfiguration(
                    left.m_configuration,
                    right.m_configuration);
                if (comparison == 0)
                {
                    comparison = CompareBatch(*left.m_batch, *right.m_batch);
                }
                return comparison < 0;
            });
        AZStd::sort(
            m_lines.begin(),
            m_lines.end(),
            [](const LineRecord& left, const LineRecord& right)
            {
                int comparison = ComparePosition(left.m_start, right.m_start);
                if (comparison == 0)
                {
                    comparison = ComparePosition(left.m_end, right.m_end);
                }
                if (comparison == 0)
                {
                    comparison = CompareValue(left.m_colorRgba8, right.m_colorRgba8);
                }
                return comparison < 0;
            });
        AZStd::sort(
            m_texts.begin(),
            m_texts.end(),
            [this](const TextRecord& left, const TextRecord& right)
            {
                int comparison = ComparePosition(left.m_position, right.m_position);
                if (comparison == 0)
                {
                    AZStd::string_view leftText;
                    if (left.m_textSize > 0)
                    {
                        leftText = AZStd::string_view(
                            m_textBytes.data() + left.m_textOffset,
                            left.m_textSize);
                    }

                    AZStd::string_view rightText;
                    if (right.m_textSize > 0)
                    {
                        rightText = AZStd::string_view(
                            m_textBytes.data() + right.m_textOffset,
                            right.m_textSize);
                    }
                    comparison = leftText.compare(rightText);
                }
                if (comparison == 0)
                {
                    comparison = CompareValue(left.m_colorRgba8, right.m_colorRgba8);
                }
                if (comparison == 0)
                {
                    comparison = CompareFloatBits(left.m_height, right.m_height);
                }
                return comparison < 0;
            });
        AZStd::sort(
            m_triangles.begin(),
            m_triangles.end(),
            [](const TriangleRecord& left, const TriangleRecord& right)
            {
                int comparison = ComparePosition(left.m_first, right.m_first);
                if (comparison == 0)
                {
                    comparison = ComparePosition(left.m_second, right.m_second);
                }
                if (comparison == 0)
                {
                    comparison = ComparePosition(left.m_third, right.m_third);
                }
                if (comparison == 0)
                {
                    comparison = CompareValue(left.m_colorRgba8, right.m_colorRgba8);
                }
                if (comparison == 0)
                {
                    comparison = CompareValue(left.m_castsShadow, right.m_castsShadow);
                }
                return comparison < 0;
            });
        m_statistics.m_geometryCount = aznumeric_cast<AZ::u32>(m_geometry.size());
        m_statistics.m_lineCount = aznumeric_cast<AZ::u32>(m_lines.size());
        m_statistics.m_textCount = aznumeric_cast<AZ::u32>(m_texts.size());
        m_statistics.m_textByteCount = aznumeric_cast<AZ::u32>(m_textBytes.size());
        m_statistics.m_triangleCount = aznumeric_cast<AZ::u32>(m_triangles.size());
    }

    const DebugCaptureConfiguration& DebugCapture::GetConfiguration() const
    {
        return m_configuration;
    }

    DebugCaptureStatistics DebugCapture::GetStatistics() const
    {
        DebugCaptureStatistics statistics;
        {
            AZStd::lock_guard lock(m_geometryMutex);
            statistics.m_geometryCount = aznumeric_cast<AZ::u32>(m_geometry.size());
            statistics.m_droppedGeometryCount = m_statistics.m_droppedGeometryCount;
        }
        {
            AZStd::lock_guard lock(m_lineMutex);
            statistics.m_lineCount = aznumeric_cast<AZ::u32>(m_lines.size());
            statistics.m_droppedLineCount = m_statistics.m_droppedLineCount;
        }
        {
            AZStd::lock_guard lock(m_textMutex);
            statistics.m_textCount = aznumeric_cast<AZ::u32>(m_texts.size());
            statistics.m_droppedTextCount = m_statistics.m_droppedTextCount;
            statistics.m_textByteCount = aznumeric_cast<AZ::u32>(m_textBytes.size());
        }
        {
            AZStd::lock_guard lock(m_triangleMutex);
            statistics.m_triangleCount = aznumeric_cast<AZ::u32>(m_triangles.size());
            statistics.m_droppedTriangleCount = m_statistics.m_droppedTriangleCount;
        }
        return statistics;
    }

    AZ::u64 DebugCapture::GetRetainedBytes() const
    {
        AZ::u64 retainedBytes = 0;
        {
            AZStd::lock_guard lock(m_geometryMutex);
            retainedBytes += m_geometry.capacity() * sizeof(GeometryRecord);
            for (const GeometryRecord& geometry : m_geometry)
            {
                if (geometry.m_batch)
                {
                    retainedBytes += geometry.m_batch->m_vertices.capacity() * sizeof(DebugVertex);
                    retainedBytes += geometry.m_batch->m_indices.capacity() * sizeof(AZ::u32);
                }
            }
        }
        {
            AZStd::lock_guard lock(m_lineMutex);
            retainedBytes += m_lines.capacity() * sizeof(LineRecord);
        }
        {
            AZStd::lock_guard lock(m_textMutex);
            retainedBytes += m_texts.capacity() * sizeof(TextRecord);
            retainedBytes += m_textBytes.capacity();
        }
        {
            AZStd::lock_guard lock(m_triangleMutex);
            retainedBytes += m_triangles.capacity() * sizeof(TriangleRecord);
        }
        return retainedBytes;
    }

    void DebugCapture::RecordGeometry(
        DebugBatch& batch,
        const DebugGeometryConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_geometryMutex);
        if (m_geometry.size() >= m_configuration.m_geometryCapacity)
        {
            ++m_statistics.m_droppedGeometryCount;
            return;
        }

        m_geometry.push_back({
            .m_batch = &batch,
            .m_configuration = configuration,
        });
    }

    void DebugCapture::RecordLine(
        const WorldPosition& start,
        const WorldPosition& end,
        const AZ::u32 colorRgba8)
    {
        AZStd::lock_guard lock(m_lineMutex);
        if (m_lines.size() >= m_configuration.m_lineCapacity)
        {
            ++m_statistics.m_droppedLineCount;
            return;
        }

        m_lines.push_back({
            .m_start = start,
            .m_end = end,
            .m_colorRgba8 = colorRgba8,
        });
    }

    void DebugCapture::RecordText(
        const WorldPosition& position,
        const AZStd::string_view text,
        const AZ::u32 colorRgba8,
        const float height)
    {
        AZStd::lock_guard lock(m_textMutex);
        const size_t availableTextBytes = m_configuration.m_textByteCapacity - m_textBytes.size();
        if (m_texts.size() >= m_configuration.m_textCapacity || text.size() > availableTextBytes)
        {
            ++m_statistics.m_droppedTextCount;
            return;
        }

        const AZ::u32 textOffset = aznumeric_cast<AZ::u32>(m_textBytes.size());
        m_textBytes.insert(m_textBytes.end(), text.begin(), text.end());
        m_texts.push_back({
            .m_position = position,
            .m_textOffset = textOffset,
            .m_textSize = aznumeric_cast<AZ::u32>(text.size()),
            .m_colorRgba8 = colorRgba8,
            .m_height = height,
        });
    }

    void DebugCapture::RecordTriangle(
        const WorldPosition& first,
        const WorldPosition& second,
        const WorldPosition& third,
        const AZ::u32 colorRgba8,
        const bool castsShadow)
    {
        AZStd::lock_guard lock(m_triangleMutex);
        if (m_triangles.size() >= m_configuration.m_triangleCapacity)
        {
            ++m_statistics.m_droppedTriangleCount;
            return;
        }

        m_triangles.push_back({
            .m_first = first,
            .m_second = second,
            .m_third = third,
            .m_colorRgba8 = colorRgba8,
            .m_castsShadow = castsShadow,
        });
    }

    void DebugCapture::Replay(
        IDebugRenderer& renderer) const
    {
        for (const GeometryRecord& record : m_geometry)
        {
            renderer.DrawGeometry(
                record.m_batch->m_vertices,
                record.m_batch->m_indices,
                record.m_configuration);
        }

        for (const LineRecord& record : m_lines)
        {
            renderer.DrawLine(
                record.m_start,
                record.m_end,
                FromRgba8(record.m_colorRgba8));
        }

        for (const TextRecord& record : m_texts)
        {
            AZStd::string_view text;
            if (record.m_textSize > 0)
            {
                text = AZStd::string_view(
                    m_textBytes.data() + record.m_textOffset,
                    record.m_textSize);
            }

            renderer.DrawText(
                record.m_position,
                text,
                FromRgba8(record.m_colorRgba8),
                record.m_height);
        }

        for (const TriangleRecord& record : m_triangles)
        {
            renderer.DrawTriangle(
                record.m_first,
                record.m_second,
                record.m_third,
                FromRgba8(record.m_colorRgba8),
                record.m_castsShadow);
        }
    }

    DebugRenderer::DebugRenderer()
    {
        Initialize();
    }

    void DebugRenderer::BeginFrame(
        IDebugRenderer& renderer,
        const WorldPosition& origin,
        const WorldPosition& cameraPosition)
    {
        AZ_Assert(!m_renderer && !m_capture, "A Jolt debug frame is already active.");
        m_renderer = &renderer;
        m_origin = origin;
        m_cameraPosition = {
            static_cast<float>(cameraPosition.m_x - origin.m_x),
            static_cast<float>(cameraPosition.m_y - origin.m_y),
            static_cast<float>(cameraPosition.m_z - origin.m_z),
        };
    }

    void DebugRenderer::EndFrame()
    {
        AZ_Assert(m_renderer, "No Jolt debug frame is being drawn.");
        NextFrame();
        m_renderer = nullptr;
    }

    void DebugRenderer::BeginCapture(
        DebugCapture& capture,
        const WorldPosition& origin,
        const WorldPosition& cameraPosition)
    {
        AZ_Assert(!m_renderer && !m_capture, "A Jolt debug frame is already active.");
        capture.Begin();
        m_capture = &capture;
        m_origin = origin;
        m_cameraPosition = {
            static_cast<float>(cameraPosition.m_x - origin.m_x),
            static_cast<float>(cameraPosition.m_y - origin.m_y),
            static_cast<float>(cameraPosition.m_z - origin.m_z),
        };
    }

    void DebugRenderer::EndCapture()
    {
        AZ_Assert(m_capture && !m_renderer, "No Jolt debug capture is active.");
        m_capture->End();
        NextFrame();
        m_capture = nullptr;
    }

    void DebugRenderer::DrawLine(
        const JPH::RVec3Arg start,
        const JPH::RVec3Arg end,
        const JPH::ColorArg color)
    {
        AZ_Assert(m_renderer || m_capture, "No Jolt debug renderer is active.");
        const WorldPosition publicStart = FromNativePosition(start, m_origin);
        const WorldPosition publicEnd = FromNativePosition(end, m_origin);
        if (m_capture)
        {
            m_capture->RecordLine(
                publicStart,
                publicEnd,
                color.GetUInt32());
            return;
        }

        m_renderer->DrawLine(
            publicStart,
            publicEnd,
            FromNativeColor(color));
    }

    void DebugRenderer::DrawTriangle(
        const JPH::RVec3Arg first,
        const JPH::RVec3Arg second,
        const JPH::RVec3Arg third,
        const JPH::ColorArg color,
        const ECastShadow castShadow)
    {
        AZ_Assert(m_renderer || m_capture, "No Jolt debug renderer is active.");
        const WorldPosition publicFirst = FromNativePosition(first, m_origin);
        const WorldPosition publicSecond = FromNativePosition(second, m_origin);
        const WorldPosition publicThird = FromNativePosition(third, m_origin);
        const bool castsShadow = castShadow == ECastShadow::On;
        if (m_capture)
        {
            m_capture->RecordTriangle(
                publicFirst,
                publicSecond,
                publicThird,
                color.GetUInt32(),
                castsShadow);
            return;
        }

        m_renderer->DrawTriangle(
            publicFirst,
            publicSecond,
            publicThird,
            FromNativeColor(color),
            castsShadow);
    }

    void DebugRenderer::DrawText3D(
        const JPH::RVec3Arg position,
        const JPH::string_view& text,
        const JPH::ColorArg color,
        const float height)
    {
        AZ_Assert(m_renderer || m_capture, "No Jolt debug renderer is active.");
        const WorldPosition publicPosition = FromNativePosition(position, m_origin);
        const AZStd::string_view publicText(text.data(), text.size());
        if (m_capture)
        {
            m_capture->RecordText(
                publicPosition,
                publicText,
                color.GetUInt32(),
                height);
            return;
        }

        m_renderer->DrawText(
            publicPosition,
            publicText,
            FromNativeColor(color),
            height);
    }

    DebugBatch* DebugRenderer::CreateBatch(
        const JPH::DebugRenderer::Vertex* vertices,
        const int vertexCount)
    {
        auto* batch = new DebugBatch;
        if (!vertices || vertexCount <= 0)
        {
            return batch;
        }

        const size_t size = aznumeric_cast<size_t>(vertexCount);
        batch->m_vertices.resize_no_construct(size);
        std::memcpy(
            batch->m_vertices.data(),
            vertices,
            size * sizeof(DebugVertex));
        return batch;
    }

    JPH::DebugRenderer::Batch DebugRenderer::CreateTriangleBatch(
        const JPH::DebugRenderer::Triangle* triangles,
        const int triangleCount)
    {
        if (!triangles || triangleCount <= 0)
        {
            return new DebugBatch;
        }

        auto* batch = new DebugBatch;
        const size_t size = aznumeric_cast<size_t>(triangleCount) * 3;
        batch->m_vertices.resize_no_construct(size);
        std::memcpy(
            batch->m_vertices.data(),
            triangles,
            size * sizeof(DebugVertex));
        return batch;
    }

    JPH::DebugRenderer::Batch DebugRenderer::CreateTriangleBatch(
        const JPH::DebugRenderer::Vertex* vertices,
        const int vertexCount,
        const JPH::uint32* indices,
        const int indexCount)
    {
        DebugBatch* batch = CreateBatch(vertices, vertexCount);
        if (indices && indexCount > 0)
        {
            batch->m_indices.assign(indices, indices + indexCount);
        }
        return batch;
    }

    void DebugRenderer::DrawGeometry(
        JPH::RMat44Arg modelMatrix,
        const JPH::AABox& worldBounds,
        const float lodScaleSquared,
        const JPH::ColorArg modelColor,
        const JPH::DebugRenderer::GeometryRef& geometry,
        const ECullMode cullMode,
        const ECastShadow castShadow,
        const EDrawMode drawMode)
    {
        AZ_Assert(m_renderer || m_capture, "No Jolt debug renderer is active.");
        const JPH::DebugRenderer::LOD& lod = geometry->GetLOD(
            m_cameraPosition,
            worldBounds,
            lodScaleSquared);
        auto* batch = static_cast<DebugBatch*>(lod.mTriangleBatch.GetPtr());
        if (!batch || batch->m_vertices.empty())
        {
            return;
        }

        const JPH::Vec3 basisX = modelMatrix.GetAxisX();
        const JPH::Vec3 basisY = modelMatrix.GetAxisY();
        const JPH::Vec3 basisZ = modelMatrix.GetAxisZ();
        const DebugGeometryConfiguration configuration = {
            .m_basis = AZ::Matrix3x3::CreateFromColumns(
                FromNativeVector(basisX),
                FromNativeVector(basisY),
                FromNativeVector(basisZ)),
            .m_modelColor = FromNativeColor(modelColor),
            .m_translation = FromNativePosition(modelMatrix.GetTranslation(), m_origin),
            .m_cullMode = FromNativeCullMode(cullMode),
            .m_drawMode = FromNativeDrawMode(drawMode),
            .m_castsShadow = castShadow == ECastShadow::On,
        };
        if (m_capture)
        {
            m_capture->RecordGeometry(*batch, configuration);
            return;
        }

        m_renderer->DrawGeometry(
            batch->m_vertices,
            batch->m_indices,
            configuration);
    }
} // namespace Jolt

#endif
