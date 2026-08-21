/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/DebugDraw.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/mutex.h>

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>

namespace Jolt
{
    class JOLT_API DebugBatch final
        : public JPH::RefTargetVirtual
    {
    public:
        DebugBatch() = default;

        AZ_DISABLE_COPY_MOVE(DebugBatch);

        void AddRef() override;

        void Release() override;

        AZStd::vector<DebugVertex> m_vertices;
        AZStd::vector<AZ::u32> m_indices;

    private:
        JPH::atomic<JPH::uint32> m_referenceCount = 0;
    };

    class JOLT_API DebugCapture final
    {
    public:
        DebugCapture() = default;

        AZ_DISABLE_COPY_MOVE(DebugCapture);

        void Configure(const DebugCaptureConfiguration& configuration);

        void Begin();

        void End();

        [[nodiscard]]
        const DebugCaptureConfiguration& GetConfiguration() const;

        [[nodiscard]]
        DebugCaptureStatistics GetStatistics() const;

        [[nodiscard]]
        AZ::u64 GetRetainedBytes() const;

        void RecordGeometry(
            DebugBatch& batch,
            const DebugGeometryConfiguration& configuration);

        void RecordLine(
            const WorldPosition& start,
            const WorldPosition& end,
            AZ::u32 colorRgba8);

        void RecordText(
            const WorldPosition& position,
            AZStd::string_view text,
            AZ::u32 colorRgba8,
            float height);

        void RecordTriangle(
            const WorldPosition& first,
            const WorldPosition& second,
            const WorldPosition& third,
            AZ::u32 colorRgba8,
            bool castsShadow);

        void Replay(IDebugRenderer& renderer) const;

    private:
        struct GeometryRecord final
        {
            JPH::Ref<DebugBatch> m_batch;
            DebugGeometryConfiguration m_configuration;
        };

        struct LineRecord final
        {
            WorldPosition m_start;
            WorldPosition m_end;
            AZ::u32 m_colorRgba8 = 0;
        };

        struct TextRecord final
        {
            WorldPosition m_position;
            AZ::u32 m_textOffset = 0;
            AZ::u32 m_textSize = 0;
            AZ::u32 m_colorRgba8 = 0;
            float m_height = 0.0f;
        };

        struct TriangleRecord final
        {
            WorldPosition m_first;
            WorldPosition m_second;
            WorldPosition m_third;
            AZ::u32 m_colorRgba8 = 0;
            bool m_castsShadow = false;
        };

        DebugCaptureConfiguration m_configuration;
        DebugCaptureStatistics m_statistics;

        mutable AZStd::mutex m_geometryMutex;
        AZStd::vector<GeometryRecord> m_geometry;

        mutable AZStd::mutex m_lineMutex;
        AZStd::vector<LineRecord> m_lines;

        mutable AZStd::mutex m_textMutex;
        AZStd::vector<TextRecord> m_texts;
        AZStd::vector<char> m_textBytes;

        mutable AZStd::mutex m_triangleMutex;
        AZStd::vector<TriangleRecord> m_triangles;
    };

    class DebugRenderer final
        : public JPH::DebugRenderer
    {
    public:
        DebugRenderer();
        ~DebugRenderer() override = default;

        AZ_DISABLE_COPY_MOVE(DebugRenderer);

        void BeginFrame(
            IDebugRenderer& renderer,
            const WorldPosition& origin,
            const WorldPosition& cameraPosition);

        void EndFrame();

        void BeginCapture(
            DebugCapture& capture,
            const WorldPosition& origin,
            const WorldPosition& cameraPosition);

        void EndCapture();

        void DrawLine(
            JPH::RVec3Arg start,
            JPH::RVec3Arg end,
            JPH::ColorArg color) override;

        void DrawTriangle(
            JPH::RVec3Arg first,
            JPH::RVec3Arg second,
            JPH::RVec3Arg third,
            JPH::ColorArg color,
            ECastShadow castShadow) override;

        void DrawText3D(
            JPH::RVec3Arg position,
            const JPH::string_view& text,
            JPH::ColorArg color,
            float height) override;

    private:
        DebugBatch* CreateBatch(
            const JPH::DebugRenderer::Vertex* vertices,
            int vertexCount);

        JPH::DebugRenderer::Batch CreateTriangleBatch(
            const JPH::DebugRenderer::Triangle* triangles,
            int triangleCount) override;

        JPH::DebugRenderer::Batch CreateTriangleBatch(
            const JPH::DebugRenderer::Vertex* vertices,
            int vertexCount,
            const JPH::uint32* indices,
            int indexCount) override;

        void DrawGeometry(
            JPH::RMat44Arg modelMatrix,
            const JPH::AABox& worldBounds,
            float lodScaleSquared,
            JPH::ColorArg modelColor,
            const JPH::DebugRenderer::GeometryRef& geometry,
            ECullMode cullMode,
            ECastShadow castShadow,
            EDrawMode drawMode) override;

        IDebugRenderer* m_renderer = nullptr;
        DebugCapture* m_capture = nullptr;
        WorldPosition m_origin;
        JPH::Vec3 m_cameraPosition = JPH::Vec3::sZero();
    };
} // namespace Jolt

#else

namespace Jolt
{
    class DebugCapture final
    {
    };

    class DebugRenderer final
    {
    };
} // namespace Jolt

#endif
