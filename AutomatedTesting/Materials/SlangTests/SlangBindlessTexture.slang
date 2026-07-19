/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// End-to-end bindless access through the shared Bindless group: samples a bindlessly indexed
// texture into a buffer. The .shader descriptor pins ATOM_BINDLESS_DESCRIPTOR_SET for Vulkan
// (automatic assignment lands with the binding-ABI manifest milestone).

import Atom.Features.Bindless;

[AtomShaderResourceGroup(SrgBindingSlot.Pass)]
struct BindlessTestShaderResourceGroupLayout
{
    RWStructuredBuffer<Vector4F> m_results;
    u32 m_textureIndex;
};
ParameterBlock<BindlessTestShaderResourceGroupLayout> BindlessTestSrg;

[numthreads(64, 1, 1)]
void MainCS(u32 index : SV_DispatchThreadID)
{
    BindlessTestSrg.m_results[index] =
        Bindless::GetTexture2D(BindlessTestSrg.m_textureIndex).Load(Vector3<i32>(0, 0, 0));
}
