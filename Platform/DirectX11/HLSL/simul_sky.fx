#define pi (3.1415926536f)

Texture2D inscTexture;
Texture2D skylTexture;
Texture2D lossTexture;
Texture2D illuminationTexture;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

Texture2D flareTexture;
SamplerState flareSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

Texture3D fadeTexture1;
Texture3D fadeTexture2;

cbuffer cbPerObject : register(b11)
{
	float4 rect;
};

#include "CppHLSL.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/sky_constants.sl"
#include "../../CrossPlatform/illumination.sl"

struct vertexInput
{
    float3 position			: POSITION;
};

struct vertexOutput
{
    float4 hPosition		: SV_POSITION;
    float3 wDirection		: TEXCOORD0;
};

struct vertexInput3Dto2D
{
    float3 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutput3Dto2D
{
    float4 hPosition		: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
};

//------------------------------------
// Vertex Shader 
//------------------------------------
vertexOutput VS_Main(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition	=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

vertexOutput VS_Cubemap(vertexInput IN) 
{
    vertexOutput OUT;
	// World matrix would be identity.
    OUT.hPosition	=float4(IN.position.xyz,1.0);
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh	=0.0596831f*(1.f+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal	=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 result		=BetaTotal*inscatter_factor.rgb;
	return result;
}

float4 PS_IlluminationBuffer(vertexOutput3Dto2D IN): SV_TARGET
{
	float alt_km		=eyePosition.z/1000.0;
	return IlluminationBuffer(alt_km,IN.texCoords,targetTextureSize,overcastBaseKm,overcastRangeKm,maxFadeDistanceKm);
}

vertexOutput3Dto2D VS_Fade3DTo2D(idOnly IN) 
{
    vertexOutput3Dto2D OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(float2(-1.0,-1.0)+2.0*pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=pos;
    return OUT;
}

float4 PS_Fade3DTo2D(vertexOutput3Dto2D IN): SV_TARGET
{
	float3 texc=float3(altitudeTexCoord,1.0-IN.texCoords.y,IN.texCoords.x);
	float4 colour1=fadeTexture1.Sample(cmcSamplerState,texc);
	float4 colour2=fadeTexture2.Sample(cmcSamplerState,texc);
	float4 result=lerp(colour1,colour2,skyInterp);
    return result;
}

vec4 PS_Overc3DTo2D(vertexOutput3Dto2D IN): SV_TARGET
{
	// Texcoords representing the full distance from the eye to the given point.
	vec2 fade_texc	=vec2(IN.texCoords.x,1.0-IN.texCoords.y);

    return OvercastInscatter(inscTexture,illuminationTexture,fade_texc,overcast);
}

float4 PS_SkylightAndOvercast3Dto2D(vertexOutput3Dto2D IN): SV_TARGET
{
	float3 texc=float3(altitudeTexCoord,1.0-IN.texCoords.y,IN.texCoords.x);
	float4 colour1=fadeTexture1.Sample(cmcSamplerState,texc);
	float4 colour2=fadeTexture2.Sample(cmcSamplerState,texc);
	float4 result;
	result.rgb=lerp(colour1.rgb,colour2.rgb,skyInterp);
	result.a=1.0;
	// Now the overcastedness 
    return result;
}

vertexOutput3Dto2D VS_ShowSkyTexture(idOnly IN) 
{
    vertexOutput3Dto2D OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(rect.xy+rect.zw*pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
	OUT.texCoords	=vec2(pos.x,1.0-pos.y);
    return OUT;
}

float4 PS_ShowSkyTexture(vertexOutput3Dto2D IN): SV_TARGET
{
	float4 result=inscTexture.Sample(cmcSamplerState,IN.texCoords.xy);
	//result.rgb*=result.a;
    return float4(result.rgb,1);
}

float4 PS_ShowFadeTexture(vertexOutput3Dto2D IN): SV_TARGET
{
	float4 result=fadeTexture1.Sample(cmcSamplerState,float3(altitudeTexCoord,IN.texCoords.yx));
    return float4(result.rgb,1);
}

struct indexVertexInput
{
	uint vertex_id			: SV_VertexID;
};

struct svertexOutput
{
    float4 hPosition		: SV_POSITION;
    float2 tex				: TEXCOORD0;
};

struct starsVertexOutput
{
    float4 hPosition		: SV_POSITION;
    float tex				: TEXCOORD0;
};

svertexOutput VS_Sun(indexVertexInput IN) 
{
    svertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec3 pos=vec3(poss[IN.vertex_id],1.0/tan(radiusRadians));
    OUT.hPosition=mul(worldViewProj,float4(pos,1.0));
	// Set to far plane so can use depth test as want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z = 0.0f; 
#else
	OUT.hPosition.z = OUT.hPosition.w; 
#endif
    OUT.tex=pos.xy;
    return OUT;
}

// Sun could be overbright. So the colour is in the range [0,1], and a brightness factor is
// stored in the alpha channel.
float4 PS_Sun( svertexOutput IN): SV_TARGET
{
	float r=2.0*length(IN.tex);
	if(r>2.0)
		discard;
	float brightness=1.0;
	if(r>1.0)
	//	discard;
		brightness=colour.a/pow(r,4.0);//+colour.a*saturate((0.9-r)/0.1);
	float3 result=brightness*colour.rgb;
	return float4(result,1.f);
}

float4 PS_Flare( svertexOutput IN): SV_TARGET
{
	float3 output=colour.rgb*flareTexture.Sample(flareSamplerState,float2(.5f,.5f)+0.5f*IN.tex).rgb;

	return float4(output,1.f);
}

struct starsVertexInput
{
    float3 position			: POSITION;
    float tex				: TEXCOORD0;
};

starsVertexOutput VS_Stars(starsVertexInput IN) 
{
    starsVertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));

	// Set to far plane so can use depth test as want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z = 0.0f; 
#else
	OUT.hPosition.z = 1.0f; 
#endif
    OUT.tex=IN.tex;
    return OUT;
}

float4 PS_Stars( starsVertexOutput IN): SV_TARGET
{
	float3 colour=float3(1.0,1.0,1.0)*clamp(starBrightness*IN.tex,0.0,1.0);
	return float4(colour,1.0);
}

float approx_oren_nayar(float roughness,float3 view,float3 normal,float3 lightDir)
{
	float roughness2 = roughness * roughness;
	float2 oren_nayar_fraction = roughness2 / (float2(roughness2,roughness2)+ float2(0.33, 0.09));
	float2 oren_nayar = float2(1, 0) + float2(-0.5, 0.45) * oren_nayar_fraction;
	// Theta and phi
	float2 cos_theta = saturate(float2(dot(normal, lightDir), dot(normal, view)));
	float2 cos_theta2 = cos_theta * cos_theta;
	float u=saturate((1-cos_theta2.x) * (1-cos_theta2.y));
	float sin_theta = sqrt(u);
	float3 light_plane = normalize(lightDir - cos_theta.x * normal);
	float3 view_plane = normalize(view - cos_theta.y * normal);
	float cos_phi = saturate(dot(light_plane, view_plane));
	// Composition
	float diffuse_oren_nayar = cos_phi * sin_theta / max(0.00001,max(cos_theta.x, cos_theta.y));
	float diffuse = cos_theta.x * (oren_nayar.x + oren_nayar.y * diffuse_oren_nayar);
	return diffuse;
}

float4 PS_Planet(svertexOutput IN): SV_TARGET
{
	float4 result=flareTexture.Sample(flareSamplerState,float2(.5f,.5f)-0.5f*IN.tex);
	// IN.tex is +- 1.
	float3 normal;
	normal.x=IN.tex.x;
	normal.y=IN.tex.y;
	float l=length(IN.tex);
	if(l>1.0)
		discard;
	normal.z	=-sqrt(1.0-l*l);
	float light	=approx_oren_nayar(0.2,float3(0,0,1.0),normal,lightDir.xyz);
	//result.rgb*=colour.rgb;
	result.rgb	*=light;
	result.a	*=saturate((0.99-l)/0.01);
	return result;
}

technique11 simul_show_sky_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowSkyTexture()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowSkyTexture()));
    }
}

technique11 simul_show_fade_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowSkyTexture()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowFadeTexture()));
    }
}

technique11 simul_fade_3d_to_2d
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Fade3DTo2D()));
    }
}
 
technique11 simul_overc_3d_to_2d
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Overc3DTo2D()));
    }
}
 
technique11 skylight_and_overcast
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_SkylightAndOvercast3Dto2D()));
    }
}

technique11 simul_illumination_buffer
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_IlluminationBuffer()));
    }
}

technique11 simul_stars
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Stars()));
		SetPixelShader(CompileShader(ps_4_0,PS_Stars()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(AddBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
    }
}


technique11 simul_sun
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Sun()));
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(AddBlend,float4(1.0f,1.0f,1.0f,1.0f), 0xFFFFFFFF );
    }
}


technique11 simul_flare
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Flare()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique11 simul_query
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Sun()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 simul_planet
{
    pass p0 
    {		
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Planet()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(AlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}