/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [slang-port] removed: #include <Atom/Features/SrgSemantics.azsli> has no Slang counterpart

[AtomShaderResourceGroup(SrgBindingSlot.Object)]
struct ObjectSrgLayout
{    
    Texture2DMS<float4> m_msImage;
    RWTexture2DArray<float4> m_outputImage;
};
ParameterBlock<ObjectSrgLayout> ObjectSrg;

#define ThreadGroupSize 16
[numthreads(ThreadGroupSize, ThreadGroupSize, 1)]
void MainCS(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint3 samplePos = groupID * ThreadGroupSize + groupThreadID;
    ObjectSrg.m_outputImage[samplePos] = ObjectSrg.m_msImage.Load(samplePos.xy, groupThreadID.z);
}
