#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/spherical_harmonics_constants.sl"
#include "../../CrossPlatform/spherical_harmonics.sl"
#include "../../CrossPlatform/light_probe_constants.sl"

// A texture (l+1)^2 of basis coefficients.
StructuredBuffer<float4> basisBuffer;

// Coefficients for 
//A_0 = 3.141593	0,0
//A_1 = 2.094395	1,-1 1,0 1,1
//A_2 = 0.785398	2,-2 2,-1 2,0 2,1 2,2
//A_3 = 0			3,-3 3,-2 3,-1 3,0 3,1 3,2 3,3
//A_4 = -0.130900 
//A_5 = 0 
//A_6 = 0.049087 

static float A[]={ 3.141593
				  , 2.094395
				  , 0.785398
				  , 0		
				  , -0.130900
				  , 0 
				  , 0.049087 };

vec4 PS_IrradianceMap(posTexVertexOutput IN) : SV_TARGET
{
	vec4 clip_pos	=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x		+=2.0*IN.texCoords.x;
	clip_pos.y		-=2.0*IN.texCoords.y;
	vec3 view		=mul(invViewProj,clip_pos).xyz;
	view			=normalize(view);
	// convert spherical coords to unit vector 
	//	vec3 vec		=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
	// Therefore as atan2(y,x) is the angle from the X-AXIS:
	float theta		=acos(view.z);
	float phi		=atan2(view.y,view.x);
	// SH(int l, int m, float theta, float phi) is the basis function/
	// return a point sample of a Spherical Harmonic basis function 
	vec4 result		=vec4(0,0,0,0);
	int n=0;
	for(int l=0;l<4;l++)
	{ 
		for(int m=-l;m<=l;m++)
			result+=basisBuffer[0];//SH(l,m,theta,phi)*A[l]*basisBuffer[n++];
	} 
	return result;
}

technique11 irradiance_map
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
		SetPixelShader(CompileShader(ps_5_0,PS_IrradianceMap()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
