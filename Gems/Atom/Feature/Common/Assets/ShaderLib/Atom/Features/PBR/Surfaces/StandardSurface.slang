/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#ifndef ENABLE_CLEAR_COAT
#define ENABLE_CLEAR_COAT               1
#endif

#ifndef ENABLE_SPECULARAA
#define ENABLE_SPECULARAA               1
#endif

__exported import Atom.Features.PBR.MaterialUtils;
#include <Atom/Features/PBR/SpecularAA.slang>
__exported import Atom.Features.PBR.Surfaces.ClearCoatSurfaceData;
__exported import Atom.Features.PBR.Surfaces.TransmissionSurfaceData;

#if ENABLE_CLEAR_COAT
__exported import Atom.Features.PBR.Surfaces.ClearCoatSurfaceData;
#endif

// This data varies across different surfaces, but is uniform within a surface
public struct SurfaceSettings
{
    // Increase opacity at grazing angles for surfaces with a low m_opacityAffectsSpecularFactor.
    // For m_opacityAffectsSpecularFactor values close to 0, that indicates a transparent surface
    // like glass, so it becomes less transparent at grazing angles. For m_opacityAffectsSpecularFactor
    // values close to 1.0, that indicates the absence of a surface entirely, so this effect should
    // not apply.
    public float opacityAffectsSpecularFactor;
};

public struct Surface
{

#if ENABLE_CLEAR_COAT
    public ClearCoatSurfaceData clearCoat;
#endif

    // ------- Lighting Channels -------

    // from the object
    public uint lightingChannels;

    // ------- BasePbrSurfaceData -------
    
    public precise real3 position;    //!< Position in world-space
    public real3 normal;              //!< Normal in world-space
    public real3 vertexNormal;        //!< Vertex normal in world-space
    public real3 baseColor;           //!< Surface base color
    public real metallic;             //!< Surface metallic property
    public real roughnessLinear;      //!< Perceptually linear roughness value authored by artists. Must be remapped to roughnessA before use
    public real roughnessA;           //!< Actual roughness value ( a.k.a. "alpha roughness") to be used in microfacet calculations
    public float roughnessA2;         //!< Alpha roughness ^ 2 (i.e. roughnessA * roughnessA), used in GGX, cached here for perfromance. Float precision is needed as values can get very close to 0

    public float opacity;

    // Increase opacity at grazing angles for surfaces with a low m_opacityAffectsSpecularFactor.
    // For m_opacityAffectsSpecularFactor values close to 0, that indicates a transparent surface
    // like glass, so it becomes less transparent at grazing angles. For m_opacityAffectsSpecularFactor
    // values close to 1.0, that indicates the absence of a surface entirely, so this effect should
    // not apply.
    public real opacityAffectsSpecularFactor;

    public float opacityAffectsEmissiveFactor; //!< Allows emissive to be attenuated by alpha or not.

    //! Surface lighting data
    public real3 albedo;              //!< Albedo color of the non-metallic material, will be multiplied against the diffuse lighting value
    public real3 specularF0;          //!< Fresnel f0 spectral value of the surface
    public real3 emissiveLighting;    //!< Emissive lighting
    public real diffuseAmbientOcclusion; //!< Diffuse ambient occlusion factor - [0, 1] :: [Dark, Bright]
    public real specularOcclusion;       //!< Specular occlusion factor - [0, 1] :: [Dark, Bright]

    //! Calculates roughnessA and roughnessA2 after roughness has been set
    public void CalculateRoughnessA()
    {
        CalculateRoughnessValues(normal, roughnessLinear, roughnessA, roughnessA2);
    }

    //! Sets albedo, base color, specularF0, and metallic properties using metallic workflow
    [mutating] public void SetAlbedoAndSpecularF0(real3 newBaseColor, real specularF0Factor, real newMetallic)
    {
        baseColor = newBaseColor;
        metallic = newMetallic;

        real3 dielectricSpecularF0 = MaxDielectricSpecularF0 * specularF0Factor;

        // Compute albedo and specularF0 based on metalness
        albedo = lerp(baseColor, real3(0.0, 0.0, 0.0), metallic);
        specularF0 = lerp(dielectricSpecularF0, baseColor, metallic);
    }

    public real3 GetDiffuseNormal() { return normal; }         // Returns the normal used for diffuse  lighting calculations
    public real3 GetSpecularNormal() { return normal; }        // Returns the normal used for specular lighting calculations
    public real3 GetDefaultNormal() { return normal; }         // Returns the normal used for calculations that are common to both diffuse and specular calculations
                                                        // If you're doing something unusual with one of the normals, you probably want to return the other one here
                                                        // Search the code for 'GetDefaultNormal' to see how/where this is applied
    public real3 GetVertexNormal() { return vertexNormal; }    // Returns the interpolated vertex normal
    public real3 GetSpecularF0() { return specularF0; }
};




