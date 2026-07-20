// This code was generated from the MaterialShaderParameterLayout. Do not modify.

public struct MaterialParameters {
    public uint     m_materialType;
    public uint     m_materialInstance;
    public float4   m_baseColor;
    public float    m_baseColorFactor;
    public int      m_baseColorMap;
    public int      m_baseColorMapUvIndex;
    public float    m_vertexColorFactor;
    public float    m_metallicFactor;
    public int      m_metallicMap;
    public int      m_metallicMapUvIndex;
    public int      m_roughnessMap;
    public int      m_roughnessMapUvIndex;
    public float    m_roughnessLowerBound;
    public float    m_roughnessUpperBound;
    public float    m_roughnessFactor;
    public float    m_specularF0Factor;
    public int      m_specularF0Map;
    public int      m_specularF0MapUvIndex;
    public int      m_normalMap;
    public int      m_normalMapUvIndex;
    public bool     m_flipNormalX;
    public bool     m_flipNormalY;
    public float    m_normalFactor;
    public int      m_diffuseOcclusionMap;
    public int      m_diffuseOcclusionMapUvIndex;
    public float    m_diffuseOcclusionFactor;
    public int      m_specularOcclusionMap;
    public int      m_specularOcclusionMapUvIndex;
    public float    m_specularOcclusionFactor;
    public float4   m_emissiveColor;
    public float    m_emissiveAffectedByAlpha;
    public int      m_emissiveMap;
    public int      m_emissiveMapUvIndex;
    public float    m_clearCoatFactor;
    public int      m_clearCoatInfluenceMap;
    public int      m_clearCoatInfluenceMapUvIndex;
    public float    m_clearCoatRoughness;
    public int      m_clearCoatRoughnessMap;
    public int      m_clearCoatRoughnessMapUvIndex;
    public float    m_clearCoatNormalStrength;
    public int      m_clearCoatNormalMap;
    public int      m_clearCoatNormalMapUvIndex;
    public int      m_heightmap;
    public int      m_parallaxUvIndex;
    public float    m_heightmapScale;
    public float    m_heightmapOffset;
    public float    m_parallax_pdo_shadowFactor;
    public int      m_opacityMap;
    public int      m_opacityMapUvIndex;
    public float    m_opacityFactor;
    public float    m_opacityAffectsSpecularFactor;
    public float    m_emissiveIntensity;
    // NOTE: float3x3 is NOT used here due to a Vulkan/row_major bug with matrices inside structured buffers (ByteAddressBuffer).
    // Instead, each matrix is stored as 3 explicit row_major float4 rows (w component unused).
    // Use MulUvMatrix(params, uv) / MulUvMatrixInverse(params, uv) instead of direct mul().
    public float4 m_uvMatrixRow0;
    public float4 m_uvMatrixRow1;
    public float4 m_uvMatrixRow2;
    public float4 m_uvMatrixInverseRow0;
    public float4 m_uvMatrixInverseRow1;
    public float4 m_uvMatrixInverseRow2;
}; 

