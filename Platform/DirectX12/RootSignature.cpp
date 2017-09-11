#include "RootSignature.h"
#include "Effect.h"
#include "RenderPlatform.h"
#include "../CrossPlatform/DeviceContext.h"

#include "D3Dcompiler.h"
#include <algorithm>

namespace simul
{
	namespace dx12
	{
		RootSignature::RootSignature():
			mApiRootSignature(nullptr)
		{
		}

		RootSignature::~RootSignature()
		{
		}

		ID3D12RootSignature* RootSignature::Get() const
		{
			return mApiRootSignature;
		}

		void RootSignature::Init(dx11on12::EffectPass* pass, crossplatform::DeviceContext* context)
		{
			auto c = (dx11on12::Shader*)pass->shaders[crossplatform::SHADERTYPE_COMPUTE];
			auto p = (dx11on12::Shader*)pass->shaders[crossplatform::SHADERTYPE_PIXEL];
			auto v = (dx11on12::Shader*)pass->shaders[crossplatform::SHADERTYPE_VERTEX];

			std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>> constantBuffers;
			std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>> srViews;
			std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>> uaViews;

			HRESULT res					= S_FALSE;
			bool mIsCompute				= !(v && p);
			unsigned int numShaderDescs = mIsCompute ? 1 : 2;
			D3D12_SHADER_DESC sDesc[2]	= {}; // compute (compute 0) , std (vertex 0,pixel 1)

			// Get the reflection code
			if (mIsCompute)
			{
				res = D3DReflect(c->computeShader12->GetBufferPointer(), c->computeShader12->GetBufferSize(), IID_PPV_ARGS(&c->mShaderReflection));
				res = c->mShaderReflection->GetDesc(&sDesc[0]);
				SIMUL_ASSERT(res == S_OK);
				LoadResources(c, sDesc[0], constantBuffers, srViews, uaViews);
			}
			else
			{
				res = D3DReflect(v->vertexShader12->GetBufferPointer(), v->vertexShader12->GetBufferSize(), IID_PPV_ARGS(&v->mShaderReflection));
				res = v->mShaderReflection->GetDesc(&sDesc[0]);
				SIMUL_ASSERT(res == S_OK);
				LoadResources(v, sDesc[0], constantBuffers, srViews, uaViews);

				res = D3DReflect(p->pixelShader12->GetBufferPointer(), p->pixelShader12->GetBufferSize(), IID_PPV_ARGS(&p->mShaderReflection));
				res = p->mShaderReflection->GetDesc(&sDesc[1]);
				SIMUL_ASSERT(res == S_OK);
				LoadResources(p, sDesc[1], constantBuffers, srViews, uaViews);
			}

			// SAMPLERS
			std::sort(mSamplers.begin(), mSamplers.end());

			// CBV_SRV_UAV
			{
				// First sort the resources (this step may be not needed as the reflection code already sorts it)
				std::sort(constantBuffers.begin(), constantBuffers.end());
				std::sort(srViews.begin(), srViews.end());
				std::sort(uaViews.begin(), uaViews.end());

				// Insert the sorted vectors into the global resource list
				mResources.insert(mResources.end(), constantBuffers.begin(), constantBuffers.end());
				mResources.insert(mResources.end(), srViews.begin(), srViews.end());
				mResources.insert(mResources.end(), uaViews.begin(), uaViews.end());
			}

			// Sampler table
			if (!mSamplers.empty())
			{
				for (unsigned int i = 0; i < mSamplers.size(); i++)
				{
					mSamplerRanges.push_back(CD3DX12_DESCRIPTOR_RANGE
					(
						mSamplers[i].second,
						1,	
						mSamplers[i].first
					));
				}
				mSampIdx = mRootParams.size();
				mRootParams.push_back(CD3DX12_ROOT_PARAMETER());
				mRootParams[mSampIdx].InitAsDescriptorTable(mSamplerRanges.size(), &mSamplerRanges[0]);
			}
			// Res table
			if (!mResources.empty())
			{
				for (unsigned int i = 0; i < mResources.size(); i++)
				{
					mSrvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE
					(
						mResources[i].second,
						1,
						mResources[i].first
					));
				}
				mResIdx = mRootParams.size();
				mRootParams.push_back(CD3DX12_ROOT_PARAMETER());
				mRootParams[mResIdx].InitAsDescriptorTable(mSrvCbvUavRanges.size(), &mSrvCbvUavRanges[0]);
			}

			CreateAPIRootSignature(context);
		}

		void RootSignature::CreateAPIRootSignature(crossplatform::DeviceContext* context)
		{
			// Build the root signature
			HRESULT res = S_FALSE;
			CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
			rootDesc.Init
			(
				mRootParams.size(),
				&mRootParams[0],
				0,
				nullptr,
				mIsCompute ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			);

			ID3DBlob* error;
			ID3DBlob* rootSerialized;
			res = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSerialized, &error);
			if (res != S_OK)
			{
				SIMUL_CERR << "Root signature serialization failed:" << std::endl;
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			SIMUL_ASSERT(res == S_OK);
			res = context->renderPlatform->AsD3D12Device()->CreateRootSignature(0, rootSerialized->GetBufferPointer(), rootSerialized->GetBufferSize(), IID_PPV_ARGS(&mApiRootSignature));

			// Assign a name
			std::wstring name = L"RootSignature_";
			std::string techName = context->renderPlatform->GetContextState(*context)->currentTechnique->name;
			name += std::wstring(techName.begin(), techName.end());
			mApiRootSignature->SetName(name.c_str());
		}

		void RootSignature::LoadResources(dx11on12::Shader* shader, D3D12_SHADER_DESC& desc,std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& cb, std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& sr, std::vector<std::pair<UINT, D3D12_DESCRIPTOR_RANGE_TYPE>>& ua)
		{
			HRESULT res = S_FALSE;
			for (unsigned int j = 0; j < desc.BoundResources; j++)
			{
				D3D12_SHADER_INPUT_BIND_DESC resDesc = {};
				res = shader->mShaderReflection->GetResourceBindingDesc(j, &resDesc);
				SIMUL_ASSERT(res == S_OK);

				// SRV textures and structured buffers
				if (resDesc.Type == D3D_SIT_TEXTURE || resDesc.Type == D3D_SIT_STRUCTURED || resDesc.Type == D3D_SIT_TBUFFER || resDesc.Type == D3D_SIT_BYTEADDRESS)
				{
					bool dup = false;
					for (auto& ele : sr)
						if (ele.first == resDesc.BindPoint)
							dup = true;
					if(!dup)
						sr.push_back(std::make_pair(resDesc.BindPoint, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
				}
				// Constant Buffers
				else if (resDesc.Type == D3D_SIT_CBUFFER)
				{
					bool dup = false;
					for (auto& ele : cb)
						if (ele.first == resDesc.BindPoint)
							dup = true;
					if (!dup)
						cb.push_back(std::make_pair(resDesc.BindPoint, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
				}
				// Samplers
				else if (resDesc.Type == D3D_SIT_SAMPLER)
				{
					bool dup = false;
					for (auto& ele : mSamplers)
						if (ele.first == resDesc.BindPoint)
							dup = true;
						if (!dup)
					mSamplers.push_back(std::make_pair(resDesc.BindPoint, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER));
				}
				// UAV
				else
				{
					bool dup = false;
					for (auto& ele : ua)
						if (ele.first == resDesc.BindPoint)
							dup = true;
					if (!dup)
						ua.push_back(std::make_pair(resDesc.BindPoint, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));
				}
			}
		}
	}
}

