#ifndef COLOUR_PACKING_SL
#define COLOUR_PACKING_SL

inline uint colour3_to_uint(vec3 colour)
{
	// Convert to R11G11B10
	colour = clamp(colour, 0, 1);
	uint int_r = uint(colour.r * 2047.0 + 0.5);
	uint int_g = uint(colour.g * 2047.0 + 0.5);
	uint int_b = uint(colour.b * 1023.0 + 0.5);
	// Pack into UINT32
	return (int_r << 21) | (int_g << 10) | int_b;
}

inline vec3 uint_to_colour3(uint int_colour)
{
	// Unpack from UINT32
	float r = float(int_colour >> 21);
	float g = float((int_colour >> 10) & 0x7ff);
	float b = float(int_colour & 0x0003ff);
	// Convert R11G11B10 to float3
	return vec3(r/2047.0, g/2047.0, b/1023.0);
}

inline uint2 colour3_to_uint2(vec3 colour)
{
	// Convert to R16G16B16
	colour = clamp(colour, 0, 1);
	uint int_r = asuint(colour.r);
	uint int_g = f32tof16(colour.g);
	uint int_b = f32tof16(colour.b );
	// Pack into UINT32
	return uint2(int_r,(int_g << 16) | int_b);
}

inline vec3 uint2_to_colour3(uint2 int_colour)
{
	// Unpack from UINT2 32
	float r = asfloat(int_colour.x);
	float g = f16tof32(int_colour.y>>16);
	float b = f16tof32(int_colour.y);
	// Convert R11G11B10 to float3
	return vec3(r, g, b);
}

vec4 convertInt(TEXTURE2D_UINT glowTexture,uint2 location)
{
	uint int_color = TEXTURE_LOAD(glowTexture,int2(location)).x;

	// Convert R11G11B10 to float3
	vec4 color;
	color.r =float(int_color >> 21) / 2047.0f;
	color.g =float((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b =float(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;
	color.rgb*=10.0;
	return color;
}

vec4 texture_int(TEXTURE2D_UINT glowTexture,vec2 texCoord)
{
	uint2 tex_dim;
	GET_DIMENSIONS(glowTexture,tex_dim.x, tex_dim.y);

	vec2 pos1=vec2(tex_dim.x*texCoord.x-0.5,tex_dim.y * texCoord.y-0.5);
	vec2 pos2=vec2(tex_dim.x*texCoord.x+0.5,tex_dim.y * texCoord.y+0.5);

	uint2 location1=uint2(pos1);
	uint2 location2=uint2(pos2);

	vec2 l=vec2(tex_dim.x*texCoord.x,tex_dim.y * texCoord.y)-vec2(location1);

	vec4 tex00=convertInt(glowTexture,location1);
	vec4 tex10=convertInt(glowTexture,uint2(location2.x,location1.y));
	vec4 tex11=convertInt(glowTexture,location2);
	vec4 tex01=convertInt(glowTexture,uint2(location1.x,location2.y));

	vec4 tex0=lerp(tex00,tex10,l.x);
	vec4 tex1=lerp(tex01,tex11,l.x);
	vec4 tex=lerp(tex0,tex1,l.y);
	return tex;
}
#endif