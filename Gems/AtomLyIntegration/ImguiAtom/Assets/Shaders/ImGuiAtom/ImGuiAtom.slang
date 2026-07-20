
/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import Atom.Features.ColorManagement.TransformColor;

struct VertexInput
{
    float2 Position : POSITION;
    float2 UV : UV;
    float4 Color : COLOR;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : UV;
};

[AtomShaderResourceGroup(SrgBindingSlot.Object)]
struct ObjectSrgLayout
{
    Texture2D<float4> FontImage;
    
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
    
    column_major float4x4 m_projectionMatrix;
};
ParameterBlock<ObjectSrgLayout> ObjectSrg;

VertexOutput MainVS(in VertexInput input)
{
    VertexOutput output;
    output.Position = mul(float4(input.Position, 0.0f, 1.0f), ObjectSrg.m_projectionMatrix);
    output.Color = float4(Srgb_To_LinearSrgb(input.Color.rgb), input.Color.a);
    output.UV = input.UV;
    return output;
}

struct PixelOutput
{
     float4 m_color : SV_Target0;
};  
    
PixelOutput MainPS(in VertexOutput input)
{
    PixelOutput output;
    float4 color = ObjectSrg.FontImage.Sample(ObjectSrg.LinearSampler, input.UV) * input.Color;
    output.m_color = float4(color.rgb * color.a, color.a);
    return output;
}
