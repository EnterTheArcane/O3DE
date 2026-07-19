/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [slang-port] removed: #include <Atom/Features/SrgSemantics.azsli> has no Slang counterpart

#include <Atom/Features/PostProcessing/FullscreenPixelInfo.slangi>
#include <Atom/Features/PostProcessing/PostProcessUtil.slangi>
#include <Atom/Features/PostProcessing/FullscreenVertexInfo.slangi>
#include <Atom/Features/PostProcessing/FullscreenVertexUtil.slangi>

struct VSOutputSMAAEdgeDetection
{
    float4 m_position : SV_Position;
    float2 m_texCoord : TEXCOORD0;
    float4 m_offset[3] : TEXCOORD1;
};

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrgLayout
{
    Texture2D<float4> m_framebuffer;
    Texture2D<float4> m_depthTexture;
    
    // Information for render target.
    // x = 1.0 / width
    // y = 1.0 / height
    // z = width
    // w = height
    float4 m_renderTargetMetrics;
    // This is a threshold value for edge detection sensitivity. For details please check comment related to SMAA_THRESHOLD in SMAA.azsli.
    float m_chromaThreshold;
    // This is a threshold value for depth edge detection sensitivity. For details please check comment related to SMAA_DEPTH_THRESHOLD in SMAA.azsli.
    float m_depthThreshold;
    // This is a tweak value for the local contrast adaptation feature. For details please check comment related to SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR in SMAA.azsli.
    float m_localContrastAdaptationFactor;
    // This is a threshold value for predication feature. For details please check comment related to SMAA_PREDICATION_THRESHOLD in SMAA.azsli.
    float m_predicationThreshold;
    // This is a tweak value for predication feature. For details please check comment related to SMAA_PREDICATION_SCALE in SMAA.azsli.
    float m_predicationScale;
    // This is a tweak value for predication feature. For details please check comment related to SMAA_PREDICATION_STRENGTH in SMAA.azsli.
    float m_predicationStrength;

    [AtomStaticSampler("Linear", "Linear", "Linear", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState LinearSampler;
    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    SamplerState PointSampler;
};
ParameterBlock<PassSrgLayout> PassSrg;

// Option shader variable to enable predication feature. This option value will be used instead of SMAA_PREDICATION define in SMAA.azsli.
[AtomOption(false)]
public extern bool o_enablePredicationFeature();
// Option shader variable for the edge detection mode.
public enum EdgeDetectionMode
{
    Depth,
    Luma,
    Color,
};

[AtomOption(EdgeDetectionMode.Color)]
public extern EdgeDetectionMode o_edgeDetectionMode();

// This macro being defined before being included in SMAA.azsli, we can control which function is enabled in SMAA.azsli
// This macro is intended to enable the edge detection functions only.
#ifdef CS_SAMPLERS
    // When using Compute shaders, the "discard" intrinsic is not available (only valid for Fragment shaders).
    // ATOM_SMAA_EDGE_DETECTION_PASS_IMPLEMENTATION_ENABLE in "SMAA.azsli" uses "discard".
    #define ATOM_SMAA_BLENDING_WEIGHT_CALCULATION_PASS_IMPLEMENTATION_ENABLE
#else
    #define ATOM_SMAA_EDGE_DETECTION_PASS_IMPLEMENTATION_ENABLE
#endif

// Defining the elements below before including SMAA.azsli as they'll be used in it.
// - PassSrg SRG to be used for the edge detection implementation.
// - The option shader variables to be used for the edge detection implementation.
#include "SMAA.slangi"

VSOutputSMAAEdgeDetection MainVS(VSInput input)
{
    VSOutputSMAAEdgeDetection OUT;

    float4 posTex = GetVertexPositionAndTexCoords(input.m_vertexID);
    
    OUT.m_texCoord = float2(posTex.z, posTex.w);
    OUT.m_position = float4(posTex.x, posTex.y, 0.0, 1.0);
    
    SMAAEdgeDetectionVS(OUT.m_texCoord, OUT.m_offset);

    return OUT;
}

PSOutput MainPS(VSOutputSMAAEdgeDetection IN)
{
    PSOutput OUT = (PSOutput)0;
    switch (o_edgeDetectionMode())
    {
        case EdgeDetectionMode::Depth:
            OUT.m_color.xy = SMAADepthEdgeDetectionPS(IN.m_texCoord, IN.m_offset, PassSrg.m_depthTexture);
            break;
        case EdgeDetectionMode::Luma:
            OUT.m_color.xy = SMAALumaEdgeDetectionPS(IN.m_texCoord, IN.m_offset, PassSrg.m_framebuffer, PassSrg.m_depthTexture);
            break;
        case EdgeDetectionMode::Color:
            OUT.m_color.xy = SMAAColorEdgeDetectionPS(IN.m_texCoord, IN.m_offset, PassSrg.m_framebuffer, PassSrg.m_depthTexture);
            break;
    }
    
    return OUT;
}
