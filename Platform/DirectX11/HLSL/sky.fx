#include "CppHLSL.hlsl"
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
RWTexture2D<vec4> targetTexture;
Texture2D lightTable2DTexture;

cbuffer cbPerObject : register(b11)
{
	vec4 rect;
};

#include "states.hlsl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/SL/earth_shadow.sl"
#include "../../CrossPlatform/SL/sky_constants.sl"
#include "../../CrossPlatform/SL/illumination.sl"
#include "../../CrossPlatform/SL/sky.sl"

struct vertexInput
{
    vec3 position			: POSITION;
};

struct vertexOutput
{
    vec4 hPosition		: SV_POSITION;
    vec3 wDirection		: TEXCOORD0;
};

struct vertexInput3Dto2D
{
    vec3 position			: POSITION;
    vec2 texCoords		: TEXCOORD0;
};

struct vertexOutput3Dto2D
{
    vec4 hPosition		: SV_POSITION;
    vec2 texCoords		: TEXCOORD0;
};

//------------------------------------
// Vertex Shader 
//------------------------------------
vertexOutput VS_Main(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition	=mul(worldViewProj,vec4(IN.position.xyz,1.0));
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

vertexOutput VS_Cubemap(vertexInput IN) 
{
    vertexOutput OUT;
	// World matrix would be identity.
    OUT.hPosition	=vec4(IN.position.xyz,1.0);
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh	=CalcRayleighBeta(cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal		=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 result			=BetaTotal*inscatter_factor.rgb;
	return result;
}

vec4 PS_IlluminationBuffer(vertexOutput3Dto2D IN): SV_TARGET
{
	float alt_km		=eyePosition.z/1000.0;
	return IlluminationBuffer(alt_km,IN.texCoords,targetTextureSize,overcastBaseKm,overcastRangeKm,maxFadeDistanceKm
			,maxFadeDistance,terminatorDistance,radiusOnCylinder,earthShadowNormal,sunDir);
}

vertexOutput3Dto2D VS_Fade3DTo2D(idOnly IN) 
{
    vertexOutput3Dto2D OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(vec2(-1.0,-1.0)+2.0*pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=pos;
    return OUT;
}

vertexOutput3Dto2D VS_ShowFade(idOnly IN) 
{
    vertexOutput3Dto2D OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
	OUT.texCoords	=vec2(pos.x,1.0-pos.y);
    return OUT;
}

vec4 PS_Fade3DTo2D(vertexOutput3Dto2D IN): SV_TARGET
{
	vec3 texc=vec3(altitudeTexCoord,1.0-IN.texCoords.y,IN.texCoords.x);
	vec4 colour1=fadeTexture1.Sample(cmcSamplerState,texc);
	vec4 colour2=fadeTexture2.Sample(cmcSamplerState,texc);
	vec4 result=lerp(colour1,colour2,skyInterp);
    return result;
}

vec4 PS_OvercastInscatter(vertexOutput3Dto2D IN): SV_TARGET
{
	float alt_km		=eyePosition.z/1000.0;
	// Texcoords representing the full distance from the eye to the given point.
    return OvercastInscatter(inscTexture,illuminationTexture,IN.texCoords,alt_km,maxFadeDistanceKm,overcast,overcastBaseKm,overcastRangeKm,targetTextureSize);
}

vec4 PS_SkylightAndOvercast3Dto2D(vertexOutput3Dto2D IN): SV_TARGET
{
	vec3 texc=vec3(altitudeTexCoord,1.0-IN.texCoords.y,IN.texCoords.x);
	vec4 colour1=fadeTexture1.Sample(cmcSamplerState,texc);
	vec4 colour2=fadeTexture2.Sample(cmcSamplerState,texc);
	vec4 result;
	result.rgb=lerp(colour1.rgb,colour2.rgb,skyInterp);
	result.a=1.0;
    return result;
}

vec4 PS_ShowFadeTable(vertexOutput3Dto2D IN): SV_TARGET
{
	vec4 result=inscTexture.Sample(cmcSamplerState,IN.texCoords.xy);
	result.rb+=overlayAlpha*result.a;
    return vec4(result.rgb,1);
}

vec4 PS_Colour(vertexOutput3Dto2D IN): SV_TARGET
{
    return colour;
}

vec4 PS_ShowIlluminationBuffer(vertexOutput3Dto2D IN): SV_TARGET
{
	return ShowIlluminationBuffer(inscTexture,IN.texCoords);
}

vec4 PS_ShowFadeTexture(vertexOutput3Dto2D IN): SV_TARGET
{
	vec4 result=fadeTexture1.Sample(cmcSamplerState,vec3(altitudeTexCoord,IN.texCoords.yx));
    return vec4(result.rgb,1);
}

vec4 PS_Show3DLightTable(vertexOutput3Dto2D IN): SV_TARGET
{
	vec4 result=fadeTexture1.Sample(samplerStateNearest,vec3(IN.texCoords.y,(float(cycled_index)+.5)/3.0,IN.texCoords.x));
    return vec4(result.rgb,1);
}

vec4 PS_Show2DLightTable(vertexOutput3Dto2D IN): SV_TARGET
{
	vec4 result=texture_nearest_lod(lightTable2DTexture,IN.texCoords.yx,0);

    return vec4(result.rgb,1);
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
    vec4 hPosition		: SV_POSITION;
    vec2 tex				: TEXCOORD0;
};

struct starsVertexOutput
{
    vec4 hPosition		: SV_POSITION;
    float tex				: TEXCOORD0;
};

svertexOutput VS_Sun(indexVertexInput IN) 
{
    svertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec3 pos=vec3(poss[IN.vertex_id],1.0/tan(radiusRadians));
    OUT.hPosition=mul(worldViewProj,vec4(pos,1.0));
	// Set to far plane so can use depth test as want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z = 0.0f; 
#else
	OUT.hPosition.z = OUT.hPosition.w; 
#endif
    OUT.tex=pos.xy;
    return OUT;
}

struct starsVertexInput
{
    vec3 position			: POSITION;
    float tex				: TEXCOORD0;
};

starsVertexOutput VS_Stars(starsVertexInput IN) 
{
    starsVertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,vec4(IN.position.xyz,1.0));

	// Set to far plane so can use depth test as want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z = 0.0f; 
#else
	OUT.hPosition.z = 1.0f; 
#endif
    OUT.tex=IN.tex;
    return OUT;
}

vec4 PS_Stars( starsVertexOutput IN): SV_TARGET
{
	vec3 colour=vec3(1.0,1.0,1.0)*clamp(starBrightness*IN.tex,0.0,1.0);
	return vec4(colour,1.0);
}

// Sun could be overbright. So the colour is in the range [0,1], and a brightness factor is
// stored in the alpha channel.
vec4 PS_Sun( svertexOutput IN): SV_TARGET
{
	float r=2.0*length(IN.tex);
	if(r>2.0)
		discard;
	float brightness=1.0;
	if(r>1.0)
	//	discard;
		brightness=colour.a/pow(r,4.0);//+colour.a*saturate((0.9-r)/0.1);
	vec3 result=brightness*colour.rgb;
	return vec4(result,1.f);
}

vec4 PS_SunQuery( svertexOutput IN): SV_TARGET
{
	float r=2.0*length(IN.tex);
	if(r>1.0)
		discard;
	return vec4(1.0,0.0,0.0,1.0);
}

vec4 PS_Flare( svertexOutput IN): SV_TARGET
{
	vec3 output=colour.rgb*flareTexture.Sample(flareSamplerState,vec2(.5f,.5f)+0.5f*IN.tex).rgb;

	return vec4(output,1.f);
}

vec4 PS_Planet(svertexOutput IN): SV_TARGET
{
	vec4 result=flareTexture.Sample(flareSamplerState,vec2(.5f,.5f)-0.5f*IN.tex);
	// IN.tex is +- 1.
	vec3 normal;
	normal.x=IN.tex.x;
	normal.y=IN.tex.y;
	float l=length(IN.tex);
	if(l>1.0)
		return vec4(0,0.0,0,1.0);
	//	discard;
	normal.z	=-sqrt(1.0-l*l);
	float light	=approx_oren_nayar(0.2,vec3(0,0,1.0),normal,lightDir.xyz);
	result.rgb	*=colour.rgb;
	result.rgb	*=light;
	result.a	*=saturate((0.97-l)/0.03);
	return result;
}

technique11 show_fade_table
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowFadeTable()));
    }
}

technique11 show_illumination_buffer
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowIlluminationBuffer()));
    }
}

technique11 show_fade_texture
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowFadeTexture()));
    }
}

technique11 colour_technique
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Colour()));
    }
}

technique11 show_light_table
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
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
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Show2DLightTable()));
    }
}

technique11 fade_3d_to_2d
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Fade3DTo2D()));
    }
}

technique11 show_cross_section
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_ShowFade()));
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
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
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
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
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
		SetBlendState(DontBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_IlluminationBuffer()));
    }
}

technique11 stars
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Stars()));
		SetPixelShader(CompileShader(ps_4_0,PS_Stars()));
		SetDepthStencilState( TestDepth, 0 );
		SetBlendState(AddBlend, vec4(1.0f,1.0f,1.0f,1.0f), 0xFFFFFFFF );
    }
}

technique11 sun
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Sun()));
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AddBlend,vec4(1.0f,1.0f,1.0f,1.0f), 0xFFFFFFFF );
    }
}

technique11 query
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_SunQuery()));
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AddBlend,vec4(1.0f,1.0f,1.0f,1.0f), 0xFFFFFFFF );
    }
}

technique11 flare
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Flare()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend,vec4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF );
    }
}

technique11 planet
{
    pass p0
    {		
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Planet()));
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AlphaBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 interp_light_table
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_InterpLightTable()));
    }
}
