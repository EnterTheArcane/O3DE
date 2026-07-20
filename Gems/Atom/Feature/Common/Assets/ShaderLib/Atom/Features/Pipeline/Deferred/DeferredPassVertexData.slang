/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// This #define magic lets you use the VsInput & VsOutput classes in this file without making it the final VsInput & VsOutput
// used in your shader. Simply #define VsInput & VsOutput to your custom definitions before including this file
//

#ifndef VsInput
#define VsInput         VsInput_DeferredPass
#endif

#ifndef VsSystemValues
#define VsSystemValues  VsSystemValues_DeferredPass
#endif

#ifndef VsOutput
#define VsOutput        VsOutput_DeferredPass
#endif

#ifndef MATERIAL_USES_VERTEX_POSITION
#define MATERIAL_USES_VERTEX_POSITION 0
#endif

#ifndef MATERIAL_USES_VERTEX_POSITIONWS
#define MATERIAL_USES_VERTEX_POSITIONWS 0
#endif

#ifndef MATERIAL_USES_VERTEX_LOCALPOSITION
#define MATERIAL_USES_VERTEX_LOCALPOSITION 0
#endif

#ifndef MATERIAL_USES_VERTEX_PREV_POSITION
#define MATERIAL_USES_VERTEX_PREV_POSITION 0
#endif

#ifndef MATERIAL_USES_VERTEX_NORMAL
#define MATERIAL_USES_VERTEX_NORMAL 0
#endif

#ifndef MATERIAL_USES_VERTEX_TANGENT
#define MATERIAL_USES_VERTEX_TANGENT 0
#endif

#ifndef MATERIAL_USES_VERTEX_BITANGENT
#define MATERIAL_USES_VERTEX_BITANGENT 0
#endif

#ifndef MATERIAL_USES_VERTEX_UV
#define MATERIAL_USES_VERTEX_UV 0
#endif

#ifndef MATERIAL_USES_VERTEX_DETAIL_UV
#define MATERIAL_USES_VERTEX_DETAIL_UV 0
#endif

#ifndef MATERIAL_USES_VERTEX_COLOR
#define MATERIAL_USES_VERTEX_COLOR 0
#endif

#ifndef MATERIAL_USES_VERTEX_BLENDMASK 
#define MATERIAL_USES_VERTEX_BLENDMASK 0
#endif

#if MATERIAL_USES_VERTEX_COLOR && MATERIAL_USES_VERTEX_BLENDMASK
#error("Only one of MATERIAL_USES_VERTEX_BLENDMASK and MATERIAL_USES_VERTEX_COLOR can be active at once")
#endif


// These options indicate whether the vertex input struct's "m_optional_xxx" is bound. If false, it is not safe to read from m_optional_xxx.
// This option gets set automatically by the system at runtime; there is a soft naming convention that associates it with m_optional_xx.
// (search "m_optional_" in ShaderVariantAssetBuilder for details on the naming convention).
// [GFX TODO][ATOM-14475]: Come up with a more elegant way to associate the isBound flag with the input stream.

#if MATERIAL_USES_VERTEX_COLOR
    [AtomOption(false)]
public extern bool o_color0_isBound();
#endif
#if MATERIAL_USES_VERTEX_BLENDMASK
    [AtomOption(false)]
public extern bool o_blendMask_isBound();
#endif
#if MATERIAL_USES_VERTEX_PREV_POSITION
    [AtomOption(false)]
public extern bool o_prevPosition_isBound();
#endif

// we are running in a FullscreenTriangle-Pass, so our VertexShader - Input can't come from the Input-Assembly, 
// and cant't have semantics
public struct VsInput_DeferredPass
{
    // Base fields (required by the template azsli file)...
#if MATERIAL_USES_VERTEX_POSITION
    public float3 position;
#endif

#if MATERIAL_USES_VERTEX_NORMAL
    public float3 normal;
#endif
#if MATERIAL_USES_VERTEX_TANGENT
    public float4 tangent;
#endif
#if MATERIAL_USES_VERTEX_BITANGENT
    public float3 bitangent;
#endif

#if MATERIAL_USES_VERTEX_UV
    public float2 uv0;
    public float2 uv1;
#endif

#if MATERIAL_USES_VERTEX_PREV_POSITION
    public float3 m_optional_prevPosition;
#endif

#if MATERIAL_USES_VERTEX_COLOR
    // This gets set automatically by the system at runtime only if it's available.
    // There is a soft naming convention that associates this with o_color0_isBound, which will be set to true whenever m_optional_color0 is available.
    public float4 m_optional_color0;
#endif

#if MATERIAL_USES_VERTEX_BLENDMASK
    // This gets set automatically by the system at runtime only if it's available.
    // There is a soft naming convention that associates this with o_blendMask_isBound, which will be set to true whenever m_optional_blendMask is available.
    public float4 m_optional_blendMask;
#endif

};

// We don't have an Object or Draw-SRG, so we place what we need in this struct
public struct VsSystemValues_DeferredPass
{
    public uint m_instanceId;
    public uint m_lightingChannelMask;
    public uint m_objectId;
    public uint m_uvStreamTangentBitmask;
};

// we are running in a FullscreenTriangle-Pass, so our VertexShader-Output doesn't need semantics
public struct VsOutput_DeferredPass
{
    // Base fields (required by the template azsli file)...
#if MATERIAL_USES_VERTEX_POSITION
    public float4 position;
#endif

#if MATERIAL_USES_VERTEX_NORMAL
    public float3 normal;
#endif 

#if MATERIAL_USES_VERTEX_TANGENT
    public float4 tangent;
#endif

#if MATERIAL_USES_VERTEX_BITANGENT
    public float3 bitangent;
#endif

#if MATERIAL_USES_VERTEX_UV
    public float2 uvs[UvSetCount];
#endif

#if MATERIAL_USES_VERTEX_DETAIL_UV
    public float2 detailUvs[UvSetCount];
#endif

#if MATERIAL_USES_VERTEX_POSITIONWS
    public float3 worldPosition;
#endif

#if MATERIAL_USES_VERTEX_LOCALPOSITION
    public float3 localPosition;
#endif

#if MATERIAL_USES_VERTEX_PREV_POSITION
    public float3 worldPosPrev;
#endif

#if MATERIAL_USES_VERTEX_COLOR
    public float4 color0;
#endif

#if MATERIAL_USES_VERTEX_BLENDMASK
    public float4 blendFactors;
#endif

};
