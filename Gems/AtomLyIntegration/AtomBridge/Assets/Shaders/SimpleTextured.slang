/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
 
 
// Indicates whether the sampler should use Wrap or Clamp
[AtomOption]
public extern bool o_clamp();
 
// Indicates whether to use color channels from the texture or only the alpha channel
[AtomOption]
public extern bool o_useColorChannels();

[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
struct InstanceSrgLayout
{
    row_major float4x4 m_worldToProj;
    Texture2D m_texture;    
    
    [AtomStaticSampler("Point", "Point", "Point", "Wrap", "Wrap", "Wrap", 16, "Never", "Filter", "TransparentBlack")]
    SamplerState m_wrapSampler;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 16, "Never", "Filter", "TransparentBlack")]
    SamplerState m_clampSampler;
};
ParameterBlock<InstanceSrgLayout> InstanceSrg;

struct VSInput
{
    float3 m_position : POSITION;
    float4 m_color : COLOR0;
    float2 m_uv : TEXCOORD0;
};

struct VSOutput
{
    float4 m_position : SV_Position;
    float4 m_color : COLOR0;
    float2 m_uv : TEXCOORD0;
};

VSOutput MainVS(VSInput IN)
{
    float4x4 worldToProj = InstanceSrg.m_worldToProj;
    float4 posPS = mul(worldToProj, float4(IN.m_position, 1.0f));

    VSOutput OUT;
    OUT.m_position = posPS;
    OUT.m_color = IN.m_color;
    OUT.m_uv = IN.m_uv;
    return OUT;
};

struct PSOutput
{
    float4 m_color : SV_Target0;
};

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    float4 tex;
    
    if (o_clamp())
    {
        tex = InstanceSrg.m_texture.Sample(InstanceSrg.m_clampSampler, IN.m_uv);
    }
    else
    {
        tex = InstanceSrg.m_texture.Sample(InstanceSrg.m_wrapSampler, IN.m_uv);
    }

    if (!o_useColorChannels())
    {
        // When getting rgba from an R8 the "r" channel will be the value from the texture.
        // We want to put the r in the alpha (to use as opacity) and set the rgb to 1.
        // We do this rather than using an A8 texture because A8 is not supported on Vulkan.
        tex.a = tex.r;
        tex.rgb = 1.0f;
    }   

    float opacity = IN.m_color.a * tex.a; 

    // We use pre-multiplied alpha here since it is more flexible. For example, it enables alpha-blended rendering to
    // a render target and then alpha blending that render target into another render target
    OUT.m_color.rgb = IN.m_color.rgb * tex.rgb * opacity;

    OUT.m_color.a = opacity;
    return OUT;
};
