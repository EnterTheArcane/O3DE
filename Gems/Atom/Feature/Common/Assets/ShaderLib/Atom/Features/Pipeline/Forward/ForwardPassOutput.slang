/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

 

#include <Atom/Features/ShaderQualityOptions.slang>

#ifndef OUTPUT_DEPTH
#define OUTPUT_DEPTH 0
#endif

#ifndef OUTPUT_SUBSURFACE
#define OUTPUT_SUBSURFACE 0
#endif


#if UNIFIED_FORWARD_OUTPUT

public struct ForwardPassOutput
{
    public float4 m_color  : SV_Target0;

#if OUTPUT_DEPTH
    public float m_depth : SV_Depth;
#endif
};

#else

public struct ForwardPassOutput
{
    public float4 m_diffuseColor  : SV_Target0;     //!< RGB = Diffuse Lighting, A = Blend Alpha (for blended surfaces) OR A = special encoding of surfaceScatteringFactor, m_subsurfaceScatteringQuality, o_enableSubsurfaceScattering
    public float4 m_specularColor : SV_Target1;     //!< RGB = Specular Lighting, A = Unused
    public float4 m_albedo : SV_Target2;            //!< RGB = Surface albedo pre-multiplied by other factors that will be multiplied later by diffuse GI, A = specularOcclusion
    public float4 m_specularF0 : SV_Target3;        //!< RGB = Specular F0, A = roughness
    public float4 m_normal : SV_Target4;            //!< RGB10 = EncodeNormalSignedOctahedron(worldNormal), A2 = Flags (IBL/Multiscatter enabled)

#if OUTPUT_SUBSURFACE
    public float3 m_scatterDistance : SV_Target5;
#endif

#if OUTPUT_DEPTH
    public float m_depth : SV_Depth;
#endif
};

#endif
