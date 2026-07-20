/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


[AtomOption(false)]
public extern bool o_enableAnisotropy();

// ------- Surface Data -------

public struct AnisotropicSurfaceData
{
    public real3 tangent;                 //!< surface space tangent for anisotropic use
    public real3 bitangent;               //!< surface space bitangent for anisotropic use
    public real2 anisotropyFactors;       //!< anisotory factors along the tangent and the bitangent directions

    [mutating] public void Init(real3 normal, real3 vtxTangent, real3 vtxBitangent, real anisotropyAngle, real anisotropyFactor, real roughnessA)
    {
        anisotropyFactors = max( 0.01,
            real2( roughnessA * (1.0 + anisotropyFactor), 
                   roughnessA * (1.0 - anisotropyFactor) )
        );

        if (anisotropyAngle > 0.01)
        {
            // Base rotation according to anisotropic main direction
            real aniSin, aniCos;
            sincos(anisotropyAngle, aniSin, aniCos);

            // Rotate the vertex tangent to get new aligned to surface normal tangent
            vtxTangent = aniCos * vtxTangent - aniSin * vtxBitangent;
        }

        // Now create the new surface base according to the surface normal 
        // If rotation was required it was already applied to the tangent, hence to the bitangent
        bitangent = normalize(cross(normal, vtxTangent));
        tangent = cross(bitangent, normal);
    }
};

// ------- Functions -------

//! Notice that since the newly created surface tangent and bitangent will be rotated according
//! to the anisotropy direction and should not be used for other purposes unless rotated back.

