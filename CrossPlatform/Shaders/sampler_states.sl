//  Copyright (c) 2015-2024 Simul Software Ltd. All rights reserved.
#ifndef STATES_SL
#define STATES_SL

#ifdef __cplusplus
const std::string sampler_states_sl_file = R"(
#endif

SamplerState cmmSamplerState : register(s0), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Mirror;
};

SamplerState wccSamplerState : register(s1), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wmcSamplerState : register(s2), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState wrapMirrorSamplerState : register(s3), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
	AddressW = Wrap;
};

SamplerState cubeSamplerState : register(s4), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
	AddressV = Mirror;
	AddressW = Mirror;
};

SamplerState cmcSamplerState : register(s5), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState wrapSamplerState : register(s6), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState wwcSamplerState : register(s7), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState cwcSamplerState : register(s8), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState clampSamplerState : register(s9), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wrapClampSamplerState : register(s10), resource_group(g0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState samplerStateNearest : register(s11), resource_group(g0)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wwcNearestSamplerState : register(s12), resource_group(g0)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState cmcNearestSamplerState : register(s13), resource_group(g0)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState samplerStateNearestWrap : register(s14), resource_group(g0)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState samplerStateNearestClamp : register(s15), resource_group(g0)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

#ifdef __cplusplus
)";
#endif

#endif