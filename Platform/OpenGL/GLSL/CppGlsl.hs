#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"

#ifndef __cplusplus
constant_buffer RescaleVertexShaderConstants : register(b0)
{
	uniform float rescaleVertexShaderY;
};
#define fmod mod
#else
struct RescaleVertexShaderConstants
{
	static const int bindingIndex=0;
	uniform float rescaleVertexShaderY;
};
#endif
#endif