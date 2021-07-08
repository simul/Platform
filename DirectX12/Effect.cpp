#include "Effect.h"
#include "Texture.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/PlatformStructuredBuffer.h"
#include "Platform/DirectX12/BaseAccelerationStructure.h"
#include "Platform/DirectX12/TopLevelAccelerationStructure.h"
#include "Platform/DirectX12/BottomLevelAccelerationStructure.h"
#include "Platform/DirectX12/ShaderBindingTable.h"
#include "DirectXRaytracingHelper.h"
#include "SimulDirectXHeader.h"
#include "ThisPlatform/Direct3D12.h"
#include "Platform/DirectX12/d3dx12.h"

#include <algorithm>
#include <string>

using namespace simul;
using namespace dx12;

///////////
//Globals//
///////////

inline bool IsPowerOfTwo(UINT64 n)
{
	return ((n & (n - 1)) == 0 && (n) != 0);
}

inline UINT64 NextMultiple(UINT64 value, UINT64 multiple)
{
	SIMUL_ASSERT(IsPowerOfTwo(multiple));

	return (value + multiple - 1) & ~(multiple - 1);
}

template<class T>
UINT64 BytePtrToUint64(_In_ T* ptr)
{
	return static_cast<UINT64>(reinterpret_cast<BYTE*>(ptr) - static_cast<BYTE*>(nullptr));
}

///////////////
//RenderState//
///////////////

RenderState::RenderState()
{
	BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	DepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RasterDesc.FrontCounterClockwise = true;
}

RenderState::~RenderState()
{
}

///////////////
//Effect Pass//
///////////////

EffectPass::EffectPass(crossplatform::RenderPlatform* r, crossplatform::Effect* e) 
	:crossplatform::EffectPass(r, e)
	, mInUseOverrideDepthState(nullptr)
	, mInUseOverrideBlendState(nullptr)
{
}

EffectPass::~EffectPass()
{
	InvalidateDeviceObjects();
}

void EffectPass::InvalidateDeviceObjects()
{
	// TO-DO:  memory leaks here??
	auto pl = (dx12::RenderPlatform*)renderPlatform;
	pl->PushToReleaseManager(mComputePso, "Compute PSO");
	mComputePso = nullptr;
	for (auto& ele : mGraphicsPsoMap)
	{
		pl->PushToReleaseManager(ele.second.pipelineState, "Graphics PSO");
	}
	mGraphicsPsoMap.clear();

	for (auto& value : mTargetsMap)
	{
		SAFE_DELETE(value.second);
	}
	mTargetsMap.clear();

	SAFE_DELETE(shaderBindingTable);
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute)
{
	dx12::Shader* c = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	dx12::Shader* r = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_RAY_GENERATION];
	auto cmdList = deviceContext.asD3D12Context();
	// We will set the DescriptorHeaps and RootDescriptorTablesArguments from ApplyContextState (at RenderPlatform)
	// The create methods will only create the pso once (or if there is a change in the state)
	if (c)
	{
		CreateComputePso(deviceContext);
		if (mComputePso)
			cmdList->SetPipelineState(mComputePso);
	}
	else if (r)
	{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
		ID3D12GraphicsCommandList4* rtc = (ID3D12GraphicsCommandList4*)cmdList;
		rtc->SetPipelineState1(mRaytracePso);
#endif
		mIsRaytrace = true;
	}
	else
	{
		cmdList->SetPipelineState(GetGraphicsPso(*deviceContext.AsGraphicsDeviceContext()));
	}
}

void EffectPass::SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers, dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits = rPlat->GetResourceBindingLimits();
	int usedSlots = 0;
	auto nullSampler = rPlat->GetNullSampler();

	// The handles for the required samplers:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSamplers> srcHandles;
	for (int i = 0; i < numSamplerResourceSlots; i++)
	{
		int slot = samplerResourceSlots[i];
		crossplatform::SamplerState* samp = nullptr;
		if (deviceContext.contextState.samplerStateOverrides.size() > 0 && deviceContext.contextState.samplerStateOverrides.HasValue(slot))
		{
			samp = deviceContext.contextState.samplerStateOverrides[slot];
			// We dont override this slot, just take a default sampler state:
			if (!samp)
			{
				samp = samplers[slot];
			}
		}
		else
		{
			samp = samplers[slot];
		}
		if (!samp)
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". Sampler slot " << slot << " is invalid." << std::endl;
			srcHandles[slot] = nullSampler;
			continue;
		}
		srcHandles[slot] = *samp->AsD3D12SamplerState();
		usedSlots |= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSamplers; s++)
	{
		// All Samplers must have valid descriptors for hardware tier 1:
		if (!usesSamplerSlot(s))
		{
			// if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), nullSampler, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
		samplerHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	CheckSlots(GetSamplerSlots(), usedSlots, ResourceBindingLimits::NumSamplers, "Sampler");
#endif 
}

void EffectPass::SetConstantBuffers(crossplatform::ConstantBufferAssignmentMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits = rPlat->GetResourceBindingLimits();
	int usedSlots = 0;
	auto nullCbv = rPlat->GetNullCBV();

	// Clean up the handles array
	memset(mCbSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * mCbSrcHandles.size());

	// The handles for the required constant buffers:
	for (int i = 0; i < numConstantBufferResourceSlots; i++)
	{
		int slot = constantBufferResourceSlots[i];
		auto cb = cBuffers[slot];
		if (!cb || !usesConstantBufferSlot(slot) || slot != cb->GetIndex())
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". Constant buffer slot " << slot << " is invalid." << std::endl;
			mCbSrcHandles[slot] = nullCbv;
			continue;
		}
		auto d12cb = (dx12::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		d12cb->ActualApply(deviceContext, this, slot);
		mCbSrcHandles[slot] = d12cb->AsD3D12ConstantBuffer();
		usedSlots |= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumCBV; s++)
	{
		// All CB must have valid descriptors for hardware tier 2 and bellow:
		if (!usesConstantBufferSlot(s))
		{
			// if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)

			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mCbSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	CheckSlots(GetConstantBufferSlots(), usedSlots, ResourceBindingLimits::NumCBV, "Constant Buffer");
#endif
}

void EffectPass::SetSRVs(crossplatform::TextureAssignmentMap& textures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits = rPlat->GetResourceBindingLimits();
	auto nullSrv = rPlat->GetNullSRV();
	int usedSBSlots = 0;
	int usedTextureSlots = 0;

	// The handles for the required SRVs:
	memset(mSrvSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumSRV);
	memset(mSrvUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumSRV);
	bool is_pixel_shader = (deviceContext.contextState.currentEffectPass->shaders[crossplatform::SHADERTYPE_PIXEL] != nullptr);
	// Iterate over the textures:
	for (int i = 0; i < numResourceSlots; i++)
	{
		int slot = resourceSlots[i];
		auto ta = textures[slot];
		if (ta.resourceType == crossplatform::ShaderResourceType::ACCELERATION_STRUCTURE)
		{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
			ID3D12Resource* a = ((dx12::TopLevelAccelerationStructure*)ta.accelerationStructure)->AsD3D12ShaderResource(deviceContext);

			auto cmdList = deviceContext.asD3D12Context();
			ID3D12GraphicsCommandList4* rtc = (ID3D12GraphicsCommandList4*)cmdList;
			//commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
			rtc->SetComputeRootShaderResourceView(2, a->GetGPUVirtualAddress());
			if (slot < 25)
				mSrvSrcHandles[slot] = nullSrv;
			//device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mCbSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			//mSrvSrcHandles[slot]	= *ta.texture->AsD3D12ShaderResourceView(deviceContext, true, ta.resourceType, ta.index, ta.mip,is_pixel_shader);
#endif
		}
		else
		{
			// If the texture is null or invalid, set a dummy:
			// NOTE: this basically disables any slot checks as we will always
			// set something
			if (!ta.texture || !ta.texture->IsValid())
			{
				if (ta.dimensions == 3)
				{
					ta.texture = rPlat->GetDummy3D();
				}
				else
				{
					ta.texture = rPlat->GetDummy2D();
				}
			}
			mSrvSrcHandles[slot] = *ta.texture->AsD3D12ShaderResourceView(deviceContext, true, ta.resourceType, ta.index, ta.mip, is_pixel_shader);
		}
		if (slot < 25)
		{
			mSrvUsedSlotsArray[slot] = true;
		}
		usedTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numSbResourceSlots; i++)
	{
		int slot = sbResourceSlots[i];
		if (slot >= 25)
			continue;
		if (mSrvUsedSlotsArray[slot])
		{
			SIMUL_INTERNAL_CERR << "The slot: " << slot << " at pass: " << mTechName << ", has already being used by a texture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb)
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". Structured buffer slot " << slot << " is invalid." << std::endl;
			mSrvSrcHandles[slot] = nullSrv;
			continue;
		}
		mSrvSrcHandles[slot] = *sb->AsD3D12ShaderResourceView(deviceContext);
		mSrvUsedSlotsArray[slot] = true;
		usedSBSlots |= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		// All SRV must have valid descriptors for hardware tier 1:
		if (!usesTextureSlot(s) && !usesTextureSlotForSB(s))
		{
			// if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else if (mSrvSrcHandles[s].ptr)
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mSrvSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	CheckSlots(GetTextureSlots(), usedTextureSlots, ResourceBindingLimits::NumSRV, "Texture");
	CheckSlots(GetStructuredBufferSlots(), usedSBSlots, ResourceBindingLimits::NumSRV, "Structured Buffer");
#endif
}

void EffectPass::SetUAVs(crossplatform::TextureAssignmentMap& rwTextures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits = rPlat->GetResourceBindingLimits();
	auto nullUav = rPlat->GetNullUAV();
	int usedRwSBSlots = 0;
	int usedRwTextureSlots = 0;

	// The the handles for the required UAVs:
	memset(mUavSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumUAV);
	memset(mUavUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumUAV);

	// Iterate over the textures:
	for (int i = 0; i < numRwResourceSlots; i++)
	{
		int slot = rwResourceSlots[i];
		auto ta = rwTextures[slot];

		// If the texture is null or invalid, set a dummy:
		// NOTE: again, this disables any slot checks...
		if (!ta.texture || !ta.texture->IsValid())
		{
			if (ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
			else
			{
				ta.texture = rPlat->GetDummy2D();
			}
		}
		mUavSrcHandles[slot] = *ta.texture->AsD3D12UnorderedAccessView(deviceContext, ta.index, ta.mip);
		mUavUsedSlotsArray[slot] = true;
		usedRwTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numRwSbResourceSlots; i++)
	{
		int slot = rwSbResourceSlots[i];
		if (mUavUsedSlotsArray[slot])
		{
			SIMUL_INTERNAL_CERR << "The slot: " << slot << " at pass: " << mTechName << ", has already being used by a RWTexture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb)
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". RWStructured buffer slot " << slot << " is invalid." << std::endl;
			mUavSrcHandles[slot] = nullUav;
			continue;
		}
		mUavSrcHandles[slot] = *sb->AsD3D12UnorderedAccessView(deviceContext);
		mUavUsedSlotsArray[slot] = true;
		usedRwSBSlots |= (1 << slot);


		// Temp:

		renderPlatform->ResourceBarrierUAV(deviceContext, sb);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		// All UAV must have valid descriptors for hardware tier 2 and bellow:
		if (!usesRwTextureSlot(s) && !usesRwTextureSlotForSB(s))
		{
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mUavSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	CheckSlots(GetRwTextureSlots(), usedRwTextureSlots, ResourceBindingLimits::NumUAV, "RWTexture");
	CheckSlots(GetRwStructuredBufferSlots(), usedRwSBSlots, ResourceBindingLimits::NumUAV, "RWStructured Buffer");
#endif
}

void EffectPass::CheckSlots(int requiredSlots, int usedSlots, int numSlots, const char* type)
{
	if (requiredSlots != usedSlots)
	{
		unsigned int missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < numSlots; i++)
		{
			unsigned int testSlot = 1 << i;
			if (testSlot & missingSlots)
			{
				SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". " << type << " for slot:" << i << " was not set!\n";
			}
		}
	}
}

void EffectPass::CreateComputePso(crossplatform::DeviceContext& deviceContext)
{
	// Exit if we already have a PSO
	if (mComputePso)
	{
		return;
	}

	mTechName = this->name;
	auto curRenderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	dx12::Shader* c = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	if (!c)
	{
		SIMUL_INTERNAL_CERR << "The pass " << mTechName << " does not have valid shaders!!! \n";
		return;
	}

	mIsCompute = true;
	auto* device = curRenderPlat->AsD3D12Device();
	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc = {};
	cpsoDesc.CS = { c->shader12.data(), c->shader12.size() };
#ifdef _GAMING_XBOX_XBOXONE
	cpsoDesc.pRootSignature = nullptr;
#else
	cpsoDesc.pRootSignature = curRenderPlat->GetGraphicsRootSignature();
#endif
	cpsoDesc.NodeMask = 0;
	HRESULT res = device->CreateComputePipelineState(&cpsoDesc, SIMUL_PPV_ARGS(&mComputePso));
	SIMUL_ASSERT(res == S_OK);
	if (res == S_OK)
	{
		SIMUL_GPU_TRACK_MEMORY(mComputePso, 100)	// TODO: not the real size!
			std::wstring name = L"ComputePSO_";
		name += std::wstring(mTechName.begin(), mTechName.end());
		mComputePso->SetName(name.c_str());
	}
	else
	{
		__debugbreak();
		SIMUL_INTERNAL_CERR << "Failed to create compute PSO.\n";
		SIMUL_BREAK_ONCE("Failed to create compute PSO")
	}
}

ID3D12PipelineState* EffectPass::GetGraphicsPso(crossplatform::GraphicsDeviceContext& deviceContext)
{
	auto curRenderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	mTechName = name;
	dx12::Shader* v = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
	dx12::Shader* p = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
	if (!v && !p)
	{
		SIMUL_INTERNAL_CERR << "The pass " << name.c_str() << " does not have valid shaders!!! \n";
		return nullptr;
	}

	// Get the current blend state:
	D3D12_BLEND_DESC* finalBlend = &curRenderPlat->DefaultBlendState;
	if (blendState)
	{
		finalBlend = &((dx12::RenderState*)blendState)->BlendDesc;
	}
	else
	{
		if (curRenderPlat->BlendStateOverride)
		{
			finalBlend = curRenderPlat->BlendStateOverride;
		}
	}

	// Get current depth state:
	D3D12_DEPTH_STENCIL_DESC* finalDepth = &curRenderPlat->DefaultDepthState;
	if (depthStencilState)
	{
		finalDepth = &((dx12::RenderState*)depthStencilState)->DepthStencilDesc;
	}
	else
	{
		if (curRenderPlat->DepthStateOverride)
		{
			finalDepth = curRenderPlat->DepthStateOverride;
		}
	}

	// Get current raster:
	D3D12_RASTERIZER_DESC* finalRaster = &curRenderPlat->DefaultRasterState;
	if (rasterizerState)
	{
		finalRaster = &((dx12::RenderState*)rasterizerState)->RasterDesc;
	}
	else
	{
		if (curRenderPlat->RasterStateOverride)
		{
			finalRaster = curRenderPlat->RasterStateOverride;
		}
	}

	// Get current MSAA:
	DXGI_SAMPLE_DESC msaaDesc = { 1,0 };
	if (curRenderPlat->IsMSAAEnabled())
	{
		msaaDesc = curRenderPlat->GetMSAAInfo();
	}
	uint64_t msaaHash = ((uint64_t)msaaDesc.Count + (uint64_t)msaaDesc.Quality) << (uint64_t)32;

	// Get the current targets:
	const crossplatform::TargetsAndViewport* targets = &deviceContext.defaultTargetsAndViewport;
	if (!deviceContext.targetStack.empty())
	{
		targets = deviceContext.targetStack.top();
	}

	// Current render target output state:
	D3D12_RENDER_TARGET_FORMAT_DESC* finalRt = nullptr;
	uint64_t rthash = 0;
	if (renderTargetFormatState)
	{
		finalRt = &((dx12::RenderState*)renderTargetFormatState)->RtFormatDesc;
		// Check formats against currently bound targets
		// The debug layer will catch this, but its nice for us to be aware of this issue
		for (uint i = 0; i < finalRt->Count; i++)
		{
			if (finalRt->RTFormats[i] != RenderPlatform::ToDxgiFormat(targets->rtFormats[i]))
			{
				SIMUL_INTERNAL_CERR << "Current applied render target idx(" << i << ")" << " does not match the format specified by the effect (" << name.c_str() << "). \n";
			}
		}
		rthash = finalRt->GetHash();
	}
	else
	{
		// Lets check whats the state set by the render platform:
		D3D12_RENDER_TARGET_FORMAT_DESC tmpState = {};
		tmpState.Count = targets->num;
		for (int i = 0; i < targets->num; i++)
		{
			tmpState.RTFormats[i] = RenderPlatform::ToDxgiFormat(targets->rtFormats[i]);
			// We don't want to have an unknow state as this will end up randomly choosen by the DX
			// driver, probably causing gliches or crashes
			// To fix this, we could either send the ID3D12Resource or send the format from the client
			if (tmpState.RTFormats[i] == DXGI_FORMAT_UNKNOWN)
			{
				tmpState.RTFormats[i] = RenderPlatform::ToDxgiFormat(curRenderPlat->DefaultOutputFormat);
			}
			if (tmpState.RTFormats[i] == DXGI_FORMAT_R11G11B10_FLOAT)
			{
				//SIMUL_INTERNAL_CERR << "DXGI_FORMAT_R11G11B10_FLOAT\n";
				tmpState.Count = targets->num;
			}
		}
		rthash = tmpState.GetHash();

		// If it is a new config, add it to the map:
		auto tIt = mTargetsMap.find(rthash);
		if (tIt == mTargetsMap.end())
		{
			mTargetsMap[rthash] = new D3D12_RENDER_TARGET_FORMAT_DESC(tmpState);
		}

		finalRt = mTargetsMap[rthash];
	}

	// Get hash for the current config:
	// TO-DO: what about the depth format
	// This is a bad hashing method
	uint64_t hash = ((uint64_t)finalBlend) ^ ((uint64_t)finalDepth) ^ ((uint64_t)finalRaster) ^ rthash ^ msaaHash;

	// Runtime check for depth write:
	if (finalDepth->DepthWriteMask != D3D12_DEPTH_WRITE_MASK_ZERO && !targets->m_dt)
	{
		SIMUL_CERR_ONCE << "This pass(" << name.c_str() << ") expects a depth target to be bound (write), but there isn't one. \n";
	}
	// Runtime check for depth read:
	if (finalDepth->DepthEnable && !targets->m_dt)
	{
		SIMUL_CERR_ONCE << "This pass(" << name.c_str() << ") expects a depth target to be bound (read), but there isn't one. \n";
	}

	// If the map has items, and the item is in the map, just return
	if (!mGraphicsPsoMap.empty() && mGraphicsPsoMap.find(hash) != mGraphicsPsoMap.end())
	{
		Pso& pso = mGraphicsPsoMap[hash];
		if (pso.desc.RTVFormats[0] != finalRt->RTFormats[0])
		{
			SIMUL_INTERNAL_CERR << "Format mismatch in PSO\n";
		}
		return pso.pipelineState;
	}

	// Build a new pso pair <pixel format, PSO>
	Pso& pso = mGraphicsPsoMap[hash];

	mIsCompute = false;
	mIsRaytrace = false;

	// Try to get the input layout (if none, we dont need to set it to the pso)
	D3D12_INPUT_LAYOUT_DESC* pCurInputLayout = curRenderPlat->GetCurrentInputLayout();

	// Find the current primitive topology type
	D3D12_PRIMITIVE_TOPOLOGY curTopology = curRenderPlat->GetCurrentPrimitiveTopology();
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = {};
	// Invalid
	if (curTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}
	// Point
	else if (curTopology == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	}
	// Lines
	else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_LINELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_LINESTRIP)
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	}
	// Triangles
	else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
	// Patches
	else
	{
		primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}

	// Build the PSO description 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpsoDesc = pso.desc;
	gpsoDesc.pRootSignature = curRenderPlat->GetGraphicsRootSignature();
	gpsoDesc.VS = { v->shader12.data(), v->shader12.size() };
	gpsoDesc.PS = { p->shader12.data(), p->shader12.size() };
	gpsoDesc.InputLayout.NumElements = pCurInputLayout ? pCurInputLayout->NumElements : 0;
	gpsoDesc.InputLayout.pInputElementDescs = pCurInputLayout ? pCurInputLayout->pInputElementDescs : nullptr;
	gpsoDesc.RasterizerState = *finalRaster;
	gpsoDesc.BlendState = *finalBlend;
	gpsoDesc.DepthStencilState = *finalDepth;
	gpsoDesc.SampleMask = UINT_MAX;
	gpsoDesc.PrimitiveTopologyType = primitiveType;
	gpsoDesc.NumRenderTargets = finalRt->Count;
	memcpy(gpsoDesc.RTVFormats, finalRt->RTFormats, sizeof(DXGI_FORMAT) * finalRt->Count);
	gpsoDesc.DSVFormat = targets->m_dt ? RenderPlatform::ToDxgiFormat(targets->depthFormat) : DXGI_FORMAT_UNKNOWN;
	gpsoDesc.SampleDesc = msaaDesc;

	auto* device = curRenderPlat->AsD3D12Device();
	// Create it:
	HRESULT res = device->CreateGraphicsPipelineState(&gpsoDesc, SIMUL_PPV_ARGS(&pso.pipelineState));
	SIMUL_ASSERT(res == S_OK);
	if (pso.pipelineState)
	{
		std::string psoName = ((mTechName + " PSO for ") + base::QuickFormat("%d", finalRt ? finalRt->RTFormats[0] : 0));
		pso.pipelineState->SetName(std::wstring(psoName.begin(), psoName.end()).c_str());
	}

	return pso.pipelineState;
}

void EffectPass::CreateLocalRootSignature()
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3DBlob* blob = nullptr;
	ID3DBlob* error = nullptr;
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	memset(rootParameters, 0, sizeof(rootParameters));
	rootParameters[0].InitAsShaderResourceView(26);
	CD3DX12_ROOT_SIGNATURE_DESC rsDesc(ARRAYSIZE(rootParameters), rootParameters);
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	//rsDesc.Desc_1_1.Flags|=D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	//rsDesc.Version=D3D_ROOT_SIGNATURE_VERSION_1_1;
	HRESULT res = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (res != S_OK)
	{
		std::string err = static_cast<const char*>(error->GetBufferPointer());
		SIMUL_BREAK(err.c_str());
	}
	auto* device = renderPlatform->AsD3D12Device();
	V_CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), SIMUL_PPV_ARGS(&localRootSignature)));
	localRootSignature->SetName(L"Raytracing Local Root Signature");
	//mGRaytrac*ingSignature	=LoadRootSignature("//RTX.cso");
#endif

}

void EffectPass::CreateRaytracePso()
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device5* pDevice5 = ((dx12::RenderPlatform*)renderPlatform)->AsD3D12Device5();
	if (!pDevice5)
		return;
	// Create 7 subobjects that combine into a RTPSO:
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	// 3 - DXIL library
	// 1 - Triangle hit group
	// 1 - Shader config
	// 0 - Local root signature and association
	// 1 - Global root signature
	// 1 - Pipeline config

	std::map<void*, D3D12_EXPORT_DESC> exportDescs;
	std::map<void*, D3D12_DXIL_LIBRARY_DESC> dxilLibDescs;
	std::vector<D3D12_STATE_SUBOBJECT> subObjects;
	std::map<void*, D3D12_HIT_GROUP_DESC> hitGroupDescs;

	/*size_t num_libs=0;
	for(int i=crossplatform::SHADERTYPE_RAY_GENERATION;i<crossplatform::SHADERTYPE_CALLABLE+1;i++)
	{
		dx12::Shader* s= (dx12::Shader*)shaders[i];
		if(!s)
			continue;
		num_libs++;
	}*/

	std::map<void*, std::wstring> wnames;
	for (int i = crossplatform::SHADERTYPE_RAY_GENERATION; i < crossplatform::SHADERTYPE_INTERSECTION + 1; i++)
	{
		dx12::Shader* s = (dx12::Shader*)shaders[i];
		if (!s)
			continue;
		wnames[s] = base::StringToWString(s->entryPoint);
	}
	for (auto& hg : raytraceHitGroups)
	{
		wnames[&hg] = base::StringToWString(hg.first);
		crossplatform::Shader* sh[] = { hg.second.closestHit,hg.second.anyHit,hg.second.intersection };
		D3D12_HIT_GROUP_DESC& hitGroupDesc = hitGroupDescs[&hg];
		for (int i = 0; i < 3; i++)
		{
			if (sh[i])
			{
				wnames[sh[i]] = base::StringToWString(sh[i]->entryPoint);
			}
		}
	}
	for (auto& miss : missShaders)
	{
		dx12::Shader* s = (dx12::Shader*)miss.second;
		if (!s)
			continue;
		wnames[s] = base::StringToWString(s->entryPoint);
	}
	for (auto& callable : callableShaders)
	{
		dx12::Shader* s = (dx12::Shader*)callable.second;
		if (!s)
			continue;
		wnames[s] = base::StringToWString(s->entryPoint);
	}

	subObjects.reserve(20);
	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.

	for (int i = crossplatform::SHADERTYPE_RAY_GENERATION; i < crossplatform::SHADERTYPE_INTERSECTION + 1; i++)
	{
		dx12::Shader* s = (dx12::Shader*)shaders[i];
		if (!s)
			continue;
		crossplatform::ShaderType shaderType = (crossplatform::ShaderType)i;
		LPCWSTR entrypoint = wnames[s].c_str();
		D3D12_EXPORT_DESC& exportDesc = exportDescs[s];
		D3D12_DXIL_LIBRARY_DESC& dxilLibDesc = dxilLibDescs[s];

		exportDesc = { entrypoint, nullptr, D3D12_EXPORT_FLAG_NONE };
		D3D12_STATE_SUBOBJECT dxilLibSubObject;
		dxilLibDesc.DXILLibrary.pShaderBytecode = s->shader12.data();
		dxilLibDesc.DXILLibrary.BytecodeLength = s->shader12.size();
		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		dxilLibSubObject.pDesc = &dxilLibDesc;
		subObjects.push_back(dxilLibSubObject);
	}
	int group = 0;
	for (auto& hg : raytraceHitGroups)
	{
		crossplatform::Shader* sh[] = { hg.second.closestHit,hg.second.anyHit,hg.second.intersection };
		size_t G = (group + 1) * 20;
		for (int i = 0; i < 3; i++)
		{
			dx12::Shader* s = (dx12::Shader*)sh[i];
			if (!s)
				continue;
			crossplatform::ShaderType shaderType = s->type;
			LPCWSTR entrypoint = wnames[s].c_str();
			D3D12_EXPORT_DESC& exportDesc = exportDescs[s];
			D3D12_DXIL_LIBRARY_DESC& dxilLibDesc = dxilLibDescs[s];
			if (dxilLibDesc.NumExports > 0)
				continue;
			exportDesc = { entrypoint, nullptr, D3D12_EXPORT_FLAG_NONE };
			D3D12_STATE_SUBOBJECT dxilLibSubObject;
			dxilLibDesc.DXILLibrary.pShaderBytecode = s->shader12.data();
			dxilLibDesc.DXILLibrary.BytecodeLength = s->shader12.size();
			dxilLibDesc.NumExports = 1;
			dxilLibDesc.pExports = &exportDesc;
			dxilLibSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			dxilLibSubObject.pDesc = &dxilLibDesc;
			subObjects.push_back(dxilLibSubObject);
		}
		group++;
	}
	for (auto& miss : missShaders)
	{
		dx12::Shader* s = (dx12::Shader*)miss.second;
		if (!s)
			continue;
		LPCWSTR entrypoint = wnames[s].c_str();
		D3D12_EXPORT_DESC& exportDesc = exportDescs[s];
		D3D12_DXIL_LIBRARY_DESC& dxilLibDesc = dxilLibDescs[s];

		exportDesc = { entrypoint, nullptr, D3D12_EXPORT_FLAG_NONE };
		D3D12_STATE_SUBOBJECT dxilLibSubObject;
		dxilLibDesc.DXILLibrary.pShaderBytecode = s->shader12.data();
		dxilLibDesc.DXILLibrary.BytecodeLength = s->shader12.size();
		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		dxilLibSubObject.pDesc = &dxilLibDesc;
		subObjects.push_back(dxilLibSubObject);
	}
	for (auto& callable : callableShaders)
	{
		dx12::Shader* s = (dx12::Shader*)callable.second;
		if (!s)
			continue;
		LPCWSTR entrypoint = wnames[s].c_str();
		D3D12_EXPORT_DESC& exportDesc = exportDescs[s];
		D3D12_DXIL_LIBRARY_DESC& dxilLibDesc = dxilLibDescs[s];

		exportDesc = { entrypoint, nullptr, D3D12_EXPORT_FLAG_NONE };
		D3D12_STATE_SUBOBJECT dxilLibSubObject;
		dxilLibDesc.DXILLibrary.pShaderBytecode = s->shader12.data();
		dxilLibDesc.DXILLibrary.BytecodeLength = s->shader12.size();
		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		dxilLibSubObject.pDesc = &dxilLibDesc;
		subObjects.push_back(dxilLibSubObject);
	}

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
	dx12::Shader* h = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_CLOSEST_HIT];
	dx12::Shader* a = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_ANY_HIT];
	dx12::Shader* i = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_INTERSECTION];

	D3D12_HIT_GROUP_DESC& hitGroupDesc = hitGroupDescs[0];
	if (h)
	{
		hitGroupDesc.HitGroupExport = hitGroupExportName;
		hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitGroupDesc.ClosestHitShaderImport = wnames[h].c_str();
		if (a)
		{
			if (wnames[a].length())
			{
				hitGroupDesc.AnyHitShaderImport = wnames[a].c_str();
			}
		}
		if (i)
		{
			if (wnames[i].length())
			{
				//Type must be changed if using a intersetion section shader.
				hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
				hitGroupDesc.IntersectionShaderImport = wnames[i].c_str();
			}
		}

		D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
		hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroupSubobject.pDesc = &hitGroupDesc;
		subObjects.push_back(hitGroupSubobject);
	}
	else
	{
		int group = 0;
		for (auto& hg : raytraceHitGroups)
		{
			crossplatform::Shader* sh[3] = { hg.second.closestHit,hg.second.anyHit,hg.second.intersection };
			size_t G = (group + 1) * 20;
			D3D12_HIT_GROUP_DESC& hitGroupDesc = hitGroupDescs[&hg];
			hitGroupDesc.HitGroupExport = wnames[&hg].c_str();
			hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			if (hg.second.closestHit)
				hitGroupDesc.ClosestHitShaderImport = wnames[sh[0]].c_str();
			if (hg.second.anyHit)
				hitGroupDesc.AnyHitShaderImport = wnames[sh[1]].c_str();
			if (hg.second.intersection)
			{
				//Type must be changed if using a intersetion section shader.
				hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
				hitGroupDesc.IntersectionShaderImport = wnames[sh[2]].c_str();
			}

			D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
			hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
			hitGroupSubobject.pDesc = &hitGroupDesc;
			subObjects.push_back(hitGroupSubobject);
			group++;
		}
	}

	D3D12_STATE_SUBOBJECT shaderConfigStateObject;
	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
	shaderConfig.MaxPayloadSizeInBytes = maxPayloadSize;
	shaderConfig.MaxAttributeSizeInBytes = maxAttributeSize;
	shaderConfigStateObject.pDesc = &shaderConfig;
	shaderConfigStateObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;

	subObjects.push_back(shaderConfigStateObject);

	auto globalRootSignature = ((dx12::RenderPlatform*)renderPlatform)->GetRaytracingGlobalRootSignature();
	//auto localRootSignature = ((dx12::RenderPlatform*)renderPlatform)->GetRaytracingLocalRootSignature();

	/*D3D12_STATE_SUBOBJECT localRootSignatureSubObject;
	localRootSignatureSubObject.pDesc = &localRootSignature;
	localRootSignatureSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	subObjects.push_back(localRootSignatureSubObject);*/

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	D3D12_STATE_SUBOBJECT globalRootSignatureSubObject;
	globalRootSignatureSubObject.pDesc = &globalRootSignature;
	globalRootSignatureSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	subObjects.push_back(globalRootSignatureSubObject);

	// Local root signature and shader association
	//CreateLocalRootSignatureSubobjects(&raytracingPipeline);
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	D3D12_STATE_SUBOBJECT configurationSubObject;
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;
	pipelineConfig.MaxTraceRecursionDepth = maxTraceRecursionDepth;
	configurationSubObject.pDesc = &pipelineConfig;
	configurationSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	subObjects.push_back(configurationSubObject);

	D3D12_STATE_OBJECT_DESC stateObject;
	stateObject.NumSubobjects = (UINT)subObjects.size();
	stateObject.pSubobjects = subObjects.data();
	stateObject.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

#if PLATFORM_DEBUG_DISABLE == 1
	PrintStateObjectDesc(&stateObject);
#endif

	// Create the state object.
	HRESULT res = pDevice5->CreateStateObject(&stateObject, SIMUL_PPV_ARGS(&mRaytracePso));
	V_CHECK(res);
#endif
}

void EffectPass::InitShaderBindingTable()
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	if (!shaderBindingTable)
	{
		shaderBindingTable = (ShaderBindingTable*)renderPlatform->CreateShaderBindingTable();
		((crossplatform::ShaderBindingTable*)shaderBindingTable)->DefaultInitFromEffectPass(renderPlatform, this);
	}
#endif
}

////////////////////
//Effect Technique//
////////////////////

EffectTechnique::EffectTechnique(crossplatform::RenderPlatform* r, crossplatform::Effect* e)
	:crossplatform::EffectTechnique(r, e)
{
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

crossplatform::EffectPass* EffectTechnique::AddPass(const char* name, int i)
{
	crossplatform::EffectPass* p = new dx12::EffectPass(renderPlatform, effect);
	passes_by_name[name] = passes_by_index[i] = p;
	passes_by_name[name]->name = name;
	return p;
}

///////////
//Shader //
///////////

Shader::~Shader()
{
	SAFE_RELEASE(shaderTableResource);
}

void Shader::load(crossplatform::RenderPlatform* r, const char* filename_utf8, const void* data, size_t DataSize, crossplatform::ShaderType t)
{
	struct FileBlob
	{
		void* pData;
		uint32_t	DataSize;
	};
	// Copy shader
	type = t;
	shader12.resize(DataSize);
	memcpy(shader12.data(), data, DataSize);
}

///////////
//Effect //
///////////

EffectTechnique* Effect::CreateTechnique()
{
	return new dx12::EffectTechnique(renderPlatform,this);
}

Effect::Effect() 
	:mSamplersHeap(nullptr)
{
}

Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::Load(crossplatform::RenderPlatform* r, const char* filename_utf8, const std::map<std::string, std::string>& defines)
{
	renderPlatform = r;
	EnsureEffect(r, filename_utf8);

	crossplatform::Effect::Load(r, filename_utf8, defines);

	// Init the samplers heap:
	SAFE_DELETE(mSamplersHeap);
	auto rPlat = (dx12::RenderPlatform*)r;
	mSamplersHeap = new Heap;
	std::string name = filename_utf8;
	name += "_SamplersHeap";
	mSamplersHeap->Restore(rPlat, ResourceBindingLimits::NumSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, name.c_str());

	// Copy the default samplers into the heap:
	auto resLimits = rPlat->GetResourceBindingLimits();
	auto nullSampler = rPlat->GetNullSampler();
	for (int i = 0; i < ResourceBindingLimits::NumSamplers; i++)
	{
		auto sampler = (dx12::SamplerState*)samplerSlots[i];
		if (sampler)
		{
			rPlat->AsD3D12Device()->CopyDescriptorsSimple(1, mSamplersHeap->CpuHandle(), *sampler->AsD3D12SamplerState(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
		else
		{
			// if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				rPlat->AsD3D12Device()->CopyDescriptorsSimple(1, mSamplersHeap->CpuHandle(), nullSampler, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
		mSamplersHeap->Offset();
	}
	// Make the handles point at the start of the heap, careful, as if we write to those
	// handles we will override this values
	mSamplersHeap->Reset();
}

void Effect::PostLoad()
{
	// Ensure slots are correct
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			dx12::Shader* v = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_VERTEX];
			dx12::Shader* p = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_PIXEL];
			dx12::Shader* c = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_COMPUTE];
			// Graphics pipeline
			if (v && p)
			{
				CheckShaderSlots(v, v->shader12);
				CheckShaderSlots(p, p->shader12);
				pass.second->SetConstantBufferSlots(p->constantBufferSlots | v->constantBufferSlots);
				pass.second->SetTextureSlots(p->textureSlots | v->textureSlots);
				pass.second->SetTextureSlotsForSB(p->textureSlotsForSB | v->textureSlotsForSB);
				pass.second->SetRwTextureSlots(p->rwTextureSlots | v->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB(p->rwTextureSlotsForSB | v->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots(p->samplerSlots | v->samplerSlots);
			}
			// Compute pipeline
			if (c)
			{
				CheckShaderSlots(c, c->shader12);
				pass.second->SetConstantBufferSlots(c->constantBufferSlots);
				pass.second->SetTextureSlots(c->textureSlots);
				pass.second->SetTextureSlotsForSB(c->textureSlotsForSB);
				pass.second->SetRwTextureSlots(c->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB(c->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots(c->samplerSlots);
			}
			// Raytracing pipeline
			for (int s = crossplatform::SHADERTYPE_RAY_GENERATION; s < crossplatform::SHADERTYPE_CALLABLE + 1; s++)
			{
				dx12::Shader* sh = (dx12::Shader*)pass.second->shaders[s];
				if (!sh)
					continue;
				CheckShaderSlots(sh, sh->shader12);
				pass.second->SetUsesConstantBufferSlots(sh->constantBufferSlots);
				pass.second->SetUsesTextureSlots(sh->textureSlots);
				pass.second->SetUsesTextureSlotsForSB(sh->textureSlotsForSB);
				pass.second->SetUsesRwTextureSlots(sh->rwTextureSlots);
				pass.second->SetUsesRwTextureSlotsForSB(sh->rwTextureSlotsForSB);
				pass.second->SetUsesSamplerSlots(sh->samplerSlots);
			}
			dx12::Shader* r = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_RAY_GENERATION];
			if (r)
			{
				dx12::EffectPass* pass12 = ((dx12::EffectPass*)pass.second);
				pass12->CreateRaytracePso();
				pass12->InitShaderBindingTable();
			}
			dx12::EffectPass* ep = (EffectPass*)pass.second;
			ep->MakeResourceSlotMap();
		}
	}
}

void Effect::InvalidateDeviceObjects()
{
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			auto pass12 = (dx12::EffectPass*)pass.second;
			pass12->InvalidateDeviceObjects();
		}
	}
	techniques.clear();
	SAFE_DELETE(mSamplersHeap);
	crossplatform::Effect::InvalidateDeviceObjects();
}

crossplatform::EffectTechnique* Effect::GetTechniqueByName(const char* name)
{
	if (techniques.find(name) != techniques.end())
	{
		return techniques[name];
	}
	crossplatform::EffectTechniqueGroup* g = groups[""];
	if (!g)
		return nullptr;
	if (g->techniques.find(name) != g->techniques.end())
	{
		return g->techniques[name];
	}
	return NULL;
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
	if (techniques_by_index.find(index) != techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	crossplatform::EffectTechniqueGroup* g = groups[""];
	if (!g)
		return nullptr;
	if (g->techniques_by_index.find(index) != g->techniques_by_index.end())
	{
		return g->techniques_by_index[index];
	}
	return NULL;
}

void Effect::CheckShaderSlots(dx12::Shader* shader, const std::vector<uint8_t>& shaderBlob)
{
	HRESULT res = S_FALSE;
#ifdef WIN64
	// Load the shader reflection code
	if (!shader->mShaderReflection)
	{
		res = D3DReflect(shaderBlob.data(), shaderBlob.size(), IID_PPV_ARGS(&shader->mLibraryReflection));
		// unavailable:
		if (res != S_OK)
		{
			/*	IDxcContainerReflection* pReflection;
				UINT32 shaderIdx;
				gDxcDllHelper.CreateInstance(CLSID_DxcContainerReflection, &pReflection);
				pReflection->Load(pBlob);
				(pReflection->FindFirstPartKind(DXIL_FOURCC('D', 'X', 'I', 'L'), &shaderIdx));
				(pReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void**)&d3d12reflection));*/
			return;
		}

	}

	// Get shader description
	D3D12_SHADER_DESC shaderDesc = {};
	res = shader->mShaderReflection->GetDesc(&shaderDesc);
	SIMUL_ASSERT(res == S_OK);

	Shader temp_shader;
	// Reset the slot counter
	temp_shader.textureSlots = 0;
	temp_shader.textureSlotsForSB = 0;
	temp_shader.rwTextureSlots = 0;
	temp_shader.rwTextureSlotsForSB = 0;
	temp_shader.constantBufferSlots = 0;
	temp_shader.samplerSlots = 0;

	// Check the shader bound resources
	for (unsigned int i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC slotDesc = {};
		res = shader->mShaderReflection->GetResourceBindingDesc(i, &slotDesc);
		SIMUL_ASSERT(res == S_OK);

		D3D_SHADER_INPUT_TYPE type = slotDesc.Type;
		UINT slot = slotDesc.BindPoint;
		if (type == D3D_SIT_CBUFFER)
		{
			// NOTE: Local resources declared in the shader (like a matrix or a float4) will be added
			// to a global constant buffer but we shouldn't add it to our list
			// It looks like the only way to know if it is the global is by name or empty flag (0)
			if (slotDesc.uFlags)
			{
				temp_shader.setUsesConstantBufferSlot(slot);
			}
		}
		else if (type == D3D_SIT_TBUFFER)
		{
			SIMUL_BREAK("Check this");
		}
		else if (type == D3D_SIT_TEXTURE)
		{
			temp_shader.setUsesTextureSlot(slot);
		}
		else if (type == D3D_SIT_SAMPLER)
		{
			temp_shader.setUsesSamplerSlot(slot);
		}
		else if (type == D3D_SIT_UAV_RWTYPED)
		{
			temp_shader.setUsesRwTextureSlot(slot);
		}
		else if (type == D3D_SIT_STRUCTURED)
		{
			temp_shader.setUsesTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED)
		{
			temp_shader.setUsesRwTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_BYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWBYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_APPEND_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_CONSUME_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER)
		{
			SIMUL_BREAK("");
		}
		else
		{
			SIMUL_BREAK("Unknown slot type.");
		}
	}
	if (temp_shader.textureSlots != shader->textureSlots)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed textureSlots check." << std::endl;
		shader->textureSlots = temp_shader.textureSlots;
	}
	if (temp_shader.textureSlotsForSB != shader->textureSlotsForSB)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed textureSlotsForSB check." << std::endl;
		shader->textureSlotsForSB = temp_shader.textureSlotsForSB;
	}
	if (temp_shader.rwTextureSlots != shader->rwTextureSlots)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed rwTextureSlots check." << std::endl;
		shader->rwTextureSlots = temp_shader.rwTextureSlots;
	}
	if (temp_shader.rwTextureSlotsForSB != shader->rwTextureSlotsForSB)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed rwTextureSlotsForSB check." << std::endl;
		shader->rwTextureSlotsForSB = temp_shader.rwTextureSlotsForSB;
	}
	if (temp_shader.constantBufferSlots != shader->constantBufferSlots)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed constantBufferSlots check." << std::endl;
		shader->constantBufferSlots = temp_shader.constantBufferSlots;
	}
	if (temp_shader.samplerSlots != shader->samplerSlots)
	{
		SIMUL_INTERNAL_CERR_ONCE << shader->name.c_str() << ": failed samplerSlots check." << std::endl;
		shader->samplerSlots = temp_shader.samplerSlots;
	}
#endif
}

Heap* Effect::GetEffectSamplerHeap()
{
	return mSamplersHeap;
}