/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PBR.Lights.LightTypesCommon;

#ifndef SimplePointLightUtil
#define SimplePointLightUtil SimplePointLightUtil_PBR
#endif


public struct SimplePointLightUtil_PBR
{
    public real3 m_surfaceToLightDirection;
    public real  m_lightDistanceSquared;
    public real  m_falloff;

    public static SimplePointLightUtil_PBR Init(SimplePointLight light, Surface surface)
    {
        SimplePointLightUtil_PBR result;

        real3 posToLight = real3(light.m_position - surface.position);
        result.m_lightDistanceSquared = dot(posToLight, posToLight); // light distance squared
        result.m_falloff = result.m_lightDistanceSquared * real(light.m_invAttenuationRadiusSquared);
        result.m_surfaceToLightDirection = normalize(posToLight);
        return result;
    }

    public real3 GetSurfaceToLightDirection()
    {
        return m_surfaceToLightDirection;
    }

    public real GetFalloff()
    {
        return m_falloff;
    }

    [mutating] public void Apply(SimplePointLight light, Surface surface, real litRatio, inout LightingData lightingData)
    {
        // Smoothly adjusts the light intensity so it reaches 0 at light.m_attenuationRadius distance
        real radiusAttenuation = 1.0 - (m_falloff * m_falloff);
        radiusAttenuation = radiusAttenuation * radiusAttenuation;

        // Standard quadratic m_falloff
        m_lightDistanceSquared = max(0.001 * 0.001, m_lightDistanceSquared); // clamp the light to at least 1mm away to avoid extreme values.
        real3 lightIntensity = (real3(light.m_rgbIntensityCandelas) / m_lightDistanceSquared) * radiusAttenuation;

        // Diffuse contribution
        lightingData.diffuseLighting += GetDiffuseLighting(surface, lightingData, lightIntensity, m_surfaceToLightDirection) * litRatio;

        // Calculate specular lighting for each view
        [unroll]
        for (uint viewIndex = 0; viewIndex < GET_SHADING_VIEW_COUNT; ++viewIndex)
        {
            // Specular contribution
            lightingData.specularLighting[viewIndex] += GetSpecularLighting(surface, lightingData, lightIntensity, m_surfaceToLightDirection, viewIndex) * litRatio;
        }
    }
};

