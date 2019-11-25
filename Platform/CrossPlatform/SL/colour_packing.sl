//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COLOUR_PACKING_SL
#define COLOUR_PACKING_SL
#include "common.sl"

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
	float r = float(int_colour >> uint(21));
	float g = float((int_colour >> uint(10)) & uint(0x7ff));
	float b = float(int_colour & uint(0x0003ff));
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
	uint int_color = uint(TEXTURE_LOAD(glowTexture,int2(location)).x);

	// Convert R11G11B10 to float3
	vec4 color;
	color.r =float(int_color >> uint(21)) / 2047.0f;
	color.g =float((int_color >> uint(10)) & uint(0x7ff)) / 2047.0f;
	color.b =float(int_color & uint(0x0003ff)) / 1023.0f;
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

// enc=.5* norm(n.xy)*sqrt(.5-.5 n.z)+.5

// nn.xy=(2 enc-1)=norm(n.xy)*sqrt(.5 - .5 n.z)
// so .5-.5 n.z = pow(len(2 enc-1),2)
// let nn.z=1.0-|nn.xy|^2
// norm(n.xy) = (2 enc - 1)/sqrt(.5-.5 n.z)
// n is normalized, so |n.xy|=sqrt(1.0-n.z*n.z)
// 
vec2 encodeNormalToVec2(vec3 n)
{
	float p = sqrt(n.z * 8.0 + 8.0);
	vec2 enc = (n.xy / p + 0.5);
	return enc;
}

vec3 decodeNormalFromVec2(vec2 enc)
{
	vec2 fenc = enc * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(1 - f / 4.0);
	vec3 n;
	n.xy = fenc*g;
	n.z = 1.0 - f / 2.0;
	return n;
}

#ifdef SFX_TYPED_UAV_LOADS
vec4 UnpackUintToVec4(vec4 a)
{
	return a;
}
vec4 PackVec4ToUint(vec4 a)
{
	return a;
}
#else

vec4 UnpackUintToVec4(uint i)
{
	vec4 o;
	o.r=((i&0x000000FFu)		)/255.0;
	o.g=((i&0x0000FF00u)>>8u	)/255.0;
	o.b=((i&0x00FF0000u)>>16u	)/255.0;
	o.a=((i&0xFF000000u)>>24u	)/255.0;
	return o;
}

uint PackVec4ToUint(vec4 i)
{
	uint o=0u;
	i=saturate(i);
	o|=(uint(i.x*255.0));
	o|=(uint(i.y*255.0)<<8u);
	o|=(uint(i.z*255.0)<<16u);
	o|=(uint(i.w*255.0)<<24u);
	//uint(i.w*255.0)|(uint(i.z*255.0)<<16u)|(uint(i.y*255.0)<<32u)|;
	return o;
}
#endif
#endif