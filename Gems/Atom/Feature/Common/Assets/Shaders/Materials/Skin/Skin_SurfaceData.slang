/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// This #define magic lets you use the Surface class in this file without making it the final Surface
// used in your shader. Simply #define Surface to your custom definition before including this file
//
#ifndef Surface
#define Surface     SurfaceData_Skin
#endif

#include "../BasePBR/BasePBR_SurfaceData.slang"
__exported import Atom.Features.PBR.Surfaces.TransmissionSurfaceData;

public struct SurfaceData_Skin
{
#if ENABLE_TRANSMISSION
    public TransmissionSurfaceData transmission;
#endif

    // ------- BasePbrSurfaceData -------

    public uint lightingChannels;     //!< Lightingchannel of the object

    public float3 position;           //!< Position in world-space
    public real3 normal;              //!< Normal in world-space
    public real3 vertexNormal;        //!< Vertex normal in world-space
    public real3 albedo;              //!< Albedo color of the non-metallic material, will be multiplied against the diffuse lighting value
    public real specularF0;           //!< Fresnel f0 spectral value of the surface (only one float vs float3 since skin is non-metallic)
    public real roughnessLinear;      //!< Perceptually linear roughness value authored by artists. Must be remapped to roughnessA before use
    public real roughnessA;           //!< Actual roughness value ( a.k.a. "alpha roughness") to be used in microfacet calculations
    public float roughnessA2;         //!< Alpha roughness ^ 2 (i.e. roughnessA * roughnessA), used in GGX, cached here for perfromance. Float precision is needed as values can get very close to 0

    public real3 baseColor;
    public real metallic;

    public real diffuseAmbientOcclusion; //!< Diffuse ambient occlusion factor - [0, 1] :: [Dark, Bright]
    public real specularOcclusion;       //!< Specular occlusion factor - [0, 1] :: [Dark, Bright]

    //! Dual specular parameters

    public real dualSpecFactor;
    public real dualSpecRoughness;
    public float dualSpecRoughnessA2;
    public real dualSpecF0;

    //! Subsurface scattering parameters
#if OUTPUT_SUBSURFACE
    public real subsurfaceScatteringFactor;
    public real subsurfaceScatteringQuality;
    public real3 scatterDistance;
#endif

    //! Calculates roughnessA and roughnessA2 after roughness has been set
    public void CalculateRoughnessA()
    {
        CalculateRoughnessValues(normal, roughnessLinear, roughnessA, roughnessA2);
    }

    //! Sets albedo and specularF0 (no metallic for skin)
    [mutating] public void SetAlbedoAndSpecularF0(real3 newBaseColor, real specularF0Factor)
    {
        baseColor = newBaseColor;
        albedo = newBaseColor;
        specularF0 = MaxDielectricSpecularF0.r * specularF0Factor;
        metallic = 0;
    }

    public real3 GetDiffuseNormal() { return normal; }       // Returns the normal used for diffuse  lighting calculations
    public real3 GetSpecularNormal() { return normal; }            // Returns the normal used for specular lighting calculations
    public real3 GetDefaultNormal() { return normal; }             // Returns the normal used for calculations that are common to both diffuse and specular calculations
                                                            // If you're doing something unusual with one of the normals, you probably want to return the other one here
                                                            // Search the code for 'GetDefaultNormal' to see how/where this is applied
    public real3 GetVertexNormal() { return vertexNormal; }        // Returns the interpolated vertex normal
    public real3 GetSpecularF0() { return specularF0.xxx; }
};




