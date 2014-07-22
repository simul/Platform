#ifndef COLOUR_PACKING_SL
#define COLOUR_PACKING_SL

inline uint colour3_to_uint(vec3 colour)
{
	// Convert to R11G11B10
	colour = clamp(colour, 0, 1);
	uint int_r = (uint)(colour.r * 2047.0f + 0.5);
	uint int_g = (uint)(colour.g * 2047.0f + 0.5);
	uint int_b = (uint)(colour.b * 1023.0f + 0.5);
	// Pack into UINT32
	return (int_r << 21) | (int_g << 10) | int_b;
}

inline vec3 uint_to_colour3(uint int_colour)
{
	// Unpack from UINT32
	float r = (float)(int_colour >> 21);
	float g = (float)((int_colour >> 10) & 0x7ff);
	float b = (float)(int_colour & 0x0003ff);
	// Convert R11G11B10 to float3
	return vec3(r/2047.0f, g/2047.0f, b/1023.0f);
}

#endif