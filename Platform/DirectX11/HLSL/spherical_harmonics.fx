#include "CppHlsl.hlsl"
#include "states.hlsl"

technique11 encode
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_SimpleFullscreen()));
		SetPixelShader(CompileShader(ps_4_0,EncodePS()));
    }
}

