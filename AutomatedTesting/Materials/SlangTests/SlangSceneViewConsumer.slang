/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// M15 (SlangIntegrationPlan.md, Phase 4): a Slang shader consuming the shared Scene/View binding
// ABI. This is a port of the DepthToLinearDepth post-process: it reads ViewSrg's linearize-depth
// constants (and a SceneSrg debug value) through the generated, pinned .slangi declarations, proving
// a Slang shader binds the one shared SRG instance AZSL shaders bind — each pinned to its frequency
// space (ViewSrg space5, SceneSrg space6) so they coexist with the private per-pass SRG.

#include "Atom/Features/Srg/ViewSrg.slangi"
#include "Atom/Features/Srg/SceneSrg.slangi"

// Private per-pass SRG: the depth input to linearize.
[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct DepthToLinearShaderResourceGroupLayout
{
    Texture2D<Vector2F> m_depthStencil;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState m_pointSampler;
};
ParameterBlock<DepthToLinearShaderResourceGroupLayout> PassSrg;

struct VertexOutput
{
    Vector4F m_position : SV_Position;
    Vector2F m_uv : UV0;
};

VertexOutput MainVS(u32 vertexIndex : SV_VertexID)
{
    // Fullscreen triangle from the vertex index alone; no vertex buffer.
    const Vector2F uv = Vector2F(f32((vertexIndex << 1) & 2), f32(vertexIndex & 2));
    VertexOutput output;
    output.m_position = Vector4F(uv * Vector2F(2, -2) + Vector2F(-1, 1), 0, 1);
    output.m_uv = uv;
    return output;
}

struct PixelOutput
{
    f32 m_linearDepth : SV_Target0;
};

PixelOutput MainPS(VertexOutput input)
{
    const f32 zDepth = PassSrg.m_depthStencil.Sample(PassSrg.m_pointSampler, input.m_uv).r;

    // ViewSrg's m_linearizeDepthConstants pack (x: nearZ, y: farZ, z: farZ*nearZ, w: farZ-nearZ),
    // read straight from the shared View SRG-constants buffer — the same values AZSL's
    // ViewSrg::GetFarZ()/GetFarZTimesNearZ() accessors return.
    const Vector4F linearizeConstants = ViewSrg_SRGConstantBuffer.m_linearizeDepthConstants;
    f32 linearDepth = abs(linearizeConstants.z / (linearizeConstants.w * zDepth - linearizeConstants.y));

    // Fold in a Scene-frequency value to prove SceneSrg (space6) binds alongside ViewSrg (space5).
    linearDepth *= SceneSrg_SRGConstantBuffer.m_iblExposure;

    PixelOutput output;
    output.m_linearDepth = linearDepth;
    return output;
}
