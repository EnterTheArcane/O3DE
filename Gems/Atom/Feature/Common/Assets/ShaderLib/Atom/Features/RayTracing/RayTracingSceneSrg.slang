/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.PBR.Lights.LightStructures;

[AtomShaderResourceGroup(SrgBindingSlot.Object)]
public struct RayTracingSceneSrgLayout
{
    public RaytracingAccelerationStructure m_scene;

    public StructuredBuffer<DirectionalLight> m_directionalLights;
    public uint m_directionalLightCount;
    
    public StructuredBuffer<SimplePointLight> m_simplePointLights;
    public uint m_simplePointLightCount;

    public StructuredBuffer<SimpleSpotLight> m_simpleSpotLights;
    public uint m_simpleSpotLightCount;

    public StructuredBuffer<PointLight> m_pointLights;
    public uint m_pointLightCount;
    
    public StructuredBuffer<DiskLight> m_diskLights;
    public uint m_diskLightCount;
    
    public StructuredBuffer<CapsuleLight> m_capsuleLights;
    public uint m_capsuleLightCount;
    
    public StructuredBuffer<QuadLight> m_quadLights;
    public uint m_quadLightCount;
    
    // diffuse IBL
    [AtomStaticSampler("Linear", "Linear", "Linear", "Wrap", "Wrap", "Wrap", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_diffuseEnvMapSampler;
    
    [AtomStaticSampler("Linear", "Linear", "Linear", "Wrap", "Wrap", "Wrap", 16, "Never", "Filter", "TransparentBlack")]
    public SamplerState LinearSampler;

    public TextureCube m_diffuseEnvMap;
    public float4 m_iblOrientation;
    public float m_iblExposure;

    public uint m_blasMeshCount;

    // contains a mapping from RT InstanceIndex to procedural geometry instances, which can be used to read per-instance geometry data:
    // [0] bindlessBufferIndex parameter from RegisterProceduralGeometryType
    // [1] localInstanceIndex parameter from AddProceduralGeometry
    public Buffer<uint2> m_proceduralGeometryInfo;
    
    // Specifies which debug visualization to use (value must be from RayTracingDebugViewMode enum)
    public uint m_debugViewMode;
};
public ParameterBlock<RayTracingSceneSrgLayout> RayTracingSceneSrg;
