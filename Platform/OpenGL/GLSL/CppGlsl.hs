
#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"
// These definitions translate the HLSL terms cbuffer and R0 for GLSL or C++
#define SIMUL_TEXTURE_REGISTER(tex_num) 
#define SIMUL_SAMPLER_REGISTER(samp_num) 
#define SIMUL_BUFFER_REGISTER(buff_num) 
#define SIMUL_RWTEXTURE_REGISTER(rwtex_num)
#define SIMUL_STATE_REGISTER(snum)

#define SIMUL_TARGET_OUTPUT
#define SIMUL_RENDERTARGET_OUTPUT(n)
#define	SIMUL_DEPTH_OUTPUT

#define GLSL

#ifndef __cplusplus
// Some GLSL compilers can't abide seeing a "discard" in the source of a shader that isn't a fragment shader, even if it's in unused, shared code.
	#if !defined(GL_FRAGMENT_SHADER)
		#define discard
	#endif
	#define uniform_buffer layout(std140) uniform
	#define SIMUL_CONSTANT_BUFFER(name,buff_num) uniform_buffer name {
	#define SIMUL_CONSTANT_BUFFER_END };
#include "saturate.glsl"
	#define lerp mix
	#define atan2 atan
	#define int2 ivec2
	#define int3 ivec3
	#define int4 ivec4
	#define uint2 uvec2
	#define uint3 uvec3
	#define uint4 uvec4
	#define frac fract
	#define texture_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap(tex,texc) texture(tex,texc)
	#define texture_clamp_mirror(tex,texc) texture(tex,texc)
	#define texture_wrap_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap_mirror(tex,texc) texture(tex,texc) 
	#define sample(tex,smplr,texc) texture(tex,texc)
	#define sampleLod(tex,smplr,texc,lod) textureLod(tex,texc,lod)
	#define texture_wrap_lod(tex,texc,lod) textureLod(tex,texc,lod)
	#define texture_cwc_lod(tex,texc,lod) textureLod(tex,texc,lod)
	#define texture_clamp_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_nearest_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_clamp_mirror_lod(tex,texc,lod) textureLod(tex,texc,lod)
	#define texture_cmc_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_cmc_nearest_lod(tex,texc,lod) textureLod(tex,texc,lod)

	#define texture_wwc(tex,texc) texture(tex,texc)
	#define texture_nearest(tex,texc) texture(tex,texc)
	#define texture3Dpt(tex,texc) texture(tex,texc)
	#define texture2Dpt(tex,texc) texture(tex,texc)

	#define texelFetch3d(tex,p,lod) texelFetch(tex,p,lod)
	#define texelFetch2d(tex,p,lod) texelFetch(tex,p,lod)

	#define texture3D texture
	#define texture2D texture 
	#define Texture3D sampler3D 
	#define Texture2D sampler2D 
	#define Texture1D sampler1D 
	#define Y(texel) texel.y
	#define STATIC

	vec4 mul(mat4 m,vec4 v)
	{
		return m*v;
	}
	vec2 mul(mat2 m,vec2 v)
	{
		return m*v;
	}
	#define CS_LAYOUT(u,v,w) layout(local_size_x=u,local_size_y=v,local_size_z=w) in;
	
	#define IMAGESTORE(a,b,c) imageStore(a,ivec3(b),c)
	#define IMAGE_LOAD(a,b) texelFetch(a,ivec3(b),0)
	// SOME GLSL compilers like this version:
	//#define RW_TEXTURE3D_FLOAT4 layout(rgba32f,binding = 0) image3D
	// SOME GLSL compilers like it like this:
	#define RW_TEXTURE3D_FLOAT4 layout(rgba32f) image3D
	
#ifdef GLFX
	shader void VS_ScreenQuad( out vec2 texCoords)
	{
		vec2 poss[4];
		poss[0]		=vec2(1.0, 0.0);
		poss[1]		=vec2(1.0, 1.0);
		poss[2]		=vec2(0.0, 0.0);
		poss[3]		=vec2(0.0, 1.0);
		vec2 pos	=poss[gl_VertexID];
		gl_Position	=vec4(rect.xy+rect.zw*pos.xy,1.0,1.0);
		texCoords	=pos.xy;
		texCoords.y	=1.0-texCoords.y;
	}

	shader void VS_FullScreen(out vec2 texCoords)
	{
		vec2 poss[4];
		poss[0]			=vec2(1.0,0.0);
		poss[1]			=vec2(1.0,1.0);
		poss[2]			=vec2(0.0,0.0);
		poss[3]			=vec2(0.0,1.0);
		vec2 pos		=poss[gl_VertexID];
	//	pos.y			=yRange.x+pos.y*yRange.y;
		vec4 vert_pos	=vec4(vec2(-1.0,-1.0)+2.0*vec2(pos.x,pos.y),1.0,1.0);
		gl_Position		=vert_pos;
		texCoords		=pos.xy;
	}

#endif
#else
	#define STATIC static
	// To C++, samplers are just GLints.
	typedef int sampler1D;
	typedef int sampler2D;
	typedef int sampler3D;

	// C++ sees a layout as a struct, and doesn't care about uniforms
	#define layout(std140) struct
	#define uniform

#endif
#endif