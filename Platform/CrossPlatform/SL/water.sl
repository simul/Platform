//  Copyright (c) 2018 Simul Software Ltd. All rights reserved.

#include "../SL/water_constants.sl"

#ifndef WATER_SL
#define WATER_SL
#ifndef PI
#define PI 3.1415926536f
#endif

#define COS_PI_4_16 0.70710678118654752440084436210485f
#define TWIDDLE_1_8 COS_PI_4_16, -COS_PI_4_16
#define TWIDDLE_3_8 -COS_PI_4_16, -COS_PI_4_16

#define COHERENCY_GRANULARITY 128

#define RAND_MAX 0x7fff

struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};

struct VS_OUTPUT
{
	vec4 Position : SV_POSITION;
	vec2 texCoords : TEXCOORD0;
	vec3 LocalPos : TEXCOORD1;
	vec4 vecColour : TEXCOORD2;
	vec4 SurePosition : TEXCOORD3;
};

struct PS_WATER_OUTPUTS
{
	vec4 Depth			SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 Scattering		SIMUL_RENDERTARGET_OUTPUT(1);
	vec4 Absorption		SIMUL_RENDERTARGET_OUTPUT(2);
	vec4 Normals		SIMUL_RENDERTARGET_OUTPUT(3);
	vec4 RefractColour	SIMUL_RENDERTARGET_OUTPUT(4);
};

void FT2(inout float2 a, inout float2 b)
{
	float t;

	t = a.x;
	a.x += b.x;
	b.x = t - b.x;

	t = a.y;
	a.y += b.y;
	b.y = t - b.y;
}

void CMUL_forward(inout float2 a, float bx, float by)
{
	float t = a.x;
	a.x = t * bx - a.y * by;
	a.y = t * by + a.y * bx;
}

void UPD_forward(inout float2 a, inout float2 b)
{
	float A = a.x;
	float B = b.y;

	a.x += b.y;
	b.y = a.y + b.x;
	a.y -= b.x;
	b.x = A - B;
}


void FFT_forward_4(inout float2 D[8])
{
	FT2(D[0], D[2]);
	FT2(D[1], D[3]);
	FT2(D[0], D[1]);

	UPD_forward(D[2], D[3]);
}

void FFT_forward_8(inout float2 D[8])
{
	FT2(D[0], D[4]);
	FT2(D[1], D[5]);
	FT2(D[2], D[6]);
	FT2(D[3], D[7]);

	UPD_forward(D[4], D[6]);
	UPD_forward(D[5], D[7]);

	CMUL_forward(D[5], TWIDDLE_1_8);
	CMUL_forward(D[7], TWIDDLE_3_8);

	FFT_forward_4(D);
	FT2(D[4], D[5]);
	FT2(D[6], D[7]);
}

void TWIDDLE(inout float2 d, float phase)
{
	float tx, ty;

	sincos(phase, ty, tx);
	float t = d.x;
	d.x = t * tx - d.y * ty;
	d.y = t * ty + d.y * tx;
}

void TWIDDLE_8(inout float2 D[8], float phase)
{
	TWIDDLE(D[4], 1 * phase);
	TWIDDLE(D[2], 2 * phase);
	TWIDDLE(D[6], 3 * phase);
	TWIDDLE(D[1], 4 * phase);
	TWIDDLE(D[5], 5 * phase);
	TWIDDLE(D[3], 6 * phase);
	TWIDDLE(D[7], 7 * phase);
}

//Calculate fresnel term
float fresnel(vec3 incident, vec3 normal, float sourceIndex, float mediumIndex)
{
	float fresnel;
	float cos_incident = clamp(-1.0, 1.0, dot(incident, normal));

	float sint = (sourceIndex / mediumIndex) * sqrt(max(0.0, 1 - (cos_incident * cos_incident)));
	if (sint >= 1.0)
	{
		fresnel = 0.0;
	}
	else
	{
		float cost = sqrt(max(0.0, 1 - sint * sint));
		cos_incident = abs(cos_incident);
		float Rs = ((mediumIndex * cos_incident) - (sourceIndex * cost)) / ((mediumIndex * cos_incident) + (sourceIndex * cost));
		float Rp = ((sourceIndex * cos_incident) - (mediumIndex * cost)) / ((sourceIndex * cos_incident) + (mediumIndex * cost));
		fresnel = ((Rs * Rs + Rp * Rp) / 2.0);
	}

	return fresnel;

}

#endif