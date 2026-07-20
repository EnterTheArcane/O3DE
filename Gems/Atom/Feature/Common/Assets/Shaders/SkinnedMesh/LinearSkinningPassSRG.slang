/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
public struct PassSrgLayout
{
    public RWStructuredBuffer<float> m_skinnedMeshOutputStream;    
};
public ParameterBlock<PassSrgLayout> PassSrg;

[AtomShaderResourceGroup(SrgBindingSlot.Draw)]
public struct InstanceSrgLayout
{    
    public uint m_numVertices;
    public uint m_numInfluencesPerVertex;
    public uint m_totalNumberOfThreadsX;

    // Per-model input
    // Positions, normals, and bitangents are all 3-component per-vertex buffers,
    // but Metal doesn't support float3 buffers so Buffer<float> is used instead
    public Buffer<float> m_sourcePositions; // POSITION 0
    public Buffer<float> m_sourceNormals; // NORMAL 0
    public Buffer<float4> m_sourceTangents; // TANGENT 0
    public Buffer<float> m_sourceBiTangents; // BITANGENT 0
    public ByteAddressBuffer m_sourceBlendIndices; // BLENDINDICES 0
    public Buffer<float> m_sourceBlendWeights; // BLENDWEIGHTS 0

    // Optional color input, if colors are being morphed by morph targets
    public Buffer<float4> m_sourceColors; // COLOR 0
    
    // Per-instance input
    public StructuredBuffer<float3x4> m_boneTransformsLinear;
    public StructuredBuffer<float2x4> m_boneTransformsDualQuaternion;

    // Per-instance morph target input
    // Offsets to the locations in the accumulation buffer that hold
    // the sum of all deltas for vertex 0. Each thread can further offset into the buffer
    // using the thread id to find the delta for the vertex the thread is working on.
    public uint m_morphTargetPositionDeltaOffset;
    public uint m_morphTargetNormalDeltaOffset;
    public uint m_morphTargetTangentDeltaOffset;
    public uint m_morphTargetBitangentDeltaOffset;
    public uint m_morphTargetColorDeltaOffset;

    // Morph target deltas are stored as signed integers in order to make use of
    // InterlockedAdd to accumulate them. They are multiplied by the integer
    // encoding when encoding from float->int, and must be multiplied
    // by the inverse when decoding from int->float
    public float m_morphTargetDeltaInverseIntegerEncoding;
    
    // Per-instance output
    // Offsets to the locations in the output stream with the skinning result
    // for vertex 0. Each thread can further offset into the buffer using
    // the thread id to find the output location for the vertex the thread is working on.
    public uint m_targetPositions;
    public uint m_targetNormals;
    public uint m_targetTangents;
    public uint m_targetBiTangents;

    // Offset to the location in the output stream with the start of the position history
    // for the particular mesh the dispatch is operating on
    public uint m_targetPositionHistory;

    // Optional color output, if colors are being morphed by morph targets
    public uint m_targetColors;
};
public ParameterBlock<InstanceSrgLayout> InstanceSrg;
