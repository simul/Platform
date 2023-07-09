//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef STATES_SL
#define STATES_SL

SamplerState cmmSamplerState SIMUL_STATE_REGISTER(0)
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Mirror;
    AddressW = Mirror;
};


SamplerState wccSamplerState SIMUL_STATE_REGISTER(1)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wmcSamplerState SIMUL_STATE_REGISTER(2)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
	AddressW = Clamp;
};
SamplerState wrapMirrorSamplerState SIMUL_STATE_REGISTER(3)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
	AddressW = Wrap;
};

SamplerState cmcSamplerState SIMUL_STATE_REGISTER(5)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState wrapSamplerState SIMUL_STATE_REGISTER(6)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState wwcSamplerState SIMUL_STATE_REGISTER(7)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState cwcSamplerState SIMUL_STATE_REGISTER(8)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState clampSamplerState SIMUL_STATE_REGISTER(9)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wrapClampSamplerState SIMUL_STATE_REGISTER(10)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState samplerStateNearest SIMUL_STATE_REGISTER(11)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState wwcNearestSamplerState SIMUL_STATE_REGISTER(12)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

SamplerState cmcNearestSamplerState SIMUL_STATE_REGISTER(13)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

SamplerState samplerStateNearestWrap SIMUL_STATE_REGISTER(14)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

SamplerState samplerStateNearestClamp SIMUL_STATE_REGISTER(15)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState cubeSamplerState SIMUL_STATE_REGISTER(4)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
	AddressV = Mirror;
	AddressW = Mirror;
};

#endif