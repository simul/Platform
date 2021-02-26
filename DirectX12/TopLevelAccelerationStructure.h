#pragma once

#include "Platform/CrossPlatform/TopLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

namespace simul
{
	namespace dx12
	{
		class TopLevelAccelerationStructure final : public crossplatform::TopLevelAccelerationStructure
		{
		public:
			TopLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			~TopLevelAccelerationStructure();
			void RestoreDeviceObjects() override;
			void InvalidateDeviceObjects() override;
			ID3D12Resource* AsD3D12ShaderResource(crossplatform::DeviceContext& deviceContext);
			void BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext) override;

		protected:
			std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;

			ID3D12Resource* accelerationStructure = nullptr;
			ID3D12Resource* scratchResource = nullptr;
			ID3D12Resource* instanceDescsResource = nullptr;

		
		};
	}
}