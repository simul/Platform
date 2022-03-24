#pragma once
#include <d3d12.h>
#include <d3dx12.h>
typedef ID3D12GraphicsCommandList ID3D12GraphicsCommandListType;
typedef ID3D12Device5 ID3D12DeviceType;
#include "dxcapi.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"D3dcompiler.lib")
#pragma comment(lib,"DXGI.lib")
#define PLATFORM_NAME "DirectX 12"
#define SFX_CONFIG_FILENAME "Sfx/DirectX12.json"
#if SIMUL_INTERNAL_CHECKS
#define PLATFORM_D3D12_RELEASE_MANAGER_CHECKS 0
#define SIMUL_DEBUG_BARRIERS 0
#else
#define PLATFORM_D3D12_RELEASE_MANAGER_CHECKS 0
#define SIMUL_DEBUG_BARRIERS 0
#endif