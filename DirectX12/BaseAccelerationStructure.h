#pragma once

#include "Platform/CrossPlatform/BaseAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

#if defined(_GAMING_XBOX_XBOXONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

namespace platform
{
	namespace dx12
	{
	#if PLATFORM_SUPPORT_D3D12_RAYTRACING
		//You must call Release() on both the ID3D12Device5 and ID3D12GraphicsCommandList4, as there are created via QueryInterface().
		bool GetID3D12Device5andID3D12GraphicsCommandList4(crossplatform::DeviceContext& deviceContext, ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& commandList4);
	#endif
	}
}