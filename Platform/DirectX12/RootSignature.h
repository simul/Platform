/*

*/
#pragma once

#include "Simul/Platform/DirectX12/Export.h"
#include "SimulDirectXHeader.h"
#include "d3d12shader.h"

#include <vector>


namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;

	}
	namespace dx11on12
	{
		class Shader;
		class EffectPass;
	}
	namespace dx12
	{
		class SIMUL_DIRECTX12_EXPORT RootSignature
		{
		public:
			RootSignature();
			~RootSignature();

			ID3D12RootSignature* Get()const;
			void Init(dx11on12::EffectPass* pass,crossplatform::DeviceContext* context);

			int ResIdx()const { return mResIdx; }
			int SamIdx()const { return mSampIdx; }

		private:
			void CreateAPIRootSignature(crossplatform::DeviceContext* context);
			void LoadResources(dx11on12::Shader* shader, D3D12_SHADER_DESC& desc,std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& cb, std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& sr, std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& ua);

			bool					mIsCompute = false;
			ID3D12RootSignature*	mApiRootSignature;

			std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE> > mResources;
			std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE> > mSamplers;

			/// Root params
			std::vector<CD3DX12_ROOT_PARAMETER> mRootParams;

			/// CBV_SRV_UAV descriptors
			UINT mResIdx = -1;
			std::vector<CD3DX12_DESCRIPTOR_RANGE> mSrvCbvUavRanges;

			/// SAMPLER descriptors
			UINT mSampIdx = -1;
			std::vector<CD3DX12_DESCRIPTOR_RANGE> mSamplerRanges;
		};
	}
}