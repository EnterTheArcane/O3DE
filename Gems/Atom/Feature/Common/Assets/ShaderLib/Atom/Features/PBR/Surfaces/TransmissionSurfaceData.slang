/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#if ENABLE_TRANSMISSION

public struct TransmissionSurfaceData
{
    public real3 tint;
    public real thickness;                 //!< pre baked local thickness, used for transmission
    public real4 transmissionParams;       //!< parameters: thick mode->(attenuation coefficient, power, distortion, scale), thin mode:  (shrinkFactor, transmissionNdLBias, distanceAttenuation, scale)
    public real3 scatterDistance;          //!< scatter distance (same as in MaterialSrg) > 

    [mutating] public void InitializeToZero()
    {
        tint = real3(0.0, 0.0, 0.0);
        thickness = 0.0f;
        transmissionParams = real4(0.0, 0.0, 0.0, 0.0);
        scatterDistance = real3(0.0, 0.0, 0.0);
    }

    // ----- Thick mode getters -----

    public real GetAttenuationCoefficient() { return transmissionParams.x; }
    public real GetPower() { return transmissionParams.y; }
    public real GetDistortion() { return transmissionParams.z; }

    // ----- Thin mode getters -----

    // Shrink (absolute) offset towards the normal opposite direction to ensure correct shadow map projection
    public real GetShrinkFactor() { return transmissionParams.x; }

    // (N . L) to accept below (N . L = 0) in scattering through thin objects
    public real GetTransmissionNdLBias() { return transmissionParams.y; }

    // Attenuation applied to hide artifacts due to low-res shadow maps 
    public real GetDistanceAttenuation() { return transmissionParams.z; }

    // ----- Common getters -----

    public real GetScale() { return transmissionParams.w; }
};



#endif
