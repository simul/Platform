#pragma once

#ifdef _XBOX_ONE
#include <d3d12_x.h>
#define SIMUL_WIN8_SDK
#define MONOLITHIC 1
#else
#include <d3d12.h>
#endif