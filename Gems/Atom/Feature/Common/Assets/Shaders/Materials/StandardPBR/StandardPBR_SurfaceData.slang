/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

// This #define magic lets you use the Surface class in this file without making it the final Surface
// used in your shader. Simply #define Surface to your custom definition before including this file
//
#ifndef Surface
#define Surface     SurfaceData_StandardPBR
#endif

#include "../BasePBR/BasePBR_SurfaceData.slang"

__exported import Atom.Features.PBR.Surfaces.ClearCoatSurfaceData;

// Class inheritance doesn't seem to be working
#define DOES_CLASS_INHERITANCE_WORK_IN_ASZL 0
#if DOES_CLASS_INHERITANCE_WORK_IN_ASZL

    public struct SurfaceData_StandardPBR : SurfaceData_BasePBR
    {
    #if ENABLE_CLEAR_COAT
        public ClearCoatSurfaceData clearCoat;
    #endif

        public float3 emissiveLighting;        //!< Emissive lighting
        public float diffuseAmbientOcclusion;  //!< Diffuse ambient occlusion factor - [0, 1] :: [Dark, Bright]
        public float specularOcclusion;        //!< Specular occlusion factor - [0, 1] :: [Dark, Bright]
        public float alpha;

        // Increase opacity at grazing angles for surfaces with a low m_opacityAffectsSpecularFactor.
        // For m_opacityAffectsSpecularFactor values close to 0, that indicates a transparent surface like
        // glass, so it becomes less transparent at grazing angles. For m_opacityAffectsSpecularFactor values
        // close to 1.0, that indicates the absence of a surface entirely, so this effect should not apply.
        public float opacityAffectsSpecularFactor;
    };

#else

    public struct SurfaceData_StandardPBR
    {

        public uint lightingChannels;     //!< Lightingchannel of the object

    #if ENABLE_CLEAR_COAT
        public ClearCoatSurfaceData clearCoat;
    #endif

        // ------- BasePbrSurfaceData -------
    
        public precise float3 position;   //!< Position in world-space
        public real3 normal;              //!< Normal in world-space
        public real3 vertexNormal;        //!< Vertex normal in world-space
        public real3 baseColor;           //!< Surface base color
        public real metallic;             //!< Surface metallic property
        public real roughnessLinear;      //!< Perceptually linear roughness value authored by artists. Must be remapped to roughnessA before use
        public real roughnessA;           //!< Actual roughness value ( a.k.a. "alpha roughness") to be used in microfacet calculations
        public float roughnessA2;         //!< Alpha roughness ^ 2 (i.e. roughnessA * roughnessA), used in GGX, cached here for perfromance. Float precision is needed as values can get very close to 0
        public real alpha;

        // Increase opacity at grazing angles for surfaces with a low m_opacityAffectsSpecularFactor.
        // For m_opacityAffectsSpecularFactor values close to 0, that indicates a transparent surface like
        // glass, so it becomes less transparent at grazing angles. For m_opacityAffectsSpecularFactor values
        // close to 1.0, that indicates the absence of a surface entirely, so this effect should not apply.
        public real opacityAffectsSpecularFactor;

        //! Surface lighting data
        public real3 albedo;              //!< Albedo color of the non-metallic material, will be multiplied against the diffuse lighting value
        public real3 specularF0;          //!< Fresnel f0 spectral value of the surface
        public real3 emissiveLighting;    //!< Emissive lighting
        public real diffuseAmbientOcclusion; //!< Diffuse ambient occlusion factor - [0, 1] :: [Dark, Bright]
        public real specularOcclusion;       //!< Specular occlusion factor - [0, 1] :: [Dark, Bright]

        //! Calculates roughnessA and roughnessA2 after roughness has been set
        public void CalculateRoughnessA();

        //! Sets albedo, base color, specularF0, and metallic properties using metallic workflow
        public void SetAlbedoAndSpecularF0(real3 baseColor, real specularF0Factor, real metallic);

        public real3 GetDiffuseNormal() { return normal; }
        public real3 GetSpecularNormal() { return normal; }
        public real3 GetDefaultNormal() { return normal; }
        public real3 GetVertexNormal() { return vertexNormal; }
        public real3 GetSpecularF0() { return specularF0; }
    };

    public extension SurfaceData_StandardPBR
{
void CalculateRoughnessA()
    {
        CalculateRoughnessValues(normal, roughnessLinear, roughnessA, roughnessA2);
    }
}

    public extension SurfaceData_StandardPBR
{
void SetAlbedoAndSpecularF0(real3 newBaseColor, real specularF0Factor, real newMetallic)
    {
        baseColor = newBaseColor;
        metallic = newMetallic;

        albedo = GetAlbedo(baseColor, metallic);
        specularF0 = CalculateSpecularF0(baseColor, metallic, specularF0Factor);
    }
}
    
    public SurfaceData_StandardPBR MakeDebugSurface(float3 position, real3 normal, real3 color)
    {
        SurfaceData_StandardPBR output;
        output.position = position;
        output.normal = normal;
        output.vertexNormal = normal;
        output.baseColor = real3(0,0,0);
        output.metallic = 0;
        output.roughnessLinear = 1;
        output.roughnessA = 1;
        output.roughnessA2 = 1;
        output.alpha = 1;
        output.opacityAffectsSpecularFactor = 0;
        output.albedo = real3(0,0,0);
        output.specularF0 = 0;
        output.emissiveLighting = color;
        output.diffuseAmbientOcclusion = 1;
        output.specularOcclusion = 1;
        return output;
    }

#endif
