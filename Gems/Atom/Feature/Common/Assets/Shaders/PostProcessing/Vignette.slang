/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct PassSrg
{
    Texture2D<float4> m_inputColor;
    RWTexture2D<float4> m_outputColor;

    // Must match the struct in VignettePass.cpp
    struct Constants
    {
        uint2 m_outputSize; // texture size of output
        float2 m_outputCenter; // center of image in pixel coords
        float m_intensity; // strength of effect (0 to 1), i.e. displacement of channels
        float3 m_pad; // [[pad_to(16)]]
    };
    Constants m_constants;
};
ParameterBlock<PassSrg> passSrg;

[numthreads(8, 8, 1)]
void MainCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= passSrg.m_constants.m_outputSize.x || dispatchThreadID.y >= passSrg.m_constants.m_outputSize.y)
    {
        return;
    }

    // Displacement from center of screen as a ratio (methods that measure from edges tend to involve either a "square" appearance or extra calculations)
    float2 disp = (dispatchThreadID.xy - passSrg.m_constants.m_outputCenter) / passSrg.m_constants.m_outputCenter;

    // The dot product essentially gets us the square magnitude, which is cheaper than magnituded
    // Furthermore, we do not want a linear dropoff. By using sq mag this non linear feature is built in
    float vig = dot(disp, disp) * passSrg.m_constants.m_intensity;

    float3 output = mad(vig, -passSrg.m_inputColor[dispatchThreadID.xy].rgb, passSrg.m_inputColor[dispatchThreadID.xy].rgb);

    passSrg.m_outputColor[dispatchThreadID.xy] = float4(output.r, output.g, output.b, passSrg.m_inputColor[dispatchThreadID.xy].a);
}
