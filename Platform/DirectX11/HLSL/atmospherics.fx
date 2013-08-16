#include "CppHlsl.hlsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "states.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"

Texture2D depthTexture;
Texture2D imageTexture;
Texture2D lossTexture;
Texture2D inscTexture;
Texture2D overcTexture;
Texture2D skylTexture;
Texture2D illuminationTexture;
Texture2D cloudShadowTexture;

SamplerState samplerState: register(s1)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

#include "../../CrossPlatform/cloud_shadow.sl"
#include "../../CrossPlatform/godrays.sl"

#define pi (3.1415926536)

struct atmosVertexInput
{
	uint vertex_id			: SV_VertexID;
};

struct atmosVertexOutput
{
    float4 position			: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
    float2 pos				: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(atmosVertexInput IN)
{
	atmosVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.pos			=poss[IN.vertex_id];
	OUT.position	=float4(OUT.pos,0.0,1.0);
	// Set to far plane so can use depth test as want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.pos.x,-OUT.pos.y));
	OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

float4 PS_AtmosOverlayLossPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view	=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view		=normalize(view);
	float depth	=depthTexture.Sample(clampSamplerState,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias)).x;
	float dist	=depthToFadeDistance(depth,depthToLinFadeDistParams,nearZ,farZ,IN.pos.xy,tanHalfFov);
	float sine	=view.z;
	float2 texc2=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float3 loss	=lossTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(loss.rgb,1.f);
}

vec4 PS_AtmosOverlayInscPass(atmosVertexOutput IN) : SV_TARGET
{
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float depth			=depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float dist			=depthToFadeDistance(depth,depthToLinFadeDistParams,nearZ,farZ,IN.pos.xy,tanHalfFov);
	float sine			=view.z;
	float2 fade_texc	=vec2(pow(dist,0.5f),0.5f*(1.f-sine));

	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);

	vec4 insc			=vec4(insc_far.rgb-insc_near.rgb,insc_far.a);//0.5*(insc_near.a+insc_far.a));
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	//colour				=illum_lookup.z;
	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	colour				*=exposure;
    return float4(colour.rgb,1.f);
}

// Slanted Cylinder whose axis is along lightDir,
// radius is at the specified horizontal distance.
// Distance c is:		c=|lightDir.z*R|/|lightDir * sine - view * lightDir.z|
float4 PS_AtmosOverlayGodraysPass(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float solid_depth	=depthTexture.Sample(clampSamplerState,depth_texc).x;
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,depthToLinFadeDistParams,nearZ,farZ,IN.pos.xy,tanHalfFov);
	return Godrays(inscTexture,overcTexture,IN.pos,invViewProj,maxDistance,solid_dist);
}

technique11 simul_atmospherics_overlay
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		//SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayLossPass()));
    }
    pass p1
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayInscPass()));
    }
}

technique11 simul_godrays
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayGodraysPass()));
    }
}

