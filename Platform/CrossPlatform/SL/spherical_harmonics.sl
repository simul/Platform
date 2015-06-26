//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SPHERICAL_HARMONICS_SL
#define SPHERICAL_HARMONICS_SL

#define PI (3.1415926536)
/*float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}*/

float factorial(int j)
{
	float vals[]={1.0
				,1.0
				,2.0
				,6.0
				,24.0
				,120.0
				,720.0
				,5040.0
				,40320.0
				,362880.0
				,3628800.0
				,39916800.0
				,479001600.0
				,6227020800.0
				,87178291200.0
				,1307674368000.0
				,20922789888000.0};
	return vals[j];
}

float K(int l,int m) 
{ 
	static const float kval[]={0.282094792
							,0.488602512
							,0.345494149
							,0.630783131
							,0.257516135
							,0.128758067
							,0.746352665
							,0.215453456
							,0.068132365
							,0.027814922
							,0.846284375
							,0.189234939
							,0.044603103
							,0.011920681
							,0.004214597
							,0.93560258
							,0.170816879
							,0.032281356
							,0.006589404
							,0.001553137
							,0.000491145
							,1.017107236
							,0.156943054
							,0.024814876
							,0.004135813
							,0.000755093
							,0.000160986
							,4.64727E-05
							,1.092548431
							,0.145997925
							,0.019867801
							,0.002809731
							,0.000423583
							,7.05972E-05
							,1.38452E-05
							,3.7003E-06
							,1.163106623
							,0.13707343
							,0.016383409
							,0.002016658
							,0.000260349
							,3.6104E-05
							,5.57096E-06
							,1.01711E-06
							,2.54279E-07
							,1.22962269
							,0.129613612
							,0.013816857
							,0.001507543
							,0.000170696
							,2.0402E-05
							,2.63389E-06
							,3.80169E-07
							,6.51985E-08
							,1.53674E-08
							,1.292720736
							,0.123256086
							,0.011860322
							,0.001163
							,0.000117481
							,1.23836E-05
							,1.38452E-06
							,1.67898E-07
							,2.28481E-08
							,3.70644E-09
							,8.28786E-10
							,1.352879095
							,0.117753011
							,0.010327622
							,0.000920058
							,8.39894E-05
							,7.93625E-06
							,7.85806E-07
							,8.28312E-08
							,9.50139E-09
							,1.22662E-09
							,1.89272E-10
							,4.0353E-11
	};
	// renormalisation constant for SH function 
	//float temp = ((2.0*l+1.0)*factorial(l-m)) / (4.0*PI*factorial(l+m)); 
	//return sqrt(temp);
	int idx=l*(l+1)/2+abs(m);
	return kval[idx];
}

// evaluate an Associated Legendre Polynomial P(l,m,x) at x 
float P(int l,int m,float x) 
{ 
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
		 return K(l,0)*float(P(l,m,cos(theta))); 
	 else if(m>0)
		 return sqrt2*K(l, m)*cos(m*phi)*P(l, m, cos(theta));
	 else
		 return sqrt2*K(l,-m)*sin(-m*phi)*P(l, -m, cos(theta));
}
#ifndef GLSL
void SH_setup_spherical_samples(RWStructuredBuffer<SphericalHarmonicsSample> samplesBufferRW,int2 pos
	,int sqrt_n_samples
	,int n_bands) 
{ 
	// fill an N*N*2 array with uniformly distributed 
	// samples across the sphere using jittered stratification 
	float oneoverN			= 1.0/sqrt_n_samples; 
	int a=pos.x;
	int b=pos.y;
	int i					=a*sqrt_n_samples+b; // array index 
	// generate unbiased distribution of spherical coords 
	float x					=(a + rand(vec2(a,b))) * oneoverN; // do not reuse results 
	float y					=(b + rand(vec2(2.45*a,11.1*b))) * oneoverN; // each sample must be random 
	float theta				=2.0 * acos(sqrt(1.0 - x)); 
	float phi				=2.0 * PI * y; 
	// convert spherical coords to unit vector 
	vec3 vec				=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
	samplesBufferRW[i].dir	= vec; 
	// precompute all SH coefficients for this sample 
	int n = 0;
	for (int l = 0; l<MAX_SH_BANDS; l++)
	{
		if (l >= n_bands)
			break;
		for(int m=-l; m<=l; m++)
		{ 
//			int index = l*(l+1)/2+m; 
			samplesBufferRW[i].coeff[n++] = SH(l, m, theta, phi);
		}
	}
}
#endif

#endif