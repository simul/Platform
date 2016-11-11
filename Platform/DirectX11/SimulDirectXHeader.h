#pragma once

#ifdef _XBOX_ONE
#include <d3d11_x.h>
#define SIMUL_WIN8_SDK
#define MONOLITHIC 1
#else
#ifdef SIMUL_WIN8_SDK
#include <D3D11_1.h>
#else
#include <D3D11.h>
#endif
#endif