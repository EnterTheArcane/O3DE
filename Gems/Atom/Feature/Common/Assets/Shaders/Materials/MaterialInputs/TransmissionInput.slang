/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


[AtomOption]
public extern bool o_transmission_useTexture();

public real4 GeTransmissionInput(Texture2D map, SamplerState mapSampler, float2 uv, real4 transmissionTintThickness, float4 uvDxDy = float4(0.0, 0.0, 0.0, 0.0), bool customDerivatives = false)
{
    if(o_transmission_useTexture())
    {
        if (customDerivatives)
        {
            transmissionTintThickness.w *= real(map.SampleGrad(mapSampler, uv, uvDxDy.xy, uvDxDy.zw).r);
        }
        else
        {
#ifdef CS_SAMPLERS
            transmissionTintThickness.w *= real(map.SampleLevel(mapSampler, uv, 0).r);
#else
            transmissionTintThickness.w *= real(map.Sample(mapSampler, uv).r);
#endif /* CS_SAMPLERS */
        }
    }
    return transmissionTintThickness;
}
