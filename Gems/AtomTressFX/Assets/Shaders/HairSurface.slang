/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PBR.MaterialUtils;
#include <Atom/Features/PBR/SpecularAA.slang>
__exported import Atom.Features.PBR.Surfaces.TransmissionSurfaceData;

public struct Surface
{
#if ENABLE_TRANSMISSION
    public TransmissionSurfaceData transmission;
#endif

    public uint lightingChannels;     //!< Lightingchannel of the object

    // ------- BasePbrSurfaceData -------
    public float3 position;            //!< Position in world-space
    public float3 normal;              //!< Normal in world-space
    public float3 vertexNormal;        //!< Vertex normal in world-space
    public float3 albedo;                //!< Albedo color of the non-metallic material, will be multiplied against the diffuse lighting value
    public float3 specularF0;            //!< Fresnel f0 spectral value of the surface
    public float roughnessLinear;      //!< Perceptually linear roughness value authored by artists. Must be remapped to roughnessA before use
    public float roughnessA;           //!< Actual roughness value ( a.k.a. "alpha roughness") to be used in microfacet calculations
    public float roughnessA2;          //!< Alpha roughness ^ 2 (i.e. roughnessA * roughnessA), used in GGX, cached here for perfromance

    // ------- Hair Surface Data -------
    public float3 tangent;
    public float cuticleTilt;
    public float thickness;


    //! Calculates roughnessA and roughnessA2 after roughness has been set
    public void CalculateRoughnessA()
    {
        CalculateRoughnessValues(normal, roughnessLinear, roughnessA, roughnessA2);
    }

    //! Sets albedo and specularF0 using metallic workflow
    [mutating] public void SetAlbedoAndSpecularF0(float3 baseColor, float specularF0Factor)
    {
        albedo = baseColor;
        specularF0 = MaxDielectricSpecularF0 * specularF0Factor;
    }

    public real3 GetDiffuseNormal() { return normal; }
    public real3 GetSpecularNormal() { return normal; }
    public real3 GetDefaultNormal() { return normal; }
    public real3 GetVertexNormal() { return vertexNormal; }
    public real3 GetSpecularF0() { return specularF0; }
};




