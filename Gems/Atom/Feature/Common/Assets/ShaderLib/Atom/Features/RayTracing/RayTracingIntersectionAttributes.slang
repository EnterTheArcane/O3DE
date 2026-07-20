/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

__exported import Atom.RPI.Math;


#ifdef NO_FLOAT_16

public uint PackNormalToLower30BitsOfUint(float3 normal)
{
    float3 encodedNormal = EncodeNormalSignedOctahedron(normal);
    return
        ((uint)(encodedNormal.x * 1023.f) & 0x3FF) << 20 |
        ((uint)(encodedNormal.y * 1023.f) & 0x3FF) << 10 |
        ((uint)(encodedNormal.z * 1023.f) & 0x3FF) <<  0;
}

public float3 UnpackNormalFromLower30BitsOfUint(uint packedNormal)
{
    float3 encodedNormal = float3(
        (float)((packedNormal >> 20) & 0x3FF) / 1023.f,
        (float)((packedNormal >> 10) & 0x3FF) / 1023.f,
        (float)((packedNormal >>  0) & 0x3FF) / 1023.f
    );
    return DecodeNormalSignedOctahedron(encodedNormal);
}

public uint PackTangentToLower31BitsOfUint(float4 tangent)
{
    return PackNormalToLower30BitsOfUint(tangent.xyz) | (tangent.w > 0 ? 1 : 0) << 30;
}

public float4 UnpackTangentFromLower31BitsOfUint(uint packedTangent)
{
    float3 tangent = UnpackNormalFromLower30BitsOfUint(packedTangent);
    return float4(tangent, (packedTangent >> 30) ? 1.f : -1.f);
}

// This structure must not be larger than 32 bytes (D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES)
public struct ProceduralGeometryIntersectionAttributes
{
    public float3 m_position;    // [12 bytes] Surface hit point in object space (BLAS space)
    public float2 m_uv;          // [ 8 bytes] Surface uv coordinates
    public uint m_packedNormal;  // [ 4 bytes] Surface normal in object space (BLAS space)
    public uint m_packedTangent; // [ 4 bytes] Surface tangent in object space (BLAS space)
    //                   sum: 28 bytes

    public float3 GetPosition() { return m_position; }
    public float2 GetUv()       { return m_uv; }
    public float3 GetNormal()   { return UnpackNormalFromLower30BitsOfUint(m_packedNormal); }
    public float4 GetTangent()  { return UnpackTangentFromLower31BitsOfUint(m_packedTangent); }
    [mutating] public void SetPosition(float3 position) { m_position = position; }
    [mutating] public void SetUv      (float2 uv)       { m_uv = uv; }
    [mutating] public void SetNormal  (float3 normal)   { m_packedNormal = PackNormalToLower30BitsOfUint(normal); }
    [mutating] public void SetTangent (float4 tangent)  { m_packedTangent = PackTangentToLower31BitsOfUint(tangent); }
};

#else

// This structure must not be larger than 32 bytes (D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES)
public struct ProceduralGeometryIntersectionAttributes
{
    public float3 m_position; // [12 bytes] Surface hit point in object space (BLAS space)
    public half3 m_normal;    // [ 6 bytes] Surface normal in object space (BLAS space)
    public half2 m_uv;        // [ 4 bytes] Surface uv coordinates
    public half4 m_tangent;   // [ 8 bytes] Surface tangent in object space (BLAS space)
    //                sum: 30 bytes

    public float3 GetPosition() { return m_position; }
    public float2 GetUv()       { return float2(m_uv); }
    public float3 GetNormal()   { return float3(m_normal); }
    public float4 GetTangent()  { return float4(m_tangent); }
    [mutating] public void SetPosition(float3 position) { m_position = position; }
    [mutating] public void SetUv      (float2 uv)       { m_uv = half2(uv); }
    [mutating] public void SetNormal  (float3 normal)   { m_normal = half3(normal); }
    [mutating] public void SetTangent (float4 tangent)  { m_tangent = half4(tangent); }
};

#endif
