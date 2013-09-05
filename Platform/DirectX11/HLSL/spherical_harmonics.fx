#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/spherical_harmonics_constants.sl"
// The cubemap input we are creating coefficients for.
TextureCube cubeTexture;
// A texture (l+1)^2 of coefficients.
RWStructuredBuffer<float4> targetBuffer;
// A buffer of nxn random sample positions. The higher res, the more accurate.
RWStructuredBuffer<SphericalHarmonicsSample> samplesBuffer;
static const float PI=3.1415926536;
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float factorial(int j)
{
	float vals[]={1.0,1.0,2.0,6.0,24.0,120.0,720.0,5040.0,40320.0,362880.0
				,3628800.0
				,39916800.0
				,479001600.0
				,6227020800.0
				,87178291200.0
				,1307674368000.0
				,20922789888000.0};
	return vals[j];
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

void SH_setup_spherical_samples(int2 pos,int sqrt_n_samples,int n_bands) 
{ 
	// fill an N*N*2 array with uniformly distributed 
	// samples across the sphere using jittered stratification 
	double oneoverN = 1.0/sqrt_n_samples; 
	int a=pos.x;
	int b=pos.y;
	int i=a*4+b; // array index 
	// generate unbiased distribution of spherical coords 
	float x		=(a + rand(vec2(a,b))) * oneoverN; // do not reuse results 
	float y		=(b + rand(vec2(2*a,b))) * oneoverN; // each sample must be random 
	float theta	=2.0 * acos(sqrt(1.0 - x)); 
	float phi	=2.0 * PI * y; 
	samplesBuffer[i].theta=theta;
	samplesBuffer[i].phi	=phi;
	// convert spherical coords to unit vector 
	vec3 vec		=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
	samplesBuffer[i].dir	= vec; 
	// precompute all SH coefficients for this sample 
	for(int l=0; l<n_bands; ++l)
	{ 
		for(int m=-l; m<=l; ++m)
		{ 
			int index = l*(l+1)+m; 
			samplesBuffer[i].coeff[index] = SH(l,m,theta,phi);
		}
	}
}

[numthreads(1,1,1)]
void CS_Jitter(uint3 sub_pos: SV_DispatchThreadID )
{
	SH_setup_spherical_samples(sub_pos.xy,16,3);
}

[numthreads(1,1,1)]
void CS_Encode(uint3 sub_pos: SV_DispatchThreadID )
{
	SphericalHarmonicsSample sample=samplesBuffer[sub_pos.x];
	// The sub_pos gives the co-ordinate in the table of samples.
	vec4 colour		=cubeTexture.SampleLevel(wrapSamplerState,sample.dir,0);
	const double weight = 4.0*PI; 
	// for each sample 
	
	double theta = sample.theta; 
	double phi = sample.phi; 
	// divide the result by weight and number of samples 
	double factor = weight / 1024.0; 
	for(int n=0; n<16; ++n)
	{ 
		targetBuffer[n] += colour * sample.coeff[n]*factor; 
	} 
}

technique11 jitter
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Jitter()));
    }
}

technique11 encode
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Encode()));
    }
}

