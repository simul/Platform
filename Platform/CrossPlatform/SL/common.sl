//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMMON_SL
#define COMMON_SL

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
#define texture_clamp_mirror_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_cube(tex,texc) tex.Sample(cubeSamplerState,texc);

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
#define texture_3d_wmc(tex,texc) tex.Sample(wmcSamplerState,texc)
#define sample_3d_lod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)

#define GET_DIMENSIONS_MSAA(tex,x,y,s) tex.GetDimensions(x,y,s)
#define GET_DIMENSIONS(tex,x,y) tex.GetDimensions(x,y)
#define GET_DIMENSIONS_3D(tex,x,y,z) tex.GetDimensions(x,y,z)

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
	return OUT;
}
#endif
