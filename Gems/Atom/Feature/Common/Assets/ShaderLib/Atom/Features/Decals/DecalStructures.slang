
public struct Decal
{
    public float3 m_position;
    public float m_opacity;
    public float4 m_quaternion;
    public float3 m_halfSize;
    public float m_angleAttenuation;
    public float m_normalMapOpacity;
    public uint m_sortKeyPacked;
    public uint m_textureArrayIndex;
    public uint m_textureIndex;
    public float3 m_decalColor;
    public float m_decalColorFactor;
    // [[pad_to(16)]] - already aligned
};
