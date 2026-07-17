/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// The Bindless ShaderResourceGroup, Slang edition — the counterpart of Bindless.azsli.
//
// Usage matches the AZSL version:
//
//     Texture2D<Vector4F> texture = Bindless::GetTexture2D(42);
//
// The ParameterBlock form cannot express this group (Slang allows at most one unsized member per
// struct), so the members are global-scope unsized arrays grouped into the logical "Bindless"
// ShaderResourceGroup via [AtomShaderResourceGroupMember]. Shared SRGs are a binding ABI: every
// binding below is pinned to the exact assignment AZSLC produces for Bindless.azsli, verified by
// compiling it with the production arguments per API:
//
// - DX12 (--namespace=dx): each unsized array gets its own register space starting at 1000 in
//   declaration order, with the register index incrementing per register class.
// - Vulkan (--namespace=vk --unique-idx): sequential binding indices 0-4 in declaration order,
//   all in the Bindless descriptor set. The set index depends on which other SRGs the consuming
//   shader uses (AZSLC ranks the present SRGs by frequency), so the builder injects it per
//   compile as ATOM_BINDLESS_DESCRIPTOR_SET.

#ifndef ATOM_BINDLESS_DESCRIPTOR_SET
#define ATOM_BINDLESS_DESCRIPTOR_SET 0
#endif

[AtomShaderResourceGroupMember("Bindless", 7)]
[[vk::binding(0, ATOM_BINDLESS_DESCRIPTOR_SET)]]
Texture2D<Vector4F> m_Texture2D[] : register(t0, space1000);

[AtomShaderResourceGroupMember("Bindless", 7)]
[[vk::binding(1, ATOM_BINDLESS_DESCRIPTOR_SET)]]
RWTexture2D<Vector4F> m_RWTexture2D[] : register(u0, space1001);

[AtomShaderResourceGroupMember("Bindless", 7)]
[[vk::binding(2, ATOM_BINDLESS_DESCRIPTOR_SET)]]
ByteAddressBuffer m_ByteAddressBuffer[] : register(t1, space1002);

[AtomShaderResourceGroupMember("Bindless", 7)]
[[vk::binding(3, ATOM_BINDLESS_DESCRIPTOR_SET)]]
RWByteAddressBuffer m_RWByteAddressBuffer[] : register(u1, space1003);

[AtomShaderResourceGroupMember("Bindless", 7)]
[[vk::binding(4, ATOM_BINDLESS_DESCRIPTOR_SET)]]
TextureCube m_TextureCube[] : register(t2, space1004);

namespace Bindless
{
    public Texture2D<Vector4F> GetTexture2D(u32 index)
    {
        return m_Texture2D[index];
    }

    public RWTexture2D<Vector4F> GetRWTexture2D(u32 index)
    {
        return m_RWTexture2D[index];
    }

    public ByteAddressBuffer GetByteAddressBuffer(u32 index)
    {
        return m_ByteAddressBuffer[index];
    }

    public RWByteAddressBuffer GetRWByteAddressBuffer(u32 index)
    {
        return m_RWByteAddressBuffer[index];
    }

    public TextureCube GetTextureCube(u32 index)
    {
        return m_TextureCube[index];
    }
}
