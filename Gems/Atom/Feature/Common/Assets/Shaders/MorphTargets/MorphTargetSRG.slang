/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
public struct MorphTargetPassSrgLayout
{
    //Since we do Interlocked atomic operations on this buffer it can not be RWBuffer due to broken MetalSL generation.
    //It stems from the fact that typed buffers gets converted to textures and that breaks with atomic operations.
    //In future we can handle this under the hood via our metal shader pipeline
    public RWStructuredBuffer<int> m_accumulatedDeltas;    
};
public ParameterBlock<MorphTargetPassSrgLayout> MorphTargetPassSrg;

// This class represents the data that is passed to the morph target compute shader of an individual delta
// See MorphTargetDelta.h for the corresponding cpu struct
public struct MorphTargetDelta
{
    // The index of the vertex being modified by this delta
    public uint m_morphedVertexIndex;
    // 16 bits per component for position deltas
    public uint m_compressedPositionDeltaXY;
    // Position z plus 8 bits per component for normal deltas
    public uint m_compressedPositionDeltaZNormalDeltaXY;
    // Normal z plus 8 bits per component for tangent deltas
    public uint m_compressedNormalDeltaZTangentDelta;
    // 8 bit padding plus 8 bits per component for bitangent deltas
    public uint m_compressedPadBitangentDeltaXYZ;
    // Extra padding so the struct is 16 byte aligned for structured buffers
    public uint3 m_pad;
};

// Input to the morph target compute shader
[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct MorphTargetInstanceSrgLayout
{
    public StructuredBuffer<MorphTargetDelta> m_vertexDeltas;
};
public ParameterBlock<MorphTargetInstanceSrgLayout> MorphTargetInstanceSrg;
