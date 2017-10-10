#pragma once

//! Improves perfomance of CreateConstantBufferViews and CopyDescriptors
//! by removing a run time "if". This should only be on release builds...
#define D3DCOMPILE_NO_DEBUG 1 

#include <DirectXMath.h>

#if defined(_XBOX_ONE)
	#include <d3d12_x.h>		//! core 12.x header
	#include <d3dx12_x.h>		//! utility 12.x header
	#include <D3Dcompiler_x.h>
	#define MONOLITHIC 1
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 1 
	#define SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK 1 
	#define SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE 0x40 
#else
	#include <D3Dcompiler.h>
	#include <dxgi.h>
	#include <dxgi1_4.h>
	#include <d3d12.h>
	#include "d3dx12.h"
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 0 
#endif

#if defined(_XBOX_ONE)
	#pragma comment(lib,"d3dcompiler")
	#pragma comment(lib,"d3d12_x")
	#pragma comment(lib,"DirectXTex")
#else
	#pragma comment(lib,"D3D12")
	#pragma comment(lib,"D3dcompiler.lib")
	#pragma comment(lib,"DirectXTex")
	#pragma comment(lib,"dxgi.lib")
#endif


