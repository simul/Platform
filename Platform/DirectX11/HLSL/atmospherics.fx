#include "CppHlsl.hlsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "states.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"

Texture2D depthTexture;
Texture2D imageTexture;
Texture2D lossTexture;
Texture2D inscTexture;
Texture2D skylTexture;
Texture2D illuminationTexture;
Texture2D cloudShadowTexture;

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

float4 PS_Atmos(atmosVertexOutput IN) : SV_TARGET
{
	float3 view		=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	float2 viewportTexCoord = viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float4 lookup	=imageTexture.Sample(clampSamplerState,viewportTexCoord);
	float4 dlookup	=depthTexture.Sample(clampSamplerState,viewportTexCoord);
	float3 colour	=lookup.rgb;
	float depth		=dlookup.r;
	float dist		=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
#ifdef REVERSE_DEPTH
	if(depth<=0.0)
		dist=1.0;
#else
	if(depth>=1.0)
		dist=1.0;
#endif
	float sine		=view.z;
	float2 texc2	=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float3 loss		=lossTexture.Sample(clampSamplerState,texc2).rgb;
	colour			*=loss;
	float4 inscatter_factor=inscTexture.Sample(clampSamplerState,texc2);
	float cos0		=dot(view.xyz,lightDir.xyz);
	colour			+=InscatterFunction(inscatter_factor,hazeEccentricity,cos0,mieRayleighRatio);
	colour			+=skylTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(colour.rgb,1.f);
}

float4 PS_AtmosOverlayLossPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view	=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view		=normalize(view);
	float depth	=depthTexture.Sample(clampSamplerState,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias)).x;
	float dist	=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
	float sine	=view.z;
	float2 texc2=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float3 loss	=lossTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(loss.rgb,1.f);
}

float4 PS_AtmosOverlayInscPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	float depth			=depthTexture.Sample(clampSamplerState,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias)).x;
	float dist			=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
	float sine			=view.z;
	float2 fade_texc	=float2(pow(dist,0.5f),0.5f*(1.f-sine));

	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illuminationTexture,illum_texc).rg;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc			=insc_far-insc_near;

	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	colour				*=exposure;
    return float4(colour.rgb,1.f);
}

// Slanted Cylinder whose axis is along lightDir,
// radius is at the specified horizontal distance.
// Distance c is:
//					c=|lightDir.z*R|/|lightDir * sine - view * lightDir.z|
float4 PS_AtmosOverlayGodraysPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	float sine			=view.z;
	float cos0			=dot(view,lightDir);
	float depth			=depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float dist			=1.0;//depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);

	vec4 total_insc		=vec4(0,0,0,0);
	#define C 12
	float retain		=(float(C)-1.0)/float(C);
	float r_max			=80000.0/maxDistance;
	float u_max			=sqrt(r_max);
	float r0			=r_max;
	float u0			=abs(lightDir.z*r0)/length(view*lightDir.z-lightDir*view.z);
	vec2 fade_texc		=vec2(sqrt(min(dist,u0)),0.5*(1.0-sine));
	vec4 insc0			=texture_wrap_mirror(inscTexture,fade_texc);
	float rem=1.0;
	for(int i=0;i<C;i++)
	{
		float r1	=r0;
		r0			=u_max*((float(C)-float(i)-1.0)/float(C));
		r0			=pow(r0,2.0);
		float u1	=u0;
		u0			=abs(lightDir.z*r0)/length(view*lightDir.z-lightDir*view.z);
		float u		=0.5*(u0+u1);
		float eff	=exp(-u/u_max);
		if(u<dist)
		{
			fade_texc.x			=sqrt(u0);
			float d				=u*maxDistance;
			float ill			=GetIlluminationAt(viewPosition+view*d);
			float shadow		=eff*(1.0-ill);
			rem					*=ill;
			vec4 insc1			=insc0;
			insc0				=texture_wrap_mirror(inscTexture,fade_texc);
			vec4 insc_diff		=max(insc1-insc0,vec4(0,0,0,0));
			total_insc.rgb		+=insc_diff.rgb*shadow;
			total_insc.a		*=retain;
			total_insc.a		+=insc_diff.a*shadow;
		}
	}
	vec3 gr=-InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio).rgb;
	gr=min(gr,vec3(0.0,0.0,0.0));
	return vec4(gr,0.0);
}


technique11 simul_atmospherics
{
    pass p0
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(NoBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_Atmos()));
    }
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
    pass godrays
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayGodraysPass()));
    }
}

