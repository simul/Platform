#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"

#include <string>

using namespace simul;
using namespace dx11;
#pragma optimize("",off)
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
		m_pd3dDevice->CreateQuery(&qdesc,&d3d11Query[i]);
	}
}
void Query::InvalidateDeviceObjects() 
{
	for(int i=0;i<QueryLatency;i++)
		SAFE_RELEASE(d3d11Query[i]);
	for(int i=0;i<QueryLatency;i++)
		gotResults[i]=true;
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
}

bool Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	currFrame = (currFrame + 1) % QueryLatency;
	// Get the data from the "next" query - which is the oldest!
	HRESULT hr=pContext->GetData(d3d11Query[currFrame],data,(UINT)sz,0);
	if(hr== S_OK)
	{
		gotResults[currFrame]=true;
	}
	return hr== S_OK;
}

RenderState::RenderState()
	:m_depthStencilState(NULL)
	,m_blendState(NULL)
{
}
RenderState::~RenderState()
{
	SAFE_RELEASE(m_depthStencilState)
	SAFE_RELEASE(m_blendState)
}
static const int NUM_STAGING_BUFFERS=4;
PlatformStructuredBuffer::PlatformStructuredBuffer()
				:num_elements(0)
				,element_bytesize(0)
				,buffer(0)
				,read_data(0)
				,shaderResourceView(0)
				,unorderedAccessView(0)
				,lastContext(NULL)
			{
				stagingBuffers=new ID3D11Buffer*[NUM_STAGING_BUFFERS];
				for(int i=0;i<NUM_STAGING_BUFFERS;i++)
					stagingBuffers[i]=NULL;
				memset(&mapped,0,sizeof(mapped));
			}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data)
{
	InvalidateDeviceObjects();
	num_elements=ct;
	element_bytesize=unit_size;
	D3D11_BUFFER_DESC sbDesc;
	memset(&sbDesc,0,sizeof(sbDesc));
	if(computable)
	{
		sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.Usage				=D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags		=0;
	}
	else
	{
		sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE ;
		sbDesc.Usage				=D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
	}
	sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
	sbDesc.StructureByteStride	=element_bytesize;
	sbDesc.ByteWidth			=element_bytesize*num_elements;
	
	D3D11_SUBRESOURCE_DATA sbInit = {init_data, 0, 0};

	V_CHECK(renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &buffer));
	
	for(int i=0;i<NUM_STAGING_BUFFERS;i++)
		SAFE_RELEASE(stagingBuffers[i]);
	// May not be needed, but uses only a small amount of memory:
	if(computable)
	{
		sbDesc.BindFlags=0;
		sbDesc.Usage				=D3D11_USAGE_STAGING;
		sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_READ;
		sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
		for(int i=0;i<NUM_STAGING_BUFFERS;i++)
			renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &stagingBuffers[i]);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	memset(&srv_desc,0,sizeof(srv_desc));
	srv_desc.Format						=DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension				=D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.ElementOffset		=0;
	srv_desc.Buffer.ElementWidth		=0;
	srv_desc.Buffer.FirstElement		=0;
	srv_desc.Buffer.NumElements			=num_elements;
	V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(buffer, &srv_desc,&shaderResourceView));

	if(computable)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		memset(&uav_desc,0,sizeof(uav_desc));
		uav_desc.Format						=DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement		=0;
		uav_desc.Buffer.Flags				=0;
		uav_desc.Buffer.NumElements			=num_elements;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(buffer, &uav_desc,&unorderedAccessView));
	}
}

void *PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	if(!mapped.pData)
		lastContext->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
	void *ptr=(void *)mapped.pData;
	return ptr;
}

const void *PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	mapped.pData=NULL;
	HRESULT hr=DXGI_ERROR_WAS_STILL_DRAWING;
	int wait=0;
	while(hr==DXGI_ERROR_WAS_STILL_DRAWING)
	{
		hr=lastContext->Map(stagingBuffers[NUM_STAGING_BUFFERS-1],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);//
		wait++;
	}
	if(wait>1)
		SIMUL_CERR<<"PlatformStructuredBuffer::OpenReadBuffer waited "<<wait<<" times."<<std::endl;
	if(hr!=S_OK)
		mapped.pData=NULL;
	return mapped.pData;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext->Unmap(stagingBuffers[NUM_STAGING_BUFFERS-1], 0 );
	mapped.pData=NULL;
	lastContext=NULL;
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	for(int i=0;i<NUM_STAGING_BUFFERS-1;i++)
		std::swap(stagingBuffers[(NUM_STAGING_BUFFERS-1-i)],stagingBuffers[(NUM_STAGING_BUFFERS-2-i)]);
	lastContext->CopyResource(stagingBuffers[0],buffer);
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &deviceContext,void *data)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	if(lastContext)
	{
		HRESULT hr=lastContext->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
		if(hr==S_OK)
		{
			memcpy(mapped.pData,data,num_elements*element_bytesize);
			mapped.RowPitch=0;
			mapped.DepthPitch=0;
			lastContext->Unmap(buffer,0);
		}
		else
			SIMUL_BREAK_ONCE("Map failed");
	}
	else
		SIMUL_BREAK_ONCE("Uninitialized device context");
	mapped.pData=NULL;
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	if(!effect)
		return;
	if(!effect->asD3DX11Effect())
		return;
	if(!effect->asD3DX11Effect()->GetVariableByName(name))
		return;
	ID3DX11EffectShaderResourceVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsShaderResource();
		
	if(!var->IsValid())
	{
		SIMUL_CERR<<"Constant Buffer not found: "<<name<<", in effect "<<effect->filename.c_str()<<std::endl;
		if(effect->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(effect->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	var->SetResource(shaderResourceView);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	if (!effect->asD3DX11Effect())
		return;
	if (!effect->asD3DX11Effect()->GetVariableByName(name))
		return;
	ID3DX11EffectUnorderedAccessViewVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsUnorderedAccessView();
	if(!var->IsValid())
	{
		SIMUL_CERR<<"Constant Buffer not found: "<<name<<", in effect "<<effect->filename.c_str()<<std::endl;
		if(effect->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(effect->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	var->SetUnorderedAccessView(unorderedAccessView);
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{
}
void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(buffer);
	for(int i=0;i<NUM_STAGING_BUFFERS;i++)
		SAFE_RELEASE(stagingBuffers[i]);
	num_elements=0;
}

void dx11::PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r,size_t size,void *addr)
{
	InvalidateDeviceObjects();
	if(!r)
		return;
	SAFE_RELEASE(m_pD3D11Buffer);	
	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem			= addr;
	cb_init_data.SysMemPitch		= 0;
	cb_init_data.SysMemSlicePitch	= 0;
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage				= D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags			= 0;
	cb_desc.ByteWidth			= (UINT)(PAD16(size));
	cb_desc.StructureByteStride = 0;
	ID3D11Device *device=r->AsD3D11Device();
	device->CreateBuffer(&cb_desc,&cb_init_data, &m_pD3D11Buffer);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
}

//! Find the constant buffer in the given effect, and link to it.
void dx11::PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
	if(!effect)
		return;
	if(!effect->asD3DX11Effect())
		return;
	m_pD3DX11EffectConstantBuffer=effect->asD3DX11Effect()->GetConstantBufferByName(name);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
	else
		SIMUL_CERR<<"ConstantBuffer<> LinkToEffect did not find the buffer named "<<name<<" in the effect."<<std::endl;
}

void dx11::PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pD3D11Buffer);
	m_pD3DX11EffectConstantBuffer=NULL;
}

void dx11::PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr)
{
	if(!m_pD3D11Buffer)
	{
		SIMUL_CERR<<"Attempting to apply an uninitialized Constant Buffer"<<std::endl;
		return;
	}
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	
	D3D11_MAPPED_SUBRESOURCE mapped_res;
	if(pContext->Map(m_pD3D11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res)!=S_OK)
		return;
	memcpy(mapped_res.pData,addr,size);
	pContext->Unmap(m_pD3D11Buffer, 0);
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
}

void dx11::PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &deviceContext)
{
	if(m_pD3DX11EffectConstantBuffer)
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(NULL);
}

int EffectTechnique::NumPasses() const
{
	D3DX11_TECHNIQUE_DESC desc;
	ID3DX11EffectTechnique *tech=const_cast<EffectTechnique*>(this)->asD3DX11EffectTechnique();
	tech->GetDesc(&desc);
	return (int)desc.Passes;
}

dx11::Effect::Effect() :currentPass(NULL)
{
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx11::EffectTechnique;
}
#define D3DCOMPILE_DEBUG 1
void Effect::Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	if(!renderPlatform)
		return;
	// PREFER to use the platform shader:
	std::string filename_fx(filename_utf8);
	if(filename_fx.find(".")>=filename_fx.length())
		filename_fx+=".fx";
	int index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),renderPlatform->GetShaderPathsUtf8());

	if(index>=renderPlatform->GetShaderPathsUtf8().size())
	{
		filename_fx=(filename_utf8);
		if(filename_fx.find(".")>=filename_fx.length())
			filename_fx+=".sfx";
		index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),renderPlatform->GetShaderPathsUtf8());
	}
	if(index<0||index>=renderPlatform->GetShaderPathsUtf8().size())
		filenameInUseUtf8=filename_fx;
	else if(index<renderPlatform->GetShaderPathsUtf8().size())
		filenameInUseUtf8=(renderPlatform->GetShaderPathsUtf8()[index]+"/")+filename_fx;
	HRESULT hr = CreateEffect(renderPlatform->AsD3D11Device(), &e, filename_fx.c_str(), defines, D3DCOMPILE_OPTIMIZATION_LEVEL3, renderPlatform->GetShaderBuildMode()
		,renderPlatform->GetShaderPathsUtf8(),renderPlatform->GetShaderBinaryPath());//);D3DCOMPILE_DEBUG
	platform_effect	=e;
	groups.clear();
	if(e)
	{
		D3DX11_EFFECT_DESC desc;
		e->GetDesc(&desc);
		for(int i=0;i<(int)desc.Groups;i++)
		{
			ID3DX11EffectGroup *g=e->GetGroupByIndex(i);
			D3DX11_GROUP_DESC gdesc;
			g->GetDesc(&gdesc);
			crossplatform::EffectTechniqueGroup *G=new crossplatform::EffectTechniqueGroup;
			if(gdesc.Name)
				groups[gdesc.Name]=G;
			else
				groups[""]=G;// The ungrouped techniques!
			for(int j=0;j<(int)gdesc.Techniques;j++)
			{
				ID3DX11EffectTechnique *t	=g->GetTechniqueByIndex(j);
				D3DX11_TECHNIQUE_DESC tdesc;
				t->GetDesc(&tdesc);
				dx11::EffectTechnique *T	=new dx11::EffectTechnique;
				G->techniques[tdesc.Name]	=T;
				T->platform_technique		=t;
				G->techniques_by_index[j]	=T;
				for(int k=0;k<(int)tdesc.Passes;k++)
				{
					ID3DX11EffectPass *p=t->GetPassByIndex(k);
					D3DX11_PASS_DESC passDesc;
					D3DX11_PASS_SHADER_DESC shaderDesc;
					p->GetComputeShaderDesc(&shaderDesc);
				}
			}
		}
	}
}

dx11::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	platform_effect=e;
}

crossplatform::EffectTechnique *dx11::Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	crossplatform::EffectTechnique *tech=new dx11::EffectTechnique;
	ID3DX11EffectTechnique *t=e->GetTechniqueByName(name);
	if(!t->IsValid())
	{
		SIMUL_CERR<<"Invalid Effect technique "<<name<<" in effect "<<this->filename.c_str()<<std::endl;
		if(this->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(this->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	tech->platform_technique=t;
	techniques[name]=tech;
	if(!tech->platform_technique)
	{
		SIMUL_BREAK_ONCE("NULL technique");
	}
	return tech;
}

crossplatform::EffectTechnique *dx11::Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	ID3DX11EffectTechnique *t=e->GetTechniqueByIndex(index);
	if(!t)
		return NULL;
	D3DX11_TECHNIQUE_DESC desc;
	t->GetDesc(&desc);
	crossplatform::EffectTechnique *tech=NULL;
	if(techniques.find(desc.Name)!=techniques.end())
	{
		tech=techniques[desc.Name];
		techniques_by_index[index]=tech;
		return tech;;
	}
	tech=new dx11::EffectTechnique;
	tech->platform_technique=t;
	techniques[desc.Name]=tech;
	techniques_by_index[index]=tech;
	return tech;
}

void dx11::Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *t,int mip)
{
	crossplatform::ShaderResource shaderResource			=GetShaderResource(name);
	SetUnorderedAccessView(deviceContext,shaderResource,t,mip);
}

void dx11::Effect::SetUnorderedAccessView(crossplatform::DeviceContext &,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *t,int mip)
{
	ID3DX11EffectUnorderedAccessViewVariable *var=(ID3DX11EffectUnorderedAccessViewVariable*)(shaderResource.platform_shader_resource);
	if(!asD3DX11Effect())
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return;
	}
	if(!var)
	{
		SIMUL_CERR<<"Invalid Resource "<<std::endl;
		return;
	}
	if(t)
	{
		ID3D11UnorderedAccessView *uav=t->AsD3D11UnorderedAccessView(mip);
		if(!uav)
		{
			SIMUL_CERR<<"Unordered access view not found."<<std::endl;
		}
		var->SetUnorderedAccessView(uav);
	}
	else
		var->SetUnorderedAccessView(NULL);
}


void dx11::Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *t,int mip)
{
	if(!asD3DX11Effect())
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return;
	}
	ID3DX11Effect *e=asD3DX11Effect();
	if(!e)
		return;
	crossplatform::ShaderResource res			=GetShaderResource(name);
	SetTexture(deviceContext, res, t, mip);
}


void Effect::SetTexture(crossplatform::DeviceContext &,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *t,int mip)
{
	ID3DX11EffectShaderResourceVariable *var=(ID3DX11EffectShaderResourceVariable*)(shaderResource.platform_shader_resource);
	if(!var||!var->IsValid())
	{
		SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader texture ")).c_str());
	}
	if(t)
	{
		dx11::Texture *T=(dx11::Texture*)t;
		auto srv=T->AsD3D11ShaderResourceView(mip);
		var->SetResource(srv);
	}
	else
	{
		var->SetResource(NULL);
	}
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext,const char *name	,crossplatform::ConstantBufferBase *s)	
{
	if(!asD3DX11Effect())
		return;
	ID3DX11EffectConstantBuffer *pD3DX11EffectConstantBuffer=asD3DX11Effect()->GetConstantBufferByName(name);
	if(pD3DX11EffectConstantBuffer)
	{
		crossplatform::PlatformConstantBuffer *pcb=s->GetPlatformConstantBuffer();
		dx11::PlatformConstantBuffer *pcb11=(dx11::PlatformConstantBuffer *)pcb;
		pD3DX11EffectConstantBuffer->SetConstantBuffer(pcb11->asD3D11Buffer());
	}
}


crossplatform::ShaderResource Effect::GetShaderResource(const char *name)
{
	// First do a simple search by pointer.
	auto i=shaderResources.find(name);
	if(i!=shaderResources.end())
		return i->second;
	crossplatform::ShaderResource &res=shaderResources[name];
	res.platform_shader_resource=0;
	ID3DX11Effect *effect=asD3DX11Effect();
	if(!effect)
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return res;
	}
	ID3DX11EffectVariable *var=effect->GetVariableByName(name);
	if(!var->IsValid())
	{
		SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
		return res;
	}
	ID3DX11EffectShaderResourceVariable*	srv	=var->AsShaderResource();
	if(srv->IsValid())
		res.platform_shader_resource=(void*)srv;
	else
	{
		ID3DX11EffectUnorderedAccessViewVariable *uav=var->AsUnorderedAccessView();
		if(uav->IsValid())
		{
			res.platform_shader_resource=(void*)uav;
		}
		else
		{
			SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Unknown resource type ")+name).c_str());
			return res;
		}
	}
	return res;
}

void Effect::SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s)
{
	if(!asD3DX11Effect())
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return;
	}
	if (!s)
		return;
	ID3DX11EffectSamplerVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsSampler();
	var->SetSampler(0,s->asD3D11SamplerState());
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	if(apply_count!=0)
		SIMUL_BREAK_ONCE("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	if(!effectTechnique)
		return;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
	currentPass						=tech->GetPassByIndex(pass_num);
	HRESULT hr						=currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	V_CHECK_ONCE(hr);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	if(apply_count!=0)
		SIMUL_BREAK_ONCE("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	if(effectTechnique)
	{
		ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
		if (!tech->IsValid())
		{
			const char *techname="";//effectTechnique->getName();
			SIMUL_BREAK_ONCE(base::QuickFormat("Invalid technique %s of shader %s\n",techname,this->filename.c_str()));
			return;
		}
		if(!passname)
			currentPass = tech->GetPassByIndex(0);
		else
			currentPass = tech->GetPassByName(passname);
		if (!currentPass->IsValid())
		{
			const char *techname="";//effectTechnique->getName();
			SIMUL_BREAK_ONCE(base::QuickFormat("Invalid pass %s sent to Effect::Apply for technique %s of shader %s\n",passname,techname,this->filename.c_str()));
			D3DX11_TECHNIQUE_DESC desc;
			ID3DX11EffectTechnique *t=const_cast<ID3DX11EffectTechnique*>(tech);
			t->GetDesc(&desc);
			std::cerr<<"Passes are: ";
			for(int i=0;i<(int)desc.Passes;i++)
			{
				ID3DX11EffectPass *p=tech->GetPassByIndex(i);
				D3DX11_PASS_DESC pdesc;
				p->GetDesc(&pdesc);
				std::cerr<<pdesc.Name<<" ";
			}
			std::cerr<<std::endl;
		}
		else
		{
			HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
			V_CHECK(hr);
		}
	}
	else
	{
		SIMUL_BREAK_ONCE(base::QuickFormat("NULL technique sent to Effect::Apply for shader %s\n",this->filename.c_str()));
	}
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n",this->filename.c_str()));
	ID3DX11Effect *effect			=asD3DX11Effect();
	if(!effect)
		return;
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	if(currentPass)
		HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count<=0)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Unapply without a corresponding Apply! Effect: %s\n",this->filename.c_str()))
	else if(apply_count>1)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Apply has been called too many times! Effect: %s\n",this->filename.c_str()))
	apply_count--;
	if(currentPass)
		currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	currentTechnique=NULL;
	currentPass = NULL;
	//UnbindTextures(deviceContext);
}
void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	//if(apply_count!=1)
	//	SIMUL_BREAK_ONCE(base::QuickFormat("UnbindTextures can only be called after Apply and before Unapply! Effect: %s\n",this->filename.c_str()))
	ID3DX11Effect *effect			=asD3DX11Effect();

	D3DX11_EFFECT_DESC edesc;
	if(!effect)
		return;
	effect->GetDesc(&edesc);
	for(unsigned i=0;i<edesc.GlobalVariables;i++)
	{
		ID3DX11EffectVariable *var	=effect->GetVariableByIndex(i);
		D3DX11_EFFECT_VARIABLE_DESC desc;
		var->GetDesc(&desc);
		ID3DX11EffectType *s=var->GetType();
		//if(var->IsShaderResource())
		{
			ID3DX11EffectShaderResourceVariable*	srv	=var->AsShaderResource();
			if(srv->IsValid())
				srv->SetResource(NULL);
		}
		//if(var->IsUnorderedAccessView())
		{
			ID3DX11EffectUnorderedAccessViewVariable*	uav	=effect->GetVariableByIndex(i)->AsUnorderedAccessView();
			if(uav->IsValid())
				uav->SetUnorderedAccessView(NULL);
		}
	}
	if(apply_count!=1&&effect)
	{
		V_CHECK(effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0,deviceContext.asD3D11DeviceContext()));
	}
}