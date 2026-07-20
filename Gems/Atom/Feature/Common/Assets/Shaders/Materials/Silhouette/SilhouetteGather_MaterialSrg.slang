/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


__exported import "../MaterialInputs/SilhouetteInput.slang";

public struct MaterialParameters {
    public float4 m_color;
    public float m_alpha;
    public float m_depthBias;
    public float m_outlineSize;
    public float m_padding0;
    public float4 m_uvMatrixRow0;
    public float4 m_uvMatrixRow1;
    public float4 m_uvMatrixRow2;
};

[AtomShaderResourceGroup(SrgBindingSlot.Material)]
public struct MaterialSrgLayout
{
    public MaterialParameters m_params;
};
public ParameterBlock<MaterialSrgLayout> MaterialSrg;

public const MaterialParameters GetMaterialParameters()
{
    MaterialParameters params = MaterialSrg.m_params;
    // m_uvMatrixRow0/1/2 are read directly from the SRG (no float3x3 copy needed).
    return params;
}

