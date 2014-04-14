#include "CppHLSL.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/optics_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"

Texture2D flareTexture;
Texture2D rainbowLookupTexture;
Texture2D coronaLookupTexture;
Texture2D moistureTexture;
Texture2D depthTexture;

struct indexVertexInput
{
	uint vertex_id			: SV_VertexID;
};

struct svertexOutput
{
    vec4 hPosition		: SV_POSITION;
    vec2 tex				: TEXCOORD0;
};

svertexOutput VS_Flare(indexVertexInput IN) 
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
#ifdef REVERSE_DEPTH
	OUT.hPosition.z = 0.0f; 
#else
	OUT.hPosition.z = OUT.hPosition.w; 
#endif
    OUT.tex=pos.xy;
    return OUT;
}

vec4 PS_Flare( svertexOutput IN): SV_TARGET
{
	vec3 output=colour.rgb*texture_clamp(flareTexture,vec2(.5f,.5f)+0.5f*IN.tex).rgb;

	return vec4(output.rgb,1.f);
}


struct rainbowVertexOutput
{
    vec4 hPosition	: SV_POSITION;
    vec2 texCoords	: TEXCOORD0;	// quad texture coordinates
    vec3 view		: TEXCOORD1;	// eye vector
};

rainbowVertexOutput VS_rainbow(idOnly id)
{
    posTexVertexOutput posTex=VS_SimpleFullscreen(id);
	
	rainbowVertexOutput OUT;
	OUT.hPosition = posTex.hPosition;
	OUT.texCoords = posTex.texCoords;
	//  input is a full screen quad
	vec2 clip_pos	=OUT.hPosition.xy;
	OUT.view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
    return OUT;
}

vec4 CalculateRainbowColor(rainbowVertexOutput IN, float d, out vec4 moisture )
{
/*
 NB:
  -the lookup texture should be blurred by the suns angular size 0.5 degrees.
   this should be baked into the texture

  -rainbow light blends additively to existing light in the scene.
    aka current scene color + rainbow color
    aka add blend, one, one
    
  -horizontal thickness of moisture, 
  	a thin sheet of rain will produce less bright rainbows than a thick sheet
  	aka rainbow color  * water amount, where water amount ranges from 0 to 1
  
  -rainbow light can be scattered and absorbed by other atmospheric particles.
    aka simplified..rainbow color * light color
    
*/
	//d will be clamped between 0 and 1 by the texture sampler
	// this gives up the dot product result in the range of [0 to 1]
	// that is to say, an angle of 0 to 90 degrees
	vec4 scattered	=texture_clamp(rainbowLookupTexture, vec2( dropletRadius, d));
	moisture		=texture_clamp(moistureTexture,IN.texCoords);
	return scattered;
}

vec4 PS_rainbowOnly(rainbowVertexOutput IN) : SV_TARGET
{
	float d=  -dot( lightDir,normalize(IN.view ) 	);
	// view must be normalized per pixel to prevent banding
  
 	vec4 moisture; 
	vec4 scattered=CalculateRainbowColor(IN, d,  moisture );	
	return scattered*rainbowIntensity*moisture.x;
}

vec4 PS_rainbowAndCorona(rainbowVertexOutput IN) : SV_TARGET
{
	//return texture_clamp(coronaLookupTexture,IN.texCoords.xy);
	 //note: use a float for d here, since a half corrupts the corona
	float d=  -dot( lightDir,normalize(IN.view ) 	);
	
	vec4 moisture;

	vec4 scattered=CalculateRainbowColor(IN, d,  moisture );	

	//(1 + d) will be clamped between 0 and 1 by the texture sampler
	// this gives up the dot product result in the range of [-1 to 0]
	// that is to say, an angle of 90 to 180 degrees
	vec4 coronaDiffracted = texture_clamp(coronaLookupTexture, vec2(dropletRadius, 1.0 + d));

	return vec4(.1,0,0,0);//(coronaDiffracted + scattered)*rainbowIntensity*moisture.x;
}

technique11 simul_flare
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Flare()));
		SetPixelShader(CompileShader(ps_4_0,PS_Flare()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 rainbow_and_corona
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_rainbow()));
		SetPixelShader(CompileShader(ps_4_0,PS_rainbowAndCorona()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}