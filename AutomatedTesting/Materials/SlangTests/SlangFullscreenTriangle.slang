/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// The first end-to-end Slang shader: a fullscreen triangle with a private ShaderResourceGroup,
// authored with zero imports — the builder force-includes the Atom prelude.

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct TriangleShaderResourceGroupLayout
{
    Vector4F m_tintColor;
};
ParameterBlock<TriangleShaderResourceGroupLayout> TriangleSrg;

struct VertexOutput
{
    Vector4F m_position : SV_Position;
    Vector2F m_uv : UV0;
};

VertexOutput MainVS(u32 vertexIndex : SV_VertexID)
{
    // Fullscreen triangle from the vertex index alone; no vertex buffer
    const Vector2F uv = Vector2F(f32((vertexIndex << 1) & 2), f32(vertexIndex & 2));
    VertexOutput output;
    output.m_position = Vector4F(uv * Vector2F(2, -2) + Vector2F(-1, 1), 0, 1);
    output.m_uv = uv;
    return output;
}

struct PixelOutput
{
    Vector4F m_color : SV_Target0;
};

PixelOutput MainPS(VertexOutput input)
{
    PixelOutput output;
    output.m_color = Vector4F(input.m_uv, 0, 1) * TriangleSrg.m_tintColor;
    return output;
}
