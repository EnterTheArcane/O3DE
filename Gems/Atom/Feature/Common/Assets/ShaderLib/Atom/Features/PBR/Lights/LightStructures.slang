

// Directional Lights
public struct DirectionalLight
{
    public float3 m_direction;
    public float m_angularRadius;
    public float3 m_rgbIntensityLux;
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float2 m_padding;
};

// Simple Point Lights

public struct SimplePointLight
{
    public float3 m_position;
    public float m_invAttenuationRadiusSquared; // For a radius at which this light no longer has an effect, 1 / radius^2.
    public float3 m_rgbIntensityCandelas;
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float2 m_padding;
    // [[pad_to(16)]] - already aligned  // Here to ensure we pad the struct to 16 in case someone adds incorrect padding
};

// Simple Spot Lights

public struct SimpleSpotLight
{
    public float4x4 m_viewProjectionMatrix; // Light's view projection matrix. Used to calculate uv for gobo
    public float3 m_position;
    public float m_invAttenuationRadiusSquared; // For a radius at which this light no longer has an effect, 1 / radius^2.
    public float3 m_direction;
    public float m_cosInnerConeAngle; // cosine of the outer cone angle
    public float3 m_rgbIntensityCandelas;
    public float m_cosOuterConeAngle; // cosine of the inner cone angle
    public uint m_shadowIndex;
    public uint m_goboTexIndex;
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float3 m_pad; // [[pad_to(16)]] // Here to ensure we pad the struct to 16 in case someone adds incorrect padding
};

// Point lights (sphere lights)

public struct PointLight
{
    public float3 m_position;
    public float m_invAttenuationRadiusSquared; // For a radius at which this light no longer has an effect, 1 / radius^2.
    public float3 m_rgbIntensityCandelas;
    public float m_bulbRadius;
    public uint3 m_shadowIndices;
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float2 m_padding;
};

// Disk Lights

public enum DiskLightFlag
{
    UseConeAngle = 1,
};

public struct DiskLight
{
    public float3 m_position;
    public float m_invAttenuationRadiusSquared; // For a radius at which this light no longer has an effect, 1 / radius^2.
    public float3 m_rgbIntensityCandelas;
    public float m_diskRadius;
    public float3 m_direction;
    public uint m_flags;
    public float m_cosInnerConeAngle;
    public float m_cosOuterConeAngle;
    public float m_bulbPositionOffset;
    public uint m_shadowIndex;
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float m_padding;
};

// Capsule Lights
    
public struct CapsuleLight
{
    public float3 m_startPoint;   // One of the end points of the capsule
    public float m_radius;        // Radius of the capsule, ie distance from line segment to surface.
    public float3 m_direction;    // normalized vector from m_startPoint towards the other end point.
    public float m_length;        // length of the line segment making up the inside of the capsule. Doesn't include caps (0 length capsule == sphere)
    public float3 m_rgbIntensityCandelas; // total rgb luminous intensity of the capsule in candelas
    public float m_invAttenuationRadiusSquared; // Inverse of the distance at which this light no longer has an effect, squared. Also used for falloff calculations.
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float m_padding;
};

// Quad Lights

public enum QuadLightFlag
{
    None = 0x00,
    EmitsBothDirections  = 0x01, // 1 << 0, // Quad should emit light from both sides
    UseFastApproximation = 0x02, // 1 << 1, // Use a fast approximation instead of linearly transformed cosines.
};

public struct QuadLight
{
    public float3 m_position;
    public float m_invAttenuationRadiusSquared; // For a radius at which this light no longer has an effect, 1 / radius^2.
    public float3 m_leftDir; // Direction from center of quad to the left edge
    public float m_halfWidth; // Half the width of the quad. m_leftDir * m_halfWidth is a vector from the center to the left edge.
    public float3 m_upDir; // Direction from center of quad to the top edge
    public float m_halfHeight; // Half the height of the quad. m_upDir * m_halfHeight is a vector from the center to the top edge.
    public float3 m_rgbIntensityNits;
    public uint m_flags; // See QuadLightFlag enum
    public float m_affectsGIFactor;
    public bool m_affectsGI;
    public uint m_lightingChannelMask;
    public float m_padding;
};

// Polygon Lights

public struct PolygonLight
{
    public float3 m_position;
    public uint m_startEndIndex; // 16 bit start and end indices packed into one 32 bit uint
    public float3 m_rgbIntensityNits;
    public float m_invAttenuationRadiusSquared; // negative sign bit indicates double sided.
    public float3 m_direction;
    public uint m_lightingChannelMask;
};
