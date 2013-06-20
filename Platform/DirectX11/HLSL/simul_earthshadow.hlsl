#ifndef SIMUL_EARTHSHADOW
#define SIMUL_EARTHSHADOW
#include "../../CrossPlatform/earth_shadow_uniforms.sl"

#ifndef __cplusplus
SamplerState inscSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
#include "../../CrossPlatform/earth_shadow.sl"
#endif
#endif