//  Copyright (c) 2018 Simul Software Ltd. All rights reserved.

#include "../SL/water_constants.sl"
#include "../SL/common.sl"
#include "../SL/render_states.sl"
#include "../SL/states.sl"
#include "../SL/simul_inscatter_fns.sl"
#include "../SL/debug_constants.sl"
#include "../SL/depth.sl"
#include "../SL/noise.sl"

#ifndef WATER_SL
#define WATER_SL
#ifndef PI
#define PI 3.1415926536f
#endif

#ifndef TAU
#define TAU 6.28318530718f
#endif

#define COS_PI_4_16 0.70710678118654752440084436210485f
#define TWIDDLE_1_8 COS_PI_4_16, -COS_PI_4_16
#define TWIDDLE_3_8 -COS_PI_4_16, -COS_PI_4_16

#define COHERENCY_GRANULARITY 128

#define RAND_MAX 0x7fff

#define PATCH_BLEND_BEGIN		200
#define PATCH_BLEND_END			10000

#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

#ifndef MAX_FADE_DISTANCE_METRES
#define MAX_FADE_DISTANCE_METRES (300000.0)
#endif

//Random offset values for profile buffers to remove tiling and aliasing
float profileOffsets[16] =
{
	0.54f,
	0.18f,
	0.61f,
	0.28f,
	0.09f,
	0.71f,
	0.99f,
	0.43f,
	0.11f,
	0.85f,
	0.24f,
	0.67f,
	0.31f,
	0.86f,
	0.13f,
	0.01f
};


vec2 profile16Directons[16] = 
{
	vec2(1.0f,   0.0f),
	vec2(0.92f,  0.38f),
	vec2(0.707f, 0.707f),
	vec2(0.38f,  0.92f),

	vec2(0.0f,   1.0f),
	vec2(-0.38f, 0.92f),
	vec2(-0.707f,0.707f),
	vec2(-0.92f, 0.38f),

	vec2(-1.0f,  0.0f),
	vec2(-0.92f,-0.38f),
	vec2(-0.707f,-0.707f),
	vec2(-0.38f,-0.92f),

	vec2(0.0f,  -1.0f),
	vec2(0.38f, -0.92f),
	vec2(0.707f,-0.707f),
	vec2(0.92f, -0.38f)
};

vec2 profile32Directons[32] =
{
	vec2(1.0f,    0.0f),
	vec2(0.98f,   0.19f),
	vec2(0.92f,   0.38f),
	vec2(0.83f,   0.55f),
	vec2(0.707f,  0.707f),
	vec2(0.55f,   0.83f),
	vec2(0.38f,   0.92f),
	vec2(0.19f,   0.98f),

	vec2(0.0f,    1.0f),
	vec2(-0.19f,  0.98f),
	vec2(-0.38f,  0.92f),
	vec2(-0.55f,  0.83f),
	vec2(-0.707f, 0.707f),
	vec2(-0.83f,  0.55f),
	vec2(-0.92f,  0.38f),
	vec2(-0.98f,  0.19f),

	vec2(-1.0f,  -0.0f),
	vec2(-0.98f, -0.19f),
	vec2(-0.92f, -0.38f),
	vec2(-0.83f, -0.55f),
	vec2(-0.707f,-0.707f),
	vec2(-0.55f, -0.83f),
	vec2(-0.38f, -0.92f),
	vec2(-0.19f, -0.98f),

	vec2(0.0f,   -1.0f),
	vec2(0.19f,  -0.98f),
	vec2(0.38f,  -0.92f),
	vec2(0.55f,  -0.83f),
	vec2(0.707f, -0.707f),
	vec2(0.83f,  -0.55f),
	vec2(0.92f,  -0.38f),
	vec2(0.98f,  -0.19f),
};

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

vec3 convertPosToTexc(vec3 pos, uint3 dims)
{
	return (vec3(pos) + vec3(0.5, 0.5, 0.5)) / vec3(dims);
}

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

// Generating gaussian random number with mean 0 and standard deviation 1.
float Gauss(vec2 values)
{
	float u1 = rand(values.xy);
	float u2 = rand(values.yx * 0.5);
	if (u1 < 0.0000001f)
		u1 = 0.0000001f;
	return sqrt(-2.f* log(u1)) * cos(2.f*3.1415926536f * u2);
}

// Phillips Spectrum
// K: normalized wave vector, W: wind direction, v: wind velocity, a: amplitude constant
float Phillips2(vec2 K, vec2 W, float v, float a, float dir_depend)
{
	static float g = 981.f;
	// largest possible wave from constant wind of velocity v
	float l = v * v / g;
	// damp out waves with very small length w << l
	float w = 0;// l / 10000.f;

	float Ksqr = K.x * K.x + K.y * K.y;
	float Kcos = K.x * W.x + K.y * W.y;
	float phillips = a * exp(-1 / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips *exp(-Ksqr * w * w);
}

float spectrum(float zeta, float windSpeed) {
	float A = pow(1.1, 1.5 * zeta); // original pow(2, 1.5*zeta)
	float B = exp(-1.8038897788076411 * pow(4.f, zeta) / pow(windSpeed, 4.f));
	return 0.139098f * sqrt(A * B);
}

vec4 gerstner_wave(float phase /*=knum*x*/, float knum) {
	float s = sin(phase);
	float c = cos(phase);

	return vec4(-s, c, -knum * c, -knum * s);
}

float cubic_bump(float x) {
	if (abs(x) >= 1)
		return 0.0f;
	else
		return x * x * (2 * abs(x) - 3) + 1;
}

//Calculate fresnel term
float fresnel(vec3 incident, vec3 normal, float sourceIndex, float mediumIndex)
{
	float output;
	//Schlick's apporixmation, 
	float cos_incident = clamp(-1.0, 1.0, dot(incident, normal));
	float R0 = pow(((sourceIndex - mediumIndex) / (sourceIndex + mediumIndex)), 2.0);
	return R0 + (1 - R0) * pow(1 - cos_incident, 5.0);

	/* Old more accurate but more expensive method
	float sint = (sourceIndex / mediumIndex) * sqrt(max(0.0, 1 - (cos_incident * cos_incident)));
	if (sint >= 1.0)
	{  
		output = 0.0;
	}
	else
	{
		float cost = sqrt(max(0.0, 1 - (sint * sint)));
		cos_incident = abs(cos_incident);
		float Rs = ((mediumIndex * cos_incident) - (sourceIndex * cost)) / ((mediumIndex * cos_incident) + (sourceIndex * cost));
		float Rp = ((sourceIndex * cos_incident) - (mediumIndex * cost)) / ((sourceIndex * cos_incident) + (mediumIndex * cost));
		output = ((Rs * Rs + Rp * Rp) / 2.0);
	}

	//float R = pow((sourceIndex - mediumIndex) / (sourceIndex + mediumIndex), 2);
	//output = R + (1 - R) * pow(1 - dot(incident, normal), 5);

	return output;
	*/
}

#endif