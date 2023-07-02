//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SPHERICAL_HARMONICS_SL
#define SPHERICAL_HARMONICS_SL

#define PI (3.1415926536)

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
	const float kval[]={0.282094792
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
	//float temp = ((2.0*l+1.0)*factorial(l-|m|)) / (4.0*PI*factorial(l+|m|)); 
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

float WindowFunction(float x)
{
	return saturate((0.0001+sin(SIMUL_PI_F*x))/(0.0001+SIMUL_PI_F*x));
}

float SH(int l, int m, float theta, float phi) 
{ 
 // return a point sample of a Spherical Harmonic basis function 
 // l is the band, range [0..N] 
 // m in the range [-l..l] 
 // theta in the range [0..Pi] 
 // phi in the range [0..2*Pi] 
	 const float sqrt2 = sqrt(2.0);
	 float s=0.0;
	 if(m==0)
		 s=K(l,0)*float(P(l,m,cos(theta))); 
	 else if(m>0)
		 s=sqrt2*K(l, m)*cos(m*phi)*P(l, m, cos(theta));
	 else
		 s=sqrt2*K(l,-m)*sin(-m*phi)*P(l, -m, cos(theta));
	 //s*=WindowFunction(float(l)/4.0);
	 return s;
}


vec4 EvaluateSH(vec3 view,vec4 coefficients[9],int numSHBands)
{
	// convert spherical coords to unit vector 
	//	vec3 vec		=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
	// Therefore as atan2(y,x) is the angle from the X-AXIS:
	float theta		=acos(view.z);
	float phi		=atan2(view.y,view.x);
	// SH(int l, int m, float theta, float phi) is the basis function/
	// return a point sample of a Spherical Harmonic basis function 
	vec4 result		=vec4(0,0,0,0);
	// Coefficients for 
	//A_0 = 3.141593	0,0
	//A_1 = 2.094395	1,-1 1,0 1,1
	//A_2 = 0.785398	2,-2 2,-1 2,0 2,1 2,2
	//A_3 = 0			3,-3 3,-2 3,-1 3,0 3,1 3,2 3,3
	//A_4 = -0.130900 
	//A_5 = 0 
	//A_6 = 0.049087 

	float A[]={		3.1415926
						,2.094395
						,0.785398
						,0		
						,-0.130900
						,0 
						,0.049087
						,0
						,-0.02454
						,0
						,0.014317154
						,0
						,-0.009203885
						,0
						,0.006327671
						,0
						};
	int n=0;
	for(int l=0;l<MAX_SH_BANDS;l++)
	{
		if (l >= numSHBands)
			break;
		float w =  WindowFunction(float(l) / float(numSHBands)); // should we  bake this into SH?
		for (int m = -l; m <= l; m++)
			result += coefficients[n++] * SH(l, m, theta, phi) *w *A[l] / 3.1415926;
	}
	return result;
}

#endif