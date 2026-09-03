#pragma once
#include "Platform/CrossPlatform/TopLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX12/Heap.h"

#if defined(_GAMING_XBOX_XBOXONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

namespace platform
{
	namespace dx12
	{
		class TopLevelAccelerationStructure final : public crossplatform::TopLevelAccelerationStructure
		{
		public:
			TopLevelAccelerationStructure(crossplatform::RenderPlatform* r, const std::string& name);
			~TopLevelAccelerationStructure();
			void RestoreDeviceObjects() override;
			void InvalidateDeviceObjects() override;
			void BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext) override;
			
			ID3D12Resource* AsD3D12ShaderResource(crossplatform::DeviceContext& deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext);

		protected:
		#if PLATFORM_SUPPORT_D3D12_RAYTRACING
			std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;
		#endif	

			ID3D12Resource* accelerationStructure = nullptr;
			ID3D12Resource* scratchResource = nullptr;
			ID3D12Resource* instanceDescsResource = nullptr;

			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
			D3D12_CPU_DESCRIPTOR_HANDLE shaderResourceView = { 0 };
			Heap descriptorHeap;
		};
	}
}