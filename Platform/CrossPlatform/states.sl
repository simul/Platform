#ifndef STATES_SL
#define STATES_SL

SamplerState noiseSamplerState : register(s2)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState wrapMirrorSamplerState:register(s3)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
};

SamplerState cmcSamplerState:register(s5)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState wrapSamplerState:register(s6)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState wwcSamplerState:register(s7)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState cwcSamplerState: register(s8)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState clampSamplerState: register(s9)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressV = Clamp;
};

SamplerState wrapClampSamplerState: register(s10)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Clamp;
};

SamplerState samplerStateNearest: register(s11)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState cwcNearestSamplerState: register(s12)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Wrap;
	AddressW = Clamp;
};
#endif