/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



// Note: Since we define the parameters as members of the SRG, we can't use this material in a deferred or raytracing pipeline.
// But since the AutoBrick.materialtype specifies only the AutoBrick_ForwardPass.shader, that wouldn't have worked anyway
[AtomShaderResourceGroup(SrgBindingSlot.Material)]
public struct AutoBrickSrgLayout
{
    // Shape...
    public float m_brickWidth;
    public float m_brickHeight;
    public float m_lineWidth;
    public float m_lineDepth;
    public float m_brickOffset;

    // Appearance...
    public float3 m_brickColor;
    public float m_brickNoiseFactor;
    public float3 m_lineColor;
    public float m_lineNoiseFactor;
    public float m_brickColorBleed;
    public float m_aoFactor;
    public float m_debugValue;
    public float m_debugValueScale;

    [AtomStaticSampler("Linear", "Linear", "Linear", "Wrap", "Wrap", "Wrap", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_sampler;
    public Texture2D m_noise;
};
public ParameterBlock<AutoBrickSrgLayout> AutoBrickSrg;
