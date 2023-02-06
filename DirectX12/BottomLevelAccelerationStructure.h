#pragma once

#include "Platform/CrossPlatform/BottomLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

#if defined(_GAMING_XBOX_XBOXONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

namespace platform
{
	namespace dx12
	{
		class BottomLevelAccelerationStructure final : public crossplatform::BottomLevelAccelerationStructure
		{
		public:
			BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			~BottomLevelAccelerationStructure();
			void RestoreDeviceObjects() override;
			void InvalidateDeviceObjects() override;
			ID3D12Resource* AsD3D12ShaderResource(crossplatform::DeviceContext& deviceContext);
			void BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext) override;

		protected:
		#if PLATFORM_SUPPORT_D3D12_RAYTRACING
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;
		#endif		

			ID3D12Resource* accelerationStructure = nullptr;
			ID3D12Resource* scratchResource = nullptr;
			ID3D12Resource* transforms = nullptr;
		};
	}
}