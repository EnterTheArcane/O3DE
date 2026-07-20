/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */



public static const uint MacroClipmapStackSizeMax = 16u;
public static const uint DetailClipmapStackSizeMax = 16u;
public static const uint SharedClipmapStackSizeMax = 16u; // = max(MacroClipmapStackSizeMax, DetailClipmapStackSizeMax)
// Each clipmap can have at most 6 update regions based on the current algorithm.
// Due to compilation limitation, the size of arrays must be direct constant.
public static const uint UpdateRegionMax = 6u;
public static const uint MacroClipmapUpdateRegionMax = 96u; // = MacroClipmapStackSizeMax * 6
public static const uint DetailClipmapUpdateRegionMax = 96u; // = DetailClipmapStackSizeMax * 6



[AtomShaderResourceGroup(SrgBindingSlot.SubPass)]
public struct TerrainSrgLayout
{
    public struct MeshData
    {
        public float3 m_mainCameraPosition;
        public float m_firstLodDistance;
        public float m_rcpClodDistance;
        public float m_rcpGridSize;
        public float m_gridToQuadScale;
        public float m_pad; // [[pad_to(16)]]
    };

    public struct DetailMaterialData
    {
        // Uv
        public row_major float3x4 m_uvTransform;

        public float3 m_baseColor;

        // Factor / Scale / Bias for input textures
        public float m_baseColorFactor;

        public float m_normalFactor;
        public float m_metalFactor;
        public float m_roughnessScale;
        public float m_roughnessBias;

        public float m_specularF0Factor;
        public float m_occlusionFactor;
        public float m_heightFactor;
        public float m_heightOffset;

        public float m_heightBlendFactor;
        public float m_heightWeightClampFactor;

        // Flags
        public uint m_flags; // see DetailTextureFlags

        // Image indices
        public uint m_colorImageIndex;
        public uint m_normalImageIndex;
        public uint m_roughnessImageIndex;
        public uint m_metalnessImageIndex;
        public uint m_specularF0ImageIndex;
        public uint m_occlusionImageIndex;
        public uint m_heightImageIndex;

        // 16 byte aligned
        public uint2 m_padding;
    };

    public struct MacroMaterialData
    {
        // bit 1 : Is this macro material used.
        // bit 2 : flip normal x
        // bit 3 : flip normal y
        public uint m_flags;

        public uint m_colorMapId;
        public uint m_normalMapId;
        public float m_normalFactor;
        public float2 m_boundsMin;
        public float2 m_boundsMax;
    };

    public struct MacroMaterialGridInfo
    {
        public uint m_tileCount1D; // How many x/y tiles along edge of grid. Grid is always square, so total number of tiles is this value squared.
        public float m_tileSize; // Size of a tile in meters.
        public float2 m_pad; // [[pad_to(16)]]
    };

    public struct MacroMaterialRefs
    {
        public uint m_index01;
        public uint m_index23;
    };

    public struct ClipmapUpdateRegion
    {
        public uint4 m_clipmapRegion;
        public uint m_clipmapLevel;
        public uint3 m_padding;
    };

    public struct ClipmapData
    {
        // Current viewport size.
        public float2 m_viewportSize;

        // The max range that the clipmap is covering.
        public float m_macroClipmapMaxRenderRadius;
        public float m_detailClipmapMaxRenderRadius;

        // The scale base between two adjacent clipmap layers.
        // For example, 3 means the (n+1)th clipmap covers 3^2 = 9 times
        // to what is covered by the nth clipmap.
        public float m_macroClipmapScaleBase;
        public float m_detailClipmapScaleBase;

        // Size of the clipmap stack.
        public uint m_macroClipmapStackSize;
        public uint m_detailClipmapStackSize;

        // The margin size of the edge of the clipmap where the data won't be used.
        public float m_macroClipmapMarginSize;
        public float m_detailClipmapMarginSize;

        // In addition to the above margin size used for updating,
        // this margin is a safety margin to avoid edge cases when blending or sampling.
        public float m_extendedClipmapMarginSize;

        // The size of the clipmap image in each layer.
        // Given 2 copies in different types to save casting.
        public float m_clipmapSizeFloat;
        public uint m_clipmapSizeUint;

        // The texel position where blending to the next level should start. Equivalent to:
        // m_clipmapSizeFloat / 2.0 - m_macroClipmapMarginSize - m_extendedClipmapMarginSize
        // Cached for frequent access.
        public float m_validMacroClipmapRadius;
        // Same as above, equivalent to:
        // m_clipmapSizeFloat / 2.0 - m_detailClipmapMarginSize - m_extendedClipmapMarginSize
        public float m_validDetailClipmapRadius;

        // The size of the blending area between each clipmap level.
        public float m_clipmapBlendSize;

        // The number of regions to be updated during the current frame.
        public uint m_macroClipmapUpdateRegionCount;
        public uint m_detailClipmapUpdateRegionCount;

        // Numbers match the compute shader invoking call dispatch(X, Y, 1).
        public uint m_macroDispatchGroupCountX;
        public uint m_macroDispatchGroupCountY;
        public uint m_detailDispatchGroupCountX;
        public uint m_detailDispatchGroupCountY;

        // Debug data
        // Enables debug overlay to indicate clipmap levels.
        public float m_enableMacroClipmapOverlay;
        public float m_enableDetailClipmapOverlay;

        // 0: macro color clipmap
        // 1: macro normal clipmap
        // 2: detail color clipmap
        // 3: detail normal clipmap
        // 4: detail height clipmap
        // 5: detail roughness clipmap
        // 6: detail specularF0 clipmap
        // 7: detail metalness clipmap
        // 8: detail occlusion clipmap
        public uint m_debugClipmapId;

        // Which clipmap level to sample from, or texture array index.
        public float m_debugClipmapLevel; // cast to float in CPU

        // How big the clipmap should appear on the screen.
        public float m_debugScale;

        // Multiplier adjustment for final color output.
        public float m_debugBrightness;

        // Clipmap centers in texel coordinates ranging [0, size).
        // Clipmap centers are the logical center of the texture, based on toroidal addressing.
        public struct ClipmapCenter
        {
            public uint2 m_macro;
            public uint2 m_detail;
        };
        public ClipmapCenter m_clipmapCenters[SharedClipmapStackSizeMax];

        // Clipmap centers in world coordinates.
        public struct ClipmapWorldCenter
        {
            public float2 m_macro;
            public float2 m_detail;
        };
        public ClipmapWorldCenter m_clipmapWorldCenters[SharedClipmapStackSizeMax];

        // A scale converting the length from the texture space to the world space.
        // For example: given texel (u0, v0) and (u1, v1), dtexel = sqrt((u0 - u1)^2, (v0 - v1)^2)
        //              dworld = dtexel * clipmapToWorldScale.
        public struct ClipmapToWorldScale
        {
            public float m_macro;
            public float m_detail;
            public float2 m_padding;
        };
        public ClipmapToWorldScale m_clipmapToWorldScale[SharedClipmapStackSizeMax];
        // TODO(slang-port): [[pad_to(16)]] - add explicit padding (non-literal array length)
    };

    public MeshData m_meshData;

    // Clipmap SRG
    public ClipmapData m_clipmapData;

    // Clipmap Sampler
    [AtomStaticSampler("Point", "Point", "Point", "Clamp", "Clamp", "Clamp", 0, "Never", "Filter", "TransparentBlack")]
    public SamplerState m_clipmapSampler;
    
    // The region of the clipmap that needs update.
    // Each clipmap can have 0-6 regions to update each frame.
    public StructuredBuffer<ClipmapUpdateRegion> m_macroClipmapUpdateRegions;
    public StructuredBuffer<ClipmapUpdateRegion> m_detailClipmapUpdateRegions;

    public Texture2DArray<float4> m_macroColorClipmaps;
    public Texture2DArray<float2> m_macroNormalClipmaps;
    public Texture2DArray<float4> m_detailColorClipmaps;
    public Texture2DArray<float2> m_detailNormalClipmaps;
    public Texture2DArray<float> m_detailHeightClipmaps;
    public Texture2DArray<float> m_detailRoughnessClipmaps;
    public Texture2DArray<float> m_detailSpecularF0Clipmaps;
    public Texture2DArray<float> m_detailMetalnessClipmaps;
    public Texture2DArray<float> m_detailOcclusionClipmaps;

    public Texture2D<uint4> m_detailMaterialIdImage;
    public StructuredBuffer<DetailMaterialData> m_detailMaterialData;

    public StructuredBuffer<MacroMaterialData> m_macroMaterialData;
    public StructuredBuffer<MacroMaterialRefs> m_macroMaterialGridRefs;
    public MacroMaterialGridInfo m_macroMaterialGridInfo;
    
    public float m_detailMaterialIdScale;

    public float CalculateClodDistance(uint lodLevel, float3 worldPosition)
    {
        float maxRange = m_meshData.m_firstLodDistance * float(1u << lodLevel);
        float viewDistanceToWorldPosition = length(m_meshData.m_mainCameraPosition - worldPosition);
        return maxRange - viewDistanceToWorldPosition;
    }
};
public ParameterBlock<TerrainSrgLayout> TerrainSrg;

public static const float MacroMaterialsPerTile = 4;
