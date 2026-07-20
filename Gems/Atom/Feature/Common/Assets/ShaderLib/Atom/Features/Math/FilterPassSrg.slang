/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import Atom.Features.Shadow.Shadow;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
public struct FilterPassSrgLayout
{
    // This shader filters multiple images with distinct filter parameters.
    // So, the input and output are arrays of texture2Ds.
    public Texture2DArray<float> m_inputImage;
    public RWTexture2DArray<float> m_outputImage;

    // This can convert a coordinate in an atlas to
    // the shadowmap index.
    public Buffer<uint2> m_shadowmapIndexTable;

    // This contains parameters related to filtering.
    public StructuredBuffer<FilterParameter> m_filterParameters;
    
    // This is the array of weight tables associated to the math filters
    // used in this shader.
    public Buffer<float> m_filterTable;

    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_sampler;
};
public ParameterBlock<FilterPassSrgLayout> FilterPassSrg;
