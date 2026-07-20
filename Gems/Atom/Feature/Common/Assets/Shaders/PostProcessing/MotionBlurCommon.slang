
// The size of the NxN tile max filter, set to 20 according to the original slide
// Also modify the size multiplier in `.pass` files after modifying this value
public static const int TILE_SIZE = 20;

// Returns the larger value of two velocities
public float2 vmax(float2 a, float2 b)
{
	return dot(a, a) > dot(b, b) ? a : b;
}
