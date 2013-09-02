#include "CppHlsl.hlsl"
#include "states.hlsl"
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float K(int l, int m) 
{ 
	// renormalisation constant for SH function 
	float temp = ((2.0*l+1.0)*factorial(l-m)) / (4.0*PI*factorial(l+m)); 
	return sqrt(temp); 
}
double P(int l,int m,float x) 
{ 
	// evaluate an Associated Legendre Polynomial P(l,m,x) at x 
	float pmm = 1.0; 
	if(m>0)
	{ 
		float somx2 = sqrt((1.0-x)*(1.0+x));
		float fact = 1.0; 
		for(int i=1; i<=m; i++)
		{ 
			pmm *= (-fact) * somx2; 
			fact += 2.0; 
		} 
	} 
	if(l==m)
		return pmm; 
	float pmmp1 = x * (2.0*m+1.0) * pmm; 
	if(l==m+1)
		return pmmp1; 
	float pll = 0.0; 
	for(int ll=m+2; ll<=l; ++ll)
	{ 
		pll = ( (2.0*ll-1.0)*x*pmmp1-(ll+m-1.0)*pmm ) / (ll-m); 
		pmm = pmmp1; 
		pmmp1 = pll; 
	} 
	return pll; 
}

float SH(int l, int m, float theta, float phi) 
{ 
 // return a point sample of a Spherical Harmonic basis function 
 // l is the band, range [0..N] 
 // m in the range [-l..l] 
 // theta in the range [0..Pi] 
 // phi in the range [0..2*Pi] 
 const float sqrt2 = sqrt(2.0); 
 if(m==0)
	 return K(l,0)*P(l,m,cos(theta)); 
 else if(m>0)
	 return sqrt2*K(l,m)*cos(m*phi)*P(l,m,cos(theta)); 
 else
	 return sqrt2*K(l,-m)*sin(-m*phi)*P(l,-m,cos(theta)); 
}

struct SHSample
{ 
	vec3 sph; 
	vec3 vec; 
	float coeff[36]; 
}; 
const float PI=3.1415926536;
void SH_setup_spherical_samples(SHSample samples[], int sqrt_n_samples) 
{ 
	// fill an N*N*2 array with uniformly distributed 
	// samples across the sphere using jittered stratification 
	int i=0; // array index 
	double oneoverN = 1.0/sqrt_n_samples; 
	for(int a=0; a<sqrt_n_samples; a++)
	{ 
		for(int b=0; b<sqrt_n_samples; b++)
		{ 
			// generate unbiased distribution of spherical coords 
			float x			= (a + rand(vec2(a,b))) * oneoverN; // do not reuse results 
			float y			= (b + rand(vec2(2*a,b))) * oneoverN; // each sample must be random 
			float theta		= 2.0 * acos(sqrt(1.0 - x)); 
			float phi		= 2.0 * PI * y; 
			samples[i].sph	= vec3(theta,phi,1.0); 
			// convert spherical coords to unit vector 
			vec3 vec		=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
			samples[i].vec	= vec; 
			// precompute all SH coefficients for this sample 
			for(int l=0; l<n_bands; ++l)
			{ 
				for(int m=-l; m<=l; ++m)
				{ 
					int index = l*(l+1)+m; 
					samples[i].coeff[index] = SH(l,m,theta,phi);
				}
			}
			i++;
		}
	}
}

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

