#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"


#ifndef __cplusplus

	#define texelFetch3d(tex,p,lod) texelFetch(tex,p,lod)
	#define texelFetch2d(tex,p,lod) texelFetch(tex,p,lod)

	#define IMAGE_STORE(a,b,c) imageStore(a,int2(b),c)
	#define IMAGE_STORE_3D(a,b,c) imageStore(a,int3(b),c)
	#define IMAGE_LOAD(a,b) imageLoad(a,b)
	#define TEXTURE_LOAD_MSAA(a,b,c) texelFetch(a,b,int(c))
	#define TEXTURE_LOAD(a,b) texelFetch(a,int2(b),0)
	#define TEXTURE_LOAD_3D(a,b) texelFetch(a,int3(b),0)
	#define IMAGE_LOAD_3D(a,b) imageLoad(a,int3(b))
	constant_buffer RescaleVertexShaderConstants: register(b0)
	{
		uniform float rescaleVertexShaderY;
	};
#else
	struct RescaleVertexShaderConstants
	{
		static const int bindingIndex=0;
		float rescaleVertexShaderY;
	};
#endif
#endif