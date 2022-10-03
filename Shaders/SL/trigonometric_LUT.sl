//  Copyright (c) 2022 Simul Software Ltd. All rights reserved.
#ifndef TRIGONOMETRIC_LUT_SL
#define TRIGONOMETRIC_LUT_SL
// Definitions shared across C++, HLSL, and GLSL!

//LUTs for sin, cos, tan
//LUTs for asin, acos, atan, atan2

#include "shader_platform.sl"
#include "common.sl"

SamplerState TrigLineSampler
{
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = POINT;
	AddressU = MIRROR;
	AddressV = CLAMP;
	AddressW = CLAMP;
};

Texture2DArray<float> trig_LUT : register(t0);

RWTexture2D<float> trig_LUT0 : register(u0);
RWTexture2D<float> trig_LUT1 : register(u1);

#define LUT_Size 128
#define LUT_Size_F 128.0
#define LUT_Size_Divisor 127.0

float GetRatioFromIdx(float idx)
{
	return (idx / LUT_Size_Divisor);
}
float GetAngleFromIdx(float idx)
{
	return SIMUL_HALF_PI_F * GetRatioFromIdx(idx);
}
float GetIdxFromRatio(float ratio)
{
	return ratio * LUT_Size_Divisor;
}
float GetIdxFromAngle(float angle)
{
	return GetIdxFromRatio(angle) / SIMUL_HALF_PI_F;
}

//Bound angle between -bound and bound.
float BoundValue(float value, float bound)
{
	while (value > bound)
		value -= bound;
	while (value < -bound)
		value += bound;

	return value;
}

//Execute 128 threads per trig function. CPU Dispatch must be (1, 133, 1).
CS_LAYOUT(LUT_Size,1,1)
shader void CS_Generate_Trigonometric_LUT(uint3 g : SV_GroupID, uint3 t : SV_GroupThreadID)
{
	uint func_idx = g.y;
	uint value_idx = t.x;
	float inputRatio = GetRatioFromIdx(value_idx); //From 0.00 -> 1.00
	float inputAngle = GetAngleFromIdx(value_idx); //From 0.00 -> 1.57
	
	//Angle to Ratio
	if (func_idx == LUT_Size + 0)
	{
		trig_LUT0[uint2(value_idx, func_idx - LUT_Size)] = sin(inputAngle);
	}
	else if (func_idx == LUT_Size + 1)
	{
		trig_LUT0[uint2(value_idx, func_idx - LUT_Size)] = cos(inputAngle);
	}
	else if (func_idx == LUT_Size + 2)
	{
		trig_LUT0[uint2(value_idx, func_idx - LUT_Size)] = tan(inputAngle);
	}
	
	//Ratio to Angle
	else if (func_idx == LUT_Size + 3)
	{
		trig_LUT0[uint2(value_idx, func_idx - LUT_Size)] = asin(inputRatio);
	}
	else if (func_idx == LUT_Size + 4)
	{
		trig_LUT0[uint2(value_idx, func_idx - LUT_Size)] = acos(inputRatio);
	}
	else
	{
		float inputRatioY = GetRatioFromIdx(func_idx);
		trig_LUT1[uint2(value_idx, func_idx)] = atan2(inputRatioY, inputRatio);
	}
}

float sin_LUT(float value)
{
	float _sign = sign(value);
	float boundedValue = BoundValue(value, SIMUL_PI_F);

	float x = GetIdxFromAngle(boundedValue);
	float y = (0.0 + 0.5) / LUT_Size_F;
	float layer = 0.0;
	return _sign * trig_LUT.Sample(TrigLineSampler, float3(x, y, layer));
}

float cos_LUT(float value)
{
	float _sign = -1.0 * sign(value + SIMUL_HALF_PI_F) * sign(value - SIMUL_HALF_PI_F);
	float boundedValue = BoundValue(value, SIMUL_PI_F);

	float x = GetIdxFromAngle(boundedValue);
	float y = (1.0 + 0.5) / LUT_Size_F;
	float layer = 0.0;
	return _sign * trig_LUT.Sample(TrigLineSampler, float3(x, y, layer));
}

#endif