//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMMON_SL
#define COMMON_SL
#define SIMUL_PI_F 3.1415926536
#define SIMUL_HALF_PI_F SIMUL_PI_F * 0.5
#define SIMUL_TAU_F SIMUL_PI_F * 2
#define texture_clamp_mirror(tex,texc) tex.Sample(cmcSamplerState,texc)
#define texture_clamp(tex,texc) tex.Sample(clampSamplerState,texc)
#define texture_wrap_clamp(tex,texc) tex.Sample(wrapClampSamplerState,texc)
#define texture_wrap_mirror(tex,texc) tex.Sample(wrapMirrorSamplerState,texc)
#define texture_wrap_mirror_lod(tex,texc,lod) tex.SampleLevel(wrapMirrorSamplerState,texc,lod)
#define sample(tex,sampler,texc) tex.Sample(sampler,texc)
#define sampleLod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define sample_lod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define texture_wrap(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod)
#define texture_wrap_clamp_lod(tex,texc,lod) tex.SampleLevel(wrapClampSamplerState,texc,lod)
#define texture_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)
#define texture_wrap_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearestWrap,texc,lod)
#define texture_clamp_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearestClamp,texc,lod)
#define texture_clamp_mirror_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_cube(tex,texc) tex.Sample(cubeSamplerState,texc);
#define texture_cube_lod(tex,texc,lod) tex.SampleLevel(cubeSamplerState,texc,lod);

#define texture_wwc(tex,texc) tex.Sample(wwcSamplerState,texc)
#define texture_wwc_lod(tex,texc,lod) tex.SampleLevel(wwcSamplerState,texc,lod)
#define texture_nearest(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)

#define texture_clamp_mirror_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)

#define texture_3d_cmc(tex,texc) tex.Sample(cmcSamplerState,texc)
#define texture_3d_nearest(tex,texc) tex.Sample(samplerStateNearest,texc) 
#define sample_3d(tex,sampler,texc) tex.Sample(sampler,texc) 
#define texture_3d_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod) 
#define texture_3d_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod) 
#define texture_3d_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_3d_clamp(tex,texc) tex.Sample(clampSamplerState,texc) 
#define texture_3d_wwc_lod(tex,texc,lod) tex.SampleLevel(wwcSamplerState,texc,lod) 
#define texture_3d_cwc_lod(tex,texc,lod) tex.SampleLevel(cwcSamplerState,texc,lod)
#define texture_3d_cmc_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_3d_cmc_nearest_lod(tex,texc,lod) tex.SampleLevel(cmcNearestSamplerState,texc,lod)
#define texture_3d_wmc_lod(tex,texc,lod) tex.SampleLevel(wmcSamplerState,texc,lod)
#define texture_3d_wcc_lod(tex,texc,lod) tex.SampleLevel(wccSamplerState,texc,lod)
#define texture_3d_wmc(tex,texc) tex.Sample(wmcSamplerState,texc)
#define sample_3d_lod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)

#ifndef __cplusplus
	#define ALIGN_16
	#define SIMUL_TEXTURE_REGISTER(tex_num) : register(t##tex_num)
	#define SIMUL_SAMPLER_REGISTER(samp_num) : register(s##samp_num)
	#define SIMUL_BUFFER_REGISTER(buff_num) : register(b##buff_num)
	#define SIMUL_RWTEXTURE_REGISTER(rwtex_num) : register(u##rwtex_num)
	#define SIMUL_STATE_REGISTER(snum) : register(s##snum)

	#define SIMUL_CONSTANT_BUFFER(name,buff_num) cbuffer name SIMUL_BUFFER_REGISTER(buff_num) {
	#define SIMUL_CONSTANT_BUFFER_END };

	#define SIMUL_TARGET_OUTPUT : SV_TARGET
	#define SIMUL_RENDERTARGET_OUTPUT(n) : SV_TARGET##n
	#define SIMUL_DEPTH_OUTPUT : SV_DEPTH
	#define CS_LAYOUT(u,v,w) [numthreads(u,v,w)]
	#define texelFetch3d(tex,p,lod) tex.Load(int4(p,lod))
	#define texelFetch2d(tex,p,lod) tex.Load(int3(p,lod))

	#define GET_IMAGE_DIMENSIONS(tex,x,y) tex.GetDimensions(x,y)
	#define GET_IMAGE_DIMENSIONS_3D(tex,x,y,z) tex.GetDimensions(x,y,z)
	#define RW_TEXTURE3D_FLOAT4 RWTexture3D<float4>
	#define RW_TEXTURE3D_FLOAT RWTexture3D<float>
	#define RW_TEXTURE2D_FLOAT4 RWTexture2D<float4>
	#define TEXTURE2DMS_FLOAT4 Texture2DMS<float4>
	#define TEXTURE2D_UINT Texture2D<uint>
	#define TEXTURE2D_UINT4 Texture2D<uint4>


	#define IMAGE_LOAD(a,b) a[b]
	#define TEXTURE_LOAD_MSAA(tex,uint2pos,sampl) tex.Load(uint2pos,sampl)
	#define TEXTURE_LOAD(tex,uintpos) tex.Load(int3(uintpos,0))
	#define TEXTURE_LOAD_3D(tex,uintpos) tex.Load(int4(uintpos,0))

	#define IMAGE_LOAD_3D(tex,uintpos) tex[uintpos]
#endif
#define GET_DIMENSIONS_MSAA(tex,x,y,s) tex.GetDimensions(x,y,s)
#define GET_DIMENSIONS(tex,x,y) tex.GetDimensions(x,y)
#define GET_DIMENSIONS_3D(tex,x,y,z) tex.GetDimensions(x,y,z)

#define	IMAGE_STORE(a,b,c) a[b]=c;
#define	IMAGE_STORE_3D(a,b,c) a[b]=c;

struct idOnly
{
	uint vertex_id			: SV_VertexId;
};

struct posTexVertexOutput
{
	vec4 hPosition	: SV_POSITION;
	vec2 texCoords	: TEXCOORD0;		
};

struct positionColourVertexInput
{
	vec3 position	: POSITION;
	vec4 colour		: TEXCOORD0;		
};

posTexVertexOutput SimpleFullscreen(idOnly IN)
{
	posTexVertexOutput OUT;
	vec2 poss[4];
	poss[0]=vec2( 1.0,-1.0);
	poss[1]=vec2( 1.0, 1.0);
	poss[2]=vec2(-1.0,-1.0);
	poss[3]=vec2(-1.0, 1.0);
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
	OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
#ifdef SFX_OPENGL
	OUT.texCoords.y =1.0 - OUT.texCoords.y;
#endif
	return OUT;
}

shader posTexVertexOutput VS_SimpleFullscreen(idOnly IN)
{
	posTexVertexOutput pt=SimpleFullscreen(IN);
	return pt;
}

#pragma warning(disable:1)

posTexVertexOutput VS_ScreenQuad(idOnly IN,vec4 rect)
{
	posTexVertexOutput OUT;
	vec2 poss[4];
	poss[0]			=vec2(1.0, 0.0);
	poss[1]			=vec2(1.0, 1.0);
	poss[2]			=vec2(0.0, 0.0);
	poss[3]			=vec2(0.0, 1.0);
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
	OUT.texCoords	=pos;
#ifdef SFX_OPENGL
	OUT.texCoords.y =1.0 - OUT.texCoords.y;
#endif
	return OUT;
}

#endif
