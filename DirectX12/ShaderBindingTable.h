#pragma once
#include "Platform/CrossPlatform/ShaderBindingTable.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

namespace simul
{
	namespace dx12
	{
		class ShaderBindingTable final : public crossplatform::ShaderBindingTable
		{
		public:
			std::map<crossplatform::ShaderRecord::Type, ID3D12Resource*> SBTResources;

		public:
			ShaderBindingTable();
			~ShaderBindingTable();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r) override;
			void InvalidateDeviceObjects() override;

			std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> GetShaderHandlesFromEffectPass(crossplatform::RenderPlatform* renderPlatform, crossplatform::EffectPass* pass) override;
			void BuildShaderBindingTableResources(crossplatform::RenderPlatform* renderPlatform) override;

			inline const std::map<crossplatform::ShaderRecord::Type, size_t>& GetShaderBindingTableStrides() const { return shaderBindingTableStrides; }
			inline std::map<crossplatform::ShaderRecord::Type, size_t>& GetShaderBindingTableStrides() { return shaderBindingTableStrides; }
		};
	}
}

