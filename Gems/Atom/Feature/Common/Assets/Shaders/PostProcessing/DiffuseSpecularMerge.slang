/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


import Atom.Features.PostProcessing.FullscreenPixelInfo;
import Atom.Features.PostProcessing.FullscreenVertex;
import Atom.Features.PostProcessing.PostProcessUtil;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_diffuseFrameBuffer;
    Texture2D<float4> m_specularFrameBuffer;
    
    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
};
ParameterBlock<PassSrgLayout> PassSrg;

PSOutput MainPS(VSOutput IN)
{
    PSOutput OUT;

    // 2x2 box filter to reduce noises introduced by undersampled subsurface scattering
    
    float4 diffuse = PassSrg.m_diffuseFrameBuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord);
    if(diffuse.w > 0.0)
    {
        // Center pixel always valid
        float3 sum = 0.25 * diffuse.rgb;
        float weightSum = 0.25;
        
        float3 color;
        float weight;
        
        // Up
        color = PassSrg.m_diffuseFrameBuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord, int2(0, -1)).rgb;
        weight = color.x + color.y + color.z > 0.0 ? 0.25 : 0.0;
        sum += weight * color;
        weightSum += weight;
            
        // Left
        color = PassSrg.m_diffuseFrameBuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord, int2(-1, 0)).rgb;
        weight = color.x + color.y + color.z > 0.0 ? 0.25 : 0.0;
        sum += weight * color;
        weightSum += weight;    

        // Upperleft
        color = PassSrg.m_diffuseFrameBuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord, int2(-1, -1)).rgb;
        weight = color.x + color.y + color.z > 0.0 ? 0.25 : 0.0;
        sum += weight * color;
        weightSum += weight;    

        diffuse = float4(sum / weightSum, 1.0);
    }

    float4 specular = PassSrg.m_specularFrameBuffer.Sample(PassSrg.LinearSampler, IN.m_texCoord);
    
    OUT.m_color = float4(diffuse.rgb, 1.0) + specular;

    return OUT;
}
