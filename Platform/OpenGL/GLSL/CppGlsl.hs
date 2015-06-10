#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"

#ifndef __cplusplus
	//#define IMAGE_LOAD(a,b) imageLoad(a,b)
	#define IMAGE_LOAD(a,b) a[b]
	//#define TEXTURE_LOAD_MSAA(a,b,c) texelFetch(a,b,int(c))
	#define TEXTURE_LOAD_MSAA(tex,uint2pos,sampl) tex.Load(uint2pos,sampl)
	//#define TEXTURE_LOAD(a,b) texelFetch(a,int2(b),0)
	//#define TEXTURE_LOAD_3D(a,b) texelFetch(a,int3(b),0)
	#define TEXTURE_LOAD(tex,uintpos) tex.Load(int3(uintpos,0))
	#define TEXTURE_LOAD_3D(tex,uintpos) tex.Load(int4(uintpos,0))
	//#define IMAGE_LOAD_3D(a,b) imageLoad(a,int3(b))
#define IMAGE_LOAD_3D(tex,uintpos) tex[uintpos]
	
constant_buffer RescaleVertexShaderConstants : register(b0)
{
	uniform float rescaleVertexShaderY;
};
#else
struct RescaleVertexShaderConstants
{
	static const int bindingIndex=0;
	uniform float rescaleVertexShaderY;
};
#endif
#endif