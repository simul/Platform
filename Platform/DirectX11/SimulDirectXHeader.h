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

#define SIMUL_D3D11_MAP_FLAGS (((dx11::RenderPlatform*)deviceContext.renderPlatform)->GetMapFlags())	

#ifdef _XBOX_ONE
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 1 
	#define SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK 1 
	#define SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE 0x40 
#else
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 0 
#endif

#ifndef _XBOX_ONE
	struct ID3DUserDefinedAnnotation;
#endif
