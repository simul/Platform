#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/PlatformStructuredBuffer.h"
#if defined(_XBOX_ONE) 
	#ifndef _GAMING_XBOX //Deprecated from the GDK
		#include <D3Dcompiler_x.h>
	#else
		#include <D3Dcompiler.h>
	#endif
#else
#include <D3Dcompiler.h>
#endif

using namespace simul;
using namespace dx11;
#pragma optimize("",off)

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

D3D11_QUERY toD3dQueryType(crossplatform::QueryType t)
{
	switch(t)
	{
		case crossplatform::QUERY_OCCLUSION:
			return D3D11_QUERY_OCCLUSION;
		case crossplatform::QUERY_TIMESTAMP:
			return D3D11_QUERY_TIMESTAMP;
		case crossplatform::QUERY_TIMESTAMP_DISJOINT:
			return D3D11_QUERY_TIMESTAMP_DISJOINT;
		default:
			return D3D11_QUERY_EVENT;
	};
}

void Query::SetName(const char *name)
{
	for(int i=0;i<QueryLatency;i++)
		SetDebugObjectName( d3d11Query[i], name );
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	InvalidateDeviceObjects();
	ID3D11Device *m_pd3dDevice=r->AsD3D11Device();
	D3D11_QUERY_DESC qdesc=
	{
		toD3dQueryType(type),0
	};
	for(int i=0;i<QueryLatency;i++)
	{
		gotResults[i]=true;
		doneQuery[i]=false;
		V_CHECK(m_pd3dDevice->CreateQuery(&qdesc,&d3d11Query[i]));
	}
}
void Query::InvalidateDeviceObjects() 
{
	for(int i=0;i<QueryLatency;i++)
		SAFE_RELEASE(d3d11Query[i]);
	for(int i=0;i<QueryLatency;i++)
	{
		gotResults[i]=true;
		doneQuery[i]=false;
	}
}

void Query::Begin(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->Begin(d3d11Query[currFrame]);
}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->End(d3d11Query[currFrame]);
	gotResults[currFrame]=false;
	doneQuery[currFrame]=true;
}

bool Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
{
	gotResults[currFrame]=true;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	currFrame = (currFrame + 1) % QueryLatency;
	if(!doneQuery[currFrame])
		return false;
	// Get the data from the "next" query - which is the oldest!
	HRESULT hr=pContext->GetData(d3d11Query[currFrame],data,(UINT)sz,0);
	if(hr== S_OK)
	{
		gotResults[currFrame]=true;
	}
	return hr== S_OK;
}

RenderState::RenderState()
	:m_depthStencilState(nullptr)
	,m_blendState(nullptr)
	, m_rasterizerState(nullptr)
{
}

void RenderState::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_depthStencilState)
	SAFE_RELEASE(m_blendState)
	SAFE_RELEASE(m_rasterizerState)
}

RenderState::~RenderState()
{
	InvalidateDeviceObjects();
}

EffectTechnique::EffectTechnique(crossplatform::RenderPlatform* r, crossplatform::Effect* e)
	:crossplatform::EffectTechnique(r, e)
{
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

dx11::Effect::Effect()
{
}

Shader::~Shader()
{
	Release();
}

void Shader::Release()
{
	switch (type)
	{
	case crossplatform::SHADERTYPE_PIXEL:
		SAFE_RELEASE(pixelShader);
		break;
	case crossplatform::SHADERTYPE_VERTEX:
		SAFE_RELEASE(vertexShader);
		break;
	case crossplatform::SHADERTYPE_COMPUTE:
		SAFE_RELEASE(computeShader);
		break;
	};
	shader11.clear();
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
	shader11.resize(DataSize);
	memcpy(shader11.data(), data, DataSize);
	auto* pd3dDevice = r->AsD3D11Device();
	// Copy shader
	type = t;
	if (t == crossplatform::SHADERTYPE_PIXEL)
	{
		pd3dDevice->CreatePixelShader(shader11.data(), shader11.size(), NULL, &pixelShader);
	}
	else if (t == crossplatform::SHADERTYPE_VERTEX)
	{
		pd3dDevice->CreateVertexShader(shader11.data(), shader11.size(), NULL, &vertexShader);
	}
	else if (t == crossplatform::SHADERTYPE_COMPUTE)
	{
		pd3dDevice->CreateComputeShader(shader11.data(), shader11.size(), NULL, &computeShader);
	}
	else if (t == crossplatform::SHADERTYPE_GEOMETRY)
	{
		SIMUL_INTERNAL_CERR << "Geometry shaders are not implemented." << std::endl;
	}
	else 
	{
		SIMUL_INTERNAL_CERR << "This type of shader is not implemented." << std::endl;
	}
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx11::EffectTechnique(renderPlatform,this);
}

void Effect::Load(crossplatform::RenderPlatform *r,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	renderPlatform=r;
	if(!renderPlatform)
		return;
	EnsureEffect(r, filename_utf8);
	crossplatform::Effect::Load(r, filename_utf8, defines);
}

void Effect::PostLoad()
{
	// Ensure slots are correct
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			dx11::Shader* v = (dx11::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_VERTEX];
			dx11::Shader* p = (dx11::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_PIXEL];
			dx11::Shader* c = (dx11::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_COMPUTE];
			// Graphics pipeline
			if (v && p)
			{
				//CheckShaderSlots(v, v->vertexShader11);
				//CheckShaderSlots(p, p->pixelShader11);
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
				//CheckShaderSlots(c, c->computeShader11);
				pass.second->SetConstantBufferSlots(c->constantBufferSlots);
				pass.second->SetTextureSlots(c->textureSlots);
				pass.second->SetTextureSlotsForSB(c->textureSlotsForSB);
				pass.second->SetRwTextureSlots(c->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB(c->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots(c->samplerSlots);
			}
			dx11::EffectPass* ep = (EffectPass*)pass.second;
			ep->MakeResourceSlotMap();
		}
	}
}


dx11::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	crossplatform::Effect::InvalidateDeviceObjects();
}

crossplatform::EffectTechnique* dx11::Effect::GetTechniqueByName(const char* name)
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

crossplatform::EffectTechnique* dx11::Effect::GetTechniqueByIndex(int index)
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

void Effect::SetConstantBuffer(crossplatform::DeviceContext& deviceContext, crossplatform::ConstantBufferBase* s)
{
	//RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
	s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());
	crossplatform::Effect::SetConstantBuffer(deviceContext, s);
}

void Effect::CheckShaderSlots(dx11::Shader* shader, ID3DBlob* shaderBlob)
{
}

void EffectPass::SetConstantBuffers(crossplatform::DeviceContext& deviceContext,crossplatform::ConstantBufferAssignmentMap& cBuffers)
{
	auto cmdList = deviceContext.asD3D11DeviceContext();
	auto rPlat = (dx11::RenderPlatform*)deviceContext.renderPlatform;
	int usedSlots = 0;

	// The handles for the required constant buffers:
	for (int i = 0; i < numConstantBufferResourceSlots; i++)
	{
		int slot = constantBufferResourceSlots[i];
		auto cb = cBuffers[slot];
		if (!cb || !usesConstantBufferSlot(slot) || slot != cb->GetIndex())
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at slot " << slot << "." << std::endl;
	
			continue; 
		}
		auto d11cb = (dx11::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		appliedConstantBuffers[slot] = d11cb->asD3D11Buffer();
		usedSlots |= (1 << slot);

		cmdList->CSSetConstantBuffers(slot, 1, &(appliedConstantBuffers[slot]));
		cmdList->VSSetConstantBuffers(slot, 1, &(appliedConstantBuffers[slot]));
		cmdList->PSSetConstantBuffers(slot, 1, &(appliedConstantBuffers[slot]));
	}
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute)
{
	dx11::Shader* c = (dx11::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	auto cmdList = deviceContext.asD3D11DeviceContext();
	// We will set the DescriptorHeaps and RootDescriptorTablesArguments from ApplyContextState (at RenderPlatform)
	// The create methods will only create the pso once (or if there is a change in the state)
	if (c)
	{
		cmdList->CSSetShader(c->computeShader, nullptr, 0);
	}
	else
	{
		dx11::Shader* v = (dx11::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
		dx11::Shader* p = (dx11::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
		cmdList->VSSetShader(v->vertexShader, nullptr, 0);
		cmdList->PSSetShader(p->pixelShader,nullptr,0);
	}
	if (blendState)
	{
		auto* b = (dx11::RenderState*)blendState;
		vec4 bf = { 0.0f, 0.0f, 0.0f, 0.0f };
		cmdList->OMSetBlendState(b->m_blendState,bf, 0xFFFFFFFF);
	}
	if (depthStencilState)
	{
		auto* d = (dx11::RenderState*)depthStencilState;
		cmdList->OMSetDepthStencilState(d->m_depthStencilState,0);
	}
	if (rasterizerState)
	{
		auto* r= (dx11::RenderState*)rasterizerState;
		cmdList->RSSetState(r->m_rasterizerState);
	}
}

void EffectPass::SetSamplers(crossplatform::DeviceContext& deviceContext, crossplatform::SamplerStateAssignmentMap& samplers)
{
	auto cmdList = deviceContext.asD3D11DeviceContext();
	auto rPlat = (dx11::RenderPlatform*)deviceContext.renderPlatform;
	int usedSlots = 0;

	dx11::Shader* compute = (dx11::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
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
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << name.c_str() << ". Sampler slot " << slot << " is invalid." << std::endl;

			continue;
		}
		auto *s=samp->asD3D11SamplerState();
		if (compute)
		{
			cmdList->CSSetSamplers(slot, 1, &s);
		}
		else
		{
			cmdList->VSSetSamplers(slot, 1, &s);
			cmdList->PSSetSamplers(slot, 1, &s);
		}
		usedSlots |= (1 << slot);
	}
}

void EffectPass::SetSRVs(crossplatform::DeviceContext& deviceContext, crossplatform::TextureAssignmentMap& textures, crossplatform::StructuredBufferAssignmentMap& sBuffers)
{
	auto cmdList = deviceContext.asD3D11DeviceContext();
	auto rPlat = (dx11::RenderPlatform*)deviceContext.renderPlatform;
	int usedSBSlots = 0;
	int usedTextureSlots = 0;

	dx11::Shader* compute= (dx11::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	// Iterate over the textures:
	for (int i = 0; i < numResourceSlots; i++)
	{
		int slot = resourceSlots[i];
		auto ta = textures[slot];
		if (!ta.texture || !ta.texture->IsValid())
		{
			continue;
		}
		((dx11::Texture*)ta.texture)->FinishLoading(deviceContext);
		auto* res = ta.texture->AsD3D11ShaderResourceView(ta.resourceType, ta.index, ta.mip);
		if (compute)
		{
			cmdList->CSSetShaderResources(slot, 1, &res);
		}
		else
		{
			cmdList->VSSetShaderResources(slot, 1, &res);
			cmdList->PSSetShaderResources(slot, 1, &res);
		}
		usedTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numSbResourceSlots; i++)
	{
		int slot = sbResourceSlots[i];
		if ((usedTextureSlots&(1<<slot))!=0)
		{
			SIMUL_INTERNAL_CERR << "The slot: " << slot << " at pass: " << name.c_str() << " has already being used by a texture. \n";
		}
		auto sb = (dx11::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb)
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << name.c_str() << ". Structured buffer slot " << slot << " is invalid." << std::endl;
			continue;
		}
		auto *res= sb->AsD3D11ShaderResourceView();
		if (compute)
		{
			cmdList->CSSetShaderResources(slot, 1, &res);
		}
		else
		{
			cmdList->VSSetShaderResources(slot, 1, &res);
			cmdList->PSSetShaderResources(slot, 1, &res);
		}
		usedSBSlots |= (1 << slot);
	}
}

void EffectPass::SetUAVs(crossplatform::DeviceContext& deviceContext, crossplatform::TextureAssignmentMap& rwTextures, crossplatform::StructuredBufferAssignmentMap& sBuffers)
{
	auto cmdList = deviceContext.asD3D11DeviceContext();
	auto rPlat = (dx11::RenderPlatform*)deviceContext.renderPlatform;
	int usedRwSBSlots = 0;
	int usedRwTextureSlots = 0;
	dx11::Shader* compute = (dx11::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

	// Iterate over the textures:
	for (int i = 0; i < numRwResourceSlots; i++)
	{
		int slot = rwResourceSlots[i];
		auto ta = rwTextures[slot];

		// If the texture is null or invalid, set a dummy:
		// NOTE: again, this disables any slot checks...
		if (!ta.texture || !ta.texture->IsValid())
		{
			continue;
		}
		auto *uav = ta.texture->AsD3D11UnorderedAccessView( ta.index, ta.mip);
		if (compute)
		{
			cmdList->CSSetUnorderedAccessViews(slot, 1, &uav,nullptr);
		}
		usedRwTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numRwSbResourceSlots; i++)
	{
		int slot = rwSbResourceSlots[i];
		if ((usedRwTextureSlots & (1 << slot)) != 0)
		{
			SIMUL_INTERNAL_CERR << "The slot: " << slot << " at pass: " << name.c_str() << ", has already being used by a RWTexture. \n";
		}
		auto sb = (dx11::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb)
		{
			SIMUL_INTERNAL_CERR << "Resource binding error at: " << name.c_str() << ". RWStructured buffer slot " << slot << " is invalid." << std::endl;
			continue;
		}
		auto *uav = sb->AsD3D11UnorderedAccessView();
		if (compute)
		{
			cmdList->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
		}
		usedRwSBSlots |= (1 << slot);


		// Temp:

		renderPlatform->ResourceBarrierUAV(deviceContext, sb);
	}
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::EffectTechnique* effectTechnique, int pass_num)
{
	crossplatform::Effect::Apply(deviceContext, effectTechnique, pass_num);
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::EffectTechnique* effectTechnique, const char* passname)
{
	crossplatform::Effect::Apply(deviceContext, effectTechnique, passname);
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::ContextState* cs = renderPlatform->GetContextState(deviceContext);
	auto *p=cs->currentEffectPass;
	cs->textureAssignmentMapValid = false;
	cs->rwTextureAssignmentMapValid = false;
	crossplatform::Effect::Unapply(deviceContext);
	crossplatform::Effect::Apply(deviceContext,  p);
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::Unapply(deviceContext);
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	auto c=deviceContext.asD3D11DeviceContext();
	static ID3D11ShaderResourceView *src[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	c->VSSetShaderResources(0,32,src);
	c->HSSetShaderResources(0,32,src);
	c->DSSetShaderResources(0,32,src);
	c->GSSetShaderResources(0,32,src);
	c->PSSetShaderResources(0,32,src);
	c->CSSetShaderResources(0,32,src);
	static ID3D11UnorderedAccessView *uav[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	c->CSSetUnorderedAccessViews(0,8,uav,0);
	crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx11::EffectPass(renderPlatform,effect);
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}

EffectPass::EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e)
	:crossplatform::EffectPass(r,e)
{
}

