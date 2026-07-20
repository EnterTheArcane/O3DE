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
#define Surface     SurfaceData_BasePBR
#endif

__exported import Atom.Features.PBR.MaterialUtils;
#include <Atom/Features/PBR/SpecularAA.slang>

public struct SurfaceData_BasePBR
{   

    public uint lightingChannels;     //!< Lightingchannel of the object

    public precise float3 position;   //!< Position in world-space
    public real3 normal;              //!< Normal in world-space
    public real3 vertexNormal;        //!< Vertex normal in world-space
    public real3 baseColor;           //!< Surface base color
    public real metallic;             //!< Surface metallic property
    public real roughnessLinear;      //!< Perceptually linear roughness value authored by artists. Must be remapped to roughnessA before use
    public real roughnessA;           //!< Actual roughness value ( a.k.a. "alpha roughness") to be used in microfacet calculations
    public float roughnessA2;         //!< Alpha roughness ^ 2 (i.e. roughnessA * roughnessA), used in GGX, cached here for perfromance. Float precision is needed as values can get very close to 0

    //! Surface lighting data
    public real3 albedo;              //!< Albedo color of the non-metallic material, will be multiplied against the diffuse lighting value
    public real3 specularF0;          //!< Fresnel f0 spectral value of the surface

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

        albedo = GetAlbedo(baseColor, metallic);
        specularF0 = CalculateSpecularF0(baseColor, metallic, specularF0Factor);
    }

    public real3 GetDiffuseNormal() { return normal; }
    public real3 GetSpecularNormal() { return normal; }
    public real3 GetDefaultNormal() { return normal; }
    public real3 GetVertexNormal() { return vertexNormal; }
    public real3 GetSpecularF0() { return specularF0; }
};




