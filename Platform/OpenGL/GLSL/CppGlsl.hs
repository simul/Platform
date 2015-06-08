#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"
// These definitions translate the HLSL terms cbuffer and R0 for GLSL or C++
#define SIMUL_TEXTURE_REGISTER(tex_num) 
#define SIMUL_SAMPLER_REGISTER(samp_num) 
#define SIMUL_BUFFER_REGISTER(buff_num) 
#define SIMUL_RWTEXTURE_REGISTER(rwtex_num)
#define SIMUL_STATE_REGISTER(snum)

// GLSL doesn't  have a concept of output semantics but we need them to make the sfx format work. We'll use HLSL's semantics.
#define SIMUL_TARGET_OUTPUT : SV_TARGET
#define SIMUL_RENDERTARGET_OUTPUT(n) : SV_TARGET##n
#define SIMUL_DEPTH_OUTPUT : SV_DEPTH

#define GLSL

#ifndef __cplusplus
// Some GLSL compilers can't abide seeing a "discard" in the source of a shader that isn't a fragment shader, even if it's in unused, shared code.
	#if !defined(GL_FRAGMENT_SHADER)
		#define discard
	#endif
	#define constant_buffer layout(std140) uniform
	#define SIMUL_CONSTANT_BUFFER(name,buff_num) constant_buffer name {
	#define SIMUL_CONSTANT_BUFFER_END };
	#define _Y(texc) texc
	//vec2((texc).x,1.0-(texc).y)
	#define _Y3(texc) texc
	//vec3((texc).x,1.0-((texc).y),(texc).z)
	#define texelFetch3d(tex,p,lod) texelFetch(tex,p,lod)
	#define texelFetch2d(tex,p,lod) texelFetch(tex,p,lod)

	#define Y(texel) texel.y
	#define CS_LAYOUT(u,v,w) layout(local_size_x=u,local_size_y=v,local_size_z=w) in;
	#define IMAGE_STORE(a,b,c) imageStore(a,int2(b),c)
	#define IMAGE_STORE_3D(a,b,c) imageStore(a,int3(b),c)
	#define IMAGE_LOAD(a,b) imageLoad(a,b)
	#define TEXTURE_LOAD_MSAA(a,b,c) texelFetch(a,b,int(c))
	#define TEXTURE_LOAD(a,b) texelFetch(a,int2(b),0)
	#define TEXTURE_LOAD_3D(a,b) texelFetch(a,int3(b),0)
	#define IMAGE_LOAD_3D(a,b) imageLoad(a,int3(b))
	
	#define GET_IMAGE_DIMENSIONS(tex,X,Y) {ivec2 iv=imageSize(tex); X=iv.x;Y=iv.y;}
	#define GET_IMAGE_DIMENSIONS_3D(tex,X,Y,Z) {ivec3 iv=imageSize(tex); X=iv.x;Y=iv.y;Z=iv.z;}
	#define RW_TEXTURE2D_FLOAT4 image2D
	#define TEXTURE2DMS_FLOAT4 sampler2DMS
	#define TEXTURE2D_UINT usampler2D
	//layout(r32ui) 
	#define TEXTURE2D_UINT4 usampler2D
	#define groupshared shared
	//layout(rgba8)
	struct idOnly
	{
		uint vertex_id: gl_VertexID;
	};
#else
	#define STATIC static
	// To C++, samplers are just GLints.
	typedef int sampler1D;
	typedef int sampler2D;
	typedef int sampler3D;

	// C++ sees a layout as a struct, and doesn't care about uniforms
	//#define layout(std140) struct
	#define uniform

#endif
SIMUL_CONSTANT_BUFFER(RescaleVertexShaderConstants,0)
	uniform float rescaleVertexShaderY;
SIMUL_CONSTANT_BUFFER_END
#endif