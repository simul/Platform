#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/states.sl"
Texture2D fontTexture;

cbuffer FontConstants
{
	vec4	rect;
	vec4	texc;
	vec4	colour;
	vec4	background;
};

posTexVertexOutput FontVertexShader(idOnly IN)
{
    posTexVertexOutput output=VS_ScreenQuad(IN,rect);
	output.texCoords=vec4(texc.xy+texc.zw*output.texCoords,0.0,1.0);
    return output;
}

vec4 FontPixelShader(posTexVertexOutput input) : SV_TARGET
{
	vec4 result	=background;
	vec4 lookup	=texture_nearest_lod(fontTexture,input.texCoords,0);
	lookup.a	=lookup.r;
	lookup		*=colour;
    return lookup;
}

vec4 FontBackgroundShader(posTexVertexOutput input) : SV_TARGET
{
    return background;
}

technique11 backg
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend,vec4( 0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FontVertexShader()));
		SetPixelShader(CompileShader(ps_4_0,FontBackgroundShader()));
    }
}



technique11 text
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend,vec4( 0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FontVertexShader()));
		SetPixelShader(CompileShader(ps_4_0,FontPixelShader()));
    }
}

