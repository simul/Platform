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
Texture3D sourceTexture;
RWTexture2D<float4> targetTexture;
Texture2D lightTable2DTexture;

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
	float BetaRayleigh	=CalcRayleighBeta(cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal	=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 result		=BetaTotal*inscatter_factor.rgb;
	return result;
}

float4 PS_IlluminationBuffer(vertexOutput3Dto2D IN): SV_TARGET
{
	float alt_km		=eyePosition.z/1000.0;
	return IlluminationBuffer(alt_km,IN.texCoords,targetTextureSize,overcastBaseKm,overcastRangeKm,maxFadeDistanceKm
			,maxFadeDistance,terminatorDistance,radiusOnCylinder,earthShadowNormal,sunDir);
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

vec4 PS_OvercastInscatter(vertexOutput3Dto2D IN): SV_TARGET
{
	float alt_km		=eyePosition.z/1000.0;
	// Texcoords representing the full distance from the eye to the given point.
	vec2 fade_texc	=vec2(IN.texCoords.x,1.0-IN.texCoords.y);
    return OvercastInscatter(inscTexture,illuminationTexture,fade_texc,alt_km,maxFadeDistanceKm,overcast,overcastBaseKm,overcastRangeKm,targetTextureSize);
}

float4 PS_SkylightAndOvercast3Dto2D(vertexOutput3Dto2D IN): SV_TARGET
{
	float3 texc=float3(altitudeTexCoord,1.0-IN.texCoords.y,IN.texCoords.x);
	float4 colour1=fadeTexture1.Sample(cmcSamplerState,texc);
	float4 colour2=fadeTexture2.Sample(cmcSamplerState,texc);
	float4 result;
	result.rgb=lerp(colour1.rgb,colour2.rgb,skyInterp);
	result.a=1.0;
    return result;
}

vertexOutput3Dto2D VS_ShowFade(idOnly IN) 
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

float4 PS_ShowIlluminationBuffer(vertexOutput3Dto2D IN): SV_TARGET
{
	return ShowIlluminationBuffer(inscTexture,IN.texCoords);
}

float4 PS_ShowFadeTexture(vertexOutput3Dto2D IN): SV_TARGET
{
	float4 result=fadeTexture1.Sample(cmcSamplerState,float3(altitudeTexCoord,IN.texCoords.yx));
    return float4(result.rgb,1);
}

float4 PS_Show3DLightTable(vertexOutput3Dto2D IN): SV_TARGET
{
	float4 result=fadeTexture1.Sample(samplerStateNearest,vec3(IN.texCoords.y,(float(cycled_index)+.5)/3.0,IN.texCoords.x));
    return float4(result.rgb,1);
}

float4 PS_Show2DLightTable(vertexOutput3Dto2D IN): SV_TARGET
{
	float4 result=texture_nearest_lod(lightTable2DTexture,IN.texCoords.yx,0);

    return float4(result.rgb,1);
}

[numthreads(1,1,1)]
void CS_InterpLightTable(uint3 pos	: SV_DispatchThreadID )
{
	uint3 dims;
	sourceTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.z)
		return;
	float alt_texc_x	=float(pos.x)/float(dims.x);
	float which_texc	=(float(pos.y)+0.5)/float(dims.z);
	vec3 texc_3a		=vec3(alt_texc_x,(float( cycled_index  )   +0.5)/3.0,which_texc);
	vec3 texc_3b		=vec3(alt_texc_x,(float((cycled_index+1)%3)+0.5)/3.0,which_texc);
	vec4 colour1		=texture_nearest_lod(sourceTexture,texc_3a,0);
	vec4 colour2		=texture_nearest_lod(sourceTexture,texc_3b,0);
	vec4 colour			=lerp(colour1,colour2,skyInterp);
	// Apply earth shadow to sunlight.
	//colour				*=saturate(alt_texc_x-illumination_alt_texc);
	targetTexture[pos.xy]	=colour;
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

float4 PS_SunQuery( svertexOutput IN): SV_TARGET
{
	float r=2.0*length(IN.tex);
	if(r>1.0)
		discard;
	return float4(1.0,0.0,0.0,1.0);
}
float4 PS_Flare( svertexOutput IN): SV_TARGET
{
	float3 output=colour.rgb*flareTexture.Sample(flareSamplerState,float2(.5f,.5f)+0.5f*IN.tex).rgb;

	return float4(output,1.f);
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
		return vec4(0,0.0,0,1.0);
	//	discard;
	normal.z	=-sqrt(1.0-l*l);
	float light	=approx_oren_nayar(0.2,float3(0,0,1.0),normal,lightDir.xyz);
	result.rgb	*=colour.rgb;
	result.rgb	*=light;
	result.a	*=saturate((0.97-l)/0.03);
	return result;
}

technique11 simul_show_sky_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowSkyTexture()));
    }
}

technique11 show_illumination_buffer
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowIlluminationBuffer()));
    }
}

technique11 simul_show_fade_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowFadeTexture()));
    }
}

technique11 show_light_table
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Show3DLightTable()));
    }
}

technique11 show_2d_light_table
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Show2DLightTable()));
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
 
technique11 overcast_inscatter
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_OvercastInscatter()));
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

technique11 illumination_buffer
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
		SetDepthStencilState( TestDepth, 0 );
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
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AddBlend,float4(1.0f,1.0f,1.0f,1.0f), 0xFFFFFFFF );
    }
}


technique11 sun_query
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_SunQuery()));
		SetDepthStencilState(TestDepth,0);
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

technique11 simul_planet
{
    pass p0 
    {		
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Planet()));
		SetDepthStencilState( TestDepth, 0 );
		SetBlendState(AlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 interp_light_table
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_InterpLightTable()));
    }
}