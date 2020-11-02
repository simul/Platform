#define NOMINMAX

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
#include "SimulDirectXHeader.h"
#include "ThisPlatform/Direct3D12.h"

#include <algorithm>
#include <string>

using namespace simul;
using namespace dx12;

inline bool IsPowerOfTwo( UINT64 n )
{
	return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
	SIMUL_ASSERT( IsPowerOfTwo(multiple) );

	return (value + multiple - 1) & ~(multiple - 1);
}

template< class T >
UINT64 BytePtrToUint64( _In_ T* ptr )
{
	return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}

RenderState::RenderState()
{
	BlendDesc							= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RasterDesc							= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	DepthStencilDesc					= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RasterDesc.FrontCounterClockwise	= true;	
}

RenderState::~RenderState()
{
}

EffectTechnique::EffectTechnique(crossplatform::RenderPlatform *r,crossplatform::Effect *e)
	:crossplatform::EffectTechnique(r,e)
{
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

Effect::Effect():
	mSamplersHeap(nullptr)
{
}

EffectTechnique* Effect::CreateTechnique()
{
	return new dx12::EffectTechnique(renderPlatform,this);
}

void Shader::load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *data, size_t DataSize, crossplatform::ShaderType t)
{
	struct FileBlob
	{
		void*		pData;
		uint32_t	DataSize;
	};
	// Copy shader
	type = t;
	shader12.resize(DataSize);
	memcpy(shader12.data(), data, DataSize);
}
Shader::~Shader()
{
	
}

void Effect::Load(crossplatform::RenderPlatform* r,const char* filename_utf8,const std::map<std::string,std::string>& defines)
{	
	renderPlatform = r;
	EnsureEffect(r, filename_utf8);

	crossplatform::Effect::Load(r, filename_utf8, defines);

	// Init the samplers heap:
	SAFE_DELETE(mSamplersHeap);
	auto rPlat		  = (dx12::RenderPlatform*)r;
	mSamplersHeap	   = new Heap;
	std::string name	= filename_utf8;
	name				+= "_SamplersHeap";
	mSamplersHeap->Restore(rPlat, ResourceBindingLimits::NumSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, name.c_str());
	
	// Copy the default samplers into the heap:
	auto resLimits	  = rPlat->GetResourceBindingLimits();
	auto nullSampler	= rPlat->GetNullSampler();
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
				pass.second->SetConstantBufferSlots	(p->constantBufferSlots | v->constantBufferSlots);
				pass.second->SetTextureSlots		(p->textureSlots		| v->textureSlots);
				pass.second->SetTextureSlotsForSB	(p->textureSlotsForSB	| v->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(p->rwTextureSlots		| v->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(p->rwTextureSlotsForSB | v->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(p->samplerSlots		| v->samplerSlots);
			}
			// Compute pipeline
			if (c)
			{
				CheckShaderSlots(c, c->shader12);
				pass.second->SetConstantBufferSlots	(c->constantBufferSlots);
				pass.second->SetTextureSlots		(c->textureSlots);
				pass.second->SetTextureSlotsForSB	(c->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(c->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(c->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(c->samplerSlots);
			}
			dx12::EffectPass *ep=(EffectPass *)pass.second;
			ep->MakeResourceSlotMap();
		}
	}
}

dx12::Effect::~Effect()
{
	InvalidateDeviceObjects();
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
	SAFE_RELEASE(mSamplersHeap);
	crossplatform::Effect::InvalidateDeviceObjects();
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	crossplatform::EffectTechniqueGroup *g=groups[""];
	if(!g)
		return nullptr;
	if(g->techniques.find(name)!=g->techniques.end())
	{
		return g->techniques[name];
	}
	return NULL;
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	crossplatform::EffectTechniqueGroup *g=groups[""];
	if(!g)
		return nullptr;
	if(g->techniques_by_index.find(index)!=g->techniques_by_index.end())
	{
		return g->techniques_by_index[index];
	}
	return NULL;
}

void Effect::CheckShaderSlots(dx12::Shader * shader, const std::vector<uint8_t>& shaderBlob)
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
	temp_shader.textureSlots		= 0;
	temp_shader.textureSlotsForSB	= 0;
	temp_shader.rwTextureSlots		= 0;
	temp_shader.rwTextureSlotsForSB = 0;
	temp_shader.constantBufferSlots = 0;
	temp_shader.samplerSlots		= 0;

	// Check the shader bound resources
	for (unsigned int i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC slotDesc = {};
		res = shader->mShaderReflection->GetResourceBindingDesc(i, &slotDesc);
		SIMUL_ASSERT(res == S_OK);

		D3D_SHADER_INPUT_TYPE type	= slotDesc.Type;
		UINT slot					= slotDesc.BindPoint;
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


crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx12::EffectPass(renderPlatform,effect);
	passes_by_name[name]=passes_by_index[i]=p;
	passes_by_name[name]->name=name;
	return p;
}

void EffectPass::InvalidateDeviceObjects()
{
	// TO-DO:  memory leaks here??
	auto pl=(dx12::RenderPlatform*)renderPlatform;
	pl->PushToReleaseManager(mComputePso,"Compute PSO");
	mComputePso=nullptr;
	for (auto& ele : mGraphicsPsoMap)
	{
		pl->PushToReleaseManager(ele.second, "Graphics PSO");
	}
	mGraphicsPsoMap.clear();
}

void EffectPass::Apply(crossplatform::DeviceContext &deviceContext,bool asCompute)
{		
	dx12::Shader* c = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	auto cmdList	= deviceContext.asD3D12Context();
	// We will set the DescriptorHeaps and RootDescriptorTablesArguments from ApplyContextState (at RenderPlatform)
	// The create methods will only create the pso once (or if there is a change in the state)
	if (c)
	{
		CreateComputePso(deviceContext);
		if(mComputePso)
			cmdList->SetPipelineState(mComputePso);
	}
	else
	{
		cmdList->SetPipelineState(mGraphicsPsoMap[CreateGraphicsPso(*deviceContext.AsGraphicsDeviceContext())]);
	}
}

void EffectPass::SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers, dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& deviceContext)
{
	auto rPlat				= (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	int usedSlots			= 0;
	auto nullSampler		= rPlat->GetNullSampler();

	// The handles for the required samplers:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSamplers> srcHandles;
	for (int i = 0; i < numSamplerResourceSlots; i++)
	{
		int slot							= samplerResourceSlots[i];
		crossplatform::SamplerState* samp   = nullptr;
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
		usedSlots		|= (1 << slot);
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
	auto rPlat					= (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits		= rPlat->GetResourceBindingLimits();
	int usedSlots				= 0;
	auto nullCbv				= rPlat->GetNullCBV();

	// Clean up the handles array
	memset(mCbSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * mCbSrcHandles.size());

	// The handles for the required constant buffers:
	for (int i = 0; i < numConstantBufferResourceSlots; i++)
	{
		int slot = constantBufferResourceSlots[i];
		auto cb	 = cBuffers[slot];
		if (!cb || !usesConstantBufferSlot(slot) || slot != cb->GetIndex())
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << mTechName << ". Constant buffer slot " << slot << " is invalid." << std::endl;
			mCbSrcHandles[slot] = nullCbv;
			continue;
		}
		cb->GetPlatformConstantBuffer()->Apply(deviceContext, cb->GetSize(), cb->GetAddr());
		auto d12cb			= (dx12::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		mCbSrcHandles[slot]	= d12cb->AsD3D12ConstantBuffer();
		usedSlots			|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumCBV; s++)
	{
		// All CB must have valid descriptors for hardware tier 2 and bellow:
		if (!usesConstantBufferSlot(s))
		{
			// if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
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
	auto rPlat				= (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullSrv			= rPlat->GetNullSRV();
	int usedSBSlots			= 0;
	int usedTextureSlots	= 0;

	// The handles for the required SRVs:
	memset(mSrvSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumSRV);
	memset(mSrvUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumSRV);
	bool is_pixel_shader=(deviceContext.contextState.currentEffectPass->shaders[crossplatform::SHADERTYPE_PIXEL]!=nullptr);
	// Iterate over the textures:
	for (int i = 0; i < numResourceSlots; i++)
	{
		int slot	= resourceSlots[i];
		auto ta		= textures[slot];

		// If the texture is null or invalid, set a dummy:
		// NOTE: this basically disables any slot checks as we will always
		// set something
		if (!ta.texture || !ta.texture->IsValid())
		{
			if(ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
			else
			{
				ta.texture = rPlat->GetDummy2D();
			}
		}
		((dx12::Texture*)ta.texture)->FinishLoading(deviceContext);
		mSrvSrcHandles[slot]	= *ta.texture->AsD3D12ShaderResourceView(deviceContext,true, ta.resourceType, ta.index, ta.mip,is_pixel_shader);
		mSrvUsedSlotsArray[slot]= true;
		usedTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numSbResourceSlots; i++)
	{
		int slot = sbResourceSlots[i];
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
		mSrvSrcHandles[slot]		= *sb->AsD3D12ShaderResourceView(deviceContext);
		mSrvUsedSlotsArray[slot]	= true;
		usedSBSlots					|= (1 << slot);
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
		else
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

void EffectPass::SetUAVs(crossplatform::TextureAssignmentMap & rwTextures, crossplatform::StructuredBufferAssignmentMap & sBuffers, dx12::Heap * frameHeap, ID3D12Device * device, crossplatform::DeviceContext & deviceContext)
{
	auto rPlat				= (dx12::RenderPlatform*)deviceContext.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullUav			= rPlat->GetNullUAV();
	int usedRwSBSlots		= 0;
	int usedRwTextureSlots	= 0;

	// The the handles for the required UAVs:
	memset(mUavSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumUAV);
	memset(mUavUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumUAV);

	// Iterate over the textures:
	for (int i = 0; i < numRwResourceSlots; i++)
	{
		int slot	= rwResourceSlots[i];
		auto ta		= rwTextures[slot];

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
		mUavSrcHandles[slot]	= *ta.texture->AsD3D12UnorderedAccessView(deviceContext,ta.index, ta.mip);
		mUavUsedSlotsArray[slot]= true;
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
		mUavSrcHandles[slot]		= *sb->AsD3D12UnorderedAccessView(deviceContext);
		mUavUsedSlotsArray[slot]	= true;
		usedRwSBSlots			|= (1 << slot);


		// Temp:
		
		renderPlatform->ResourceBarrierUAV(deviceContext,sb);
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

	mTechName		   = this->name;
	auto curRenderPlat  = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	dx12::Shader *c	 = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	if (!c)
	{
		SIMUL_INTERNAL_CERR << "The pass " << mTechName <<  " does not have valid shaders!!! \n";
		return;
	}

	mIsCompute								  = true;

	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc  = {};
	cpsoDesc.CS								 = { c->shader12.data(), c->shader12.size() };
	cpsoDesc.pRootSignature					 = curRenderPlat->GetGraphicsRootSignature();
	cpsoDesc.NodeMask						   = 0;
	HRESULT res								 = curRenderPlat->AsD3D12Device()->CreateComputePipelineState(&cpsoDesc,SIMUL_PPV_ARGS(&mComputePso));
	SIMUL_ASSERT(res == S_OK);
	if (res == S_OK)
	{
		SIMUL_GPU_TRACK_MEMORY(mComputePso,100)	// TODO: not the real size!
		std::wstring name   = L"ComputePSO_";
		name				+= std::wstring(mTechName.begin(), mTechName.end());
		mComputePso->SetName(name.c_str());
	}
	else
	{
		SIMUL_INTERNAL_CERR << "Failed to create compute PSO.\n";
		SIMUL_BREAK_ONCE("Failed to create compute PSO")
	}
}

size_t EffectPass::CreateGraphicsPso(crossplatform::GraphicsDeviceContext& deviceContext)
{
	auto curRenderPlat  = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	mTechName		   = name;
	dx12::Shader* v	 = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
	dx12::Shader* p	 = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
	if (!v && !p)
	{
		SIMUL_INTERNAL_CERR << "The pass " << name.c_str() << " does not have valid shaders!!! \n";
		return (size_t)-1;
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
	DXGI_SAMPLE_DESC msaaDesc = {1,0};
	if (curRenderPlat->IsMSAAEnabled())
	{
		msaaDesc = curRenderPlat->GetMSAAInfo();
	}
	size_t msaaHash = msaaDesc.Count + msaaDesc.Quality;

	// Get the current targets:
	const crossplatform::TargetsAndViewport* targets = &deviceContext.defaultTargetsAndViewport;
	if (!deviceContext.targetStack.empty())
	{
		targets = deviceContext.targetStack.top();
	}

	// Current render target output state:
	D3D12_RENDER_TARGET_FORMAT_DESC* finalRt = nullptr;
	size_t rthash = 0;
	if (renderTargetFormatState)
	{
		finalRt = &((dx12::RenderState*)renderTargetFormatState)->RtFormatDesc;
		// Check formats against currently bound targets
		// The debug layer will catch this, but its nice for us to be aware of this issue
		for (uint i = 0; i < finalRt->Count; i++)
		{
			if (finalRt->RTFormats[i] != RenderPlatform::ToDxgiFormat(targets->rtFormats[i]))
			{
				SIMUL_INTERNAL_CERR << "Current applied render target idx(" << i << ")" <<  " does not match the format specified by the effect (" << name.c_str() << "). \n";
			}
		}
		rthash = finalRt->GetHash();
	}
	else
	{
		// Lets check whats the state set by the render platform:
		D3D12_RENDER_TARGET_FORMAT_DESC tmpState  = {};
		tmpState.Count							= targets->num;
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
		}
		rthash  = tmpState.GetHash();

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
	size_t hash = (uint64_t)&finalBlend ^ (uint64_t)&finalDepth ^ (uint64_t)&finalRaster ^ finalRt->GetHash() ^ msaaHash;

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
		return hash;
	}

	// Build a new pso pair <pixel format, PSO>
	std::pair<size_t, ID3D12PipelineState*> psoPair;
	psoPair.first   = hash;
	mIsCompute	  = false;

	// Try to get the input layout (if none, we dont need to set it to the pso)
	D3D12_INPUT_LAYOUT_DESC* pCurInputLayout	= curRenderPlat->GetCurrentInputLayout();

	// Find the current primitive topology type
	D3D12_PRIMITIVE_TOPOLOGY curTopology		= curRenderPlat->GetCurrentPrimitiveTopology();
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
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc = {};
	gpsoDesc.pRootSignature					 = curRenderPlat->GetGraphicsRootSignature();
	gpsoDesc.VS								 = { v->shader12.data(), v->shader12.size() };
	gpsoDesc.PS								 = { p->shader12.data(), p->shader12.size() };
	gpsoDesc.InputLayout.NumElements			= pCurInputLayout ? pCurInputLayout->NumElements : 0;
	gpsoDesc.InputLayout.pInputElementDescs	 = pCurInputLayout ? pCurInputLayout->pInputElementDescs : nullptr;
	gpsoDesc.RasterizerState					= *finalRaster;
	gpsoDesc.BlendState						 = *finalBlend;
	gpsoDesc.DepthStencilState				  = *finalDepth;
	gpsoDesc.SampleMask						 = UINT_MAX;
	gpsoDesc.PrimitiveTopologyType			  = primitiveType;
	gpsoDesc.NumRenderTargets				   = finalRt->Count;
	memcpy(gpsoDesc.RTVFormats, finalRt->RTFormats, sizeof(DXGI_FORMAT) * finalRt->Count);
	gpsoDesc.DSVFormat						  = targets->m_dt ? RenderPlatform::ToDxgiFormat(targets->depthFormat): DXGI_FORMAT_UNKNOWN;
	gpsoDesc.SampleDesc						 = msaaDesc;

	// Create it:
	HRESULT res			 = curRenderPlat->AsD3D12Device()->CreateGraphicsPipelineState(&gpsoDesc,SIMUL_PPV_ARGS(&psoPair.second));
	SIMUL_ASSERT(res == S_OK);	
	psoPair.second->SetName(std::wstring(mTechName.begin(), mTechName.end()).c_str());
	mGraphicsPsoMap.insert(psoPair);

	return hash;
}

EffectPass::EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e):
	crossplatform::EffectPass(r,e)
	,mInUseOverrideDepthState(nullptr)
	,mInUseOverrideBlendState(nullptr)
{
}

EffectPass::~EffectPass()
{
	InvalidateDeviceObjects();
}