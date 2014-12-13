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

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	InvalidateDeviceObjects();
	ID3D11Device *m_pd3dDevice=r->AsD3D11Device();
	D3D11_QUERY_DESC qdesc=
	{
		toD3dQueryType(type),0
	};
	for(int i=0;i<QueryLatency;i++)
		m_pd3dDevice->CreateQuery(&qdesc,&d3d11Query[i]);
}
void Query::InvalidateDeviceObjects() 
{
	for(int i=0;i<QueryLatency;i++)
		SAFE_RELEASE(d3d11Query[i]);
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
	currFrame = (currFrame + 1) % QueryLatency;  
}

void Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	while (pContext->GetData(d3d11Query[currFrame],data,(UINT)sz,0) == S_FALSE);
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

	renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &buffer);

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
		lastContext->Map(buffer,0,D3D11_MAP_WRITE,0,&mapped);
	void *ptr=(void *)mapped.pData;
	return ptr;
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
			SIMUL_BREAK("Map failed");
	}
	else
		SIMUL_BREAK("Uninitialized device context");
	mapped.pData=NULL;
}

void PlatformStructuredBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)
{
	ID3DX11EffectShaderResourceVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsShaderResource();
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
//	memset(&mapped,0,sizeof(mapped));
	ID3DX11EffectShaderResourceVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsShaderResource();
	var->SetResource(shaderResourceView);
}
void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	//memset(&mapped,0,sizeof(mapped));
	ID3DX11EffectUnorderedAccessViewVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsUnorderedAccessView();
	var->SetUnorderedAccessView(unorderedAccessView);
}
void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{
}
void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(buffer);
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
		std::cerr<<"ConstantBuffer<> LinkToEffect did not find the buffer named "<<name<<" in the effect."<<std::endl;
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

dx11::Effect::Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	Load(renderPlatform,filename_utf8,defines);
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx11::EffectTechnique;
}
#define D3DCOMPILE_DEBUG 1
void dx11::Effect::Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	if(!renderPlatform)
		return;
	filenameInUseUtf8=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,dx11::GetShaderPathsUtf8());
	HRESULT hr		=CreateEffect(renderPlatform->AsD3D11Device(),&e,filename_utf8,defines,0);//D3DCOMPILE_OPTIMIZATION_LEVEL3);D3DCOMPILE_DEBUG
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
		SIMUL_BREAK("NULL technique");
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

void dx11::Effect::SetUnorderedAccessView(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *t)
{
	if(t)
	{
		dx11::Texture *T=(dx11::Texture*)t;
		simul::dx11::setUnorderedAccessView(asD3DX11Effect(),name,T->AsD3D11UnorderedAccessView());
	}
	else
		simul::dx11::setUnorderedAccessView(asD3DX11Effect(),name,NULL);
}

void dx11::Effect::SetTexture(const char *name,ID3D11ShaderResourceView *tex)
{
	simul::dx11::setTexture(asD3DX11Effect(),name,tex);
}

void dx11::Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture &t)
{
	dx11::Texture *T=(dx11::Texture*)&t;
	simul::dx11::setTexture(asD3DX11Effect(),name,T->AsD3D11ShaderResourceView());
}

void dx11::Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *t)
{
	if(t)
	{
		dx11::Texture *T=(dx11::Texture*)t;
		simul::dx11::setTexture(asD3DX11Effect(),name,T->AsD3D11ShaderResourceView());
	}
	else
		simul::dx11::setTexture(asD3DX11Effect(),name,NULL);
}

void Effect::SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s)
{
	ID3DX11EffectSamplerVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsSampler();
	var->SetSampler(0,s->asD3D11SamplerState());
}

void Effect::SetParameter(const char *name	,float value)
{
	ID3DX11EffectScalarVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloat(value);
}

void Effect::SetParameter	(const char *name	,vec2 value)	
{
	ID3DX11EffectVectorVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void Effect::SetParameter	(const char *name	,vec3 value)	
{
	ID3DX11EffectVectorVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void Effect::SetParameter	(const char *name	,vec4 value)	
{
	ID3DX11EffectVectorVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void Effect::SetParameter	(const char *name	,int value)	
{
	ID3DX11EffectScalarVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetInt(value);
}

void Effect::SetParameter	(const char *name	,int2 value)	
{
	ID3DX11EffectVectorVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetIntVector((const int *)&value);
}

void Effect::SetVector		(const char *name	,const float *value)	
{
	ID3DX11EffectVectorVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void Effect::SetMatrix		(const char *name	,const float *m)	
{
	ID3DX11EffectMatrixVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsMatrix();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetMatrix(m);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
	currentPass						=tech->GetPassByIndex(pass_num);
	HRESULT hr						= currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	V_CHECK(hr);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	if(effectTechnique)
	{
		ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
		if (!tech->IsValid())
		{
			const char *techname="";//effectTechnique->getName();
			SIMUL_BREAK(base::QuickFormat("Invalid technique %s of shader %s\n",techname,this->filename.c_str()));
			return;
		}
		currentPass = tech->GetPassByName(passname);
		if (!currentPass->IsValid())
		{
			const char *techname="";//effectTechnique->getName();
			SIMUL_BREAK(base::QuickFormat("Invalid pass %s sent to Effect::Apply for technique %s of shader %s\n",passname,techname,this->filename.c_str()));
		}
		else
		{
			HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
			V_CHECK(hr);
		}
	}
	else
	{
		SIMUL_BREAK(base::QuickFormat("NULL technique sent to Effect::Apply for shader %s\n",this->filename.c_str()));
	}
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n",this->filename.c_str()));
	ID3DX11Effect *effect			=asD3DX11Effect();
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count<=0)
		SIMUL_BREAK(base::QuickFormat("Effect::Unapply without a corresponding Apply! Effect: %s\n",this->filename.c_str()))
	else if(apply_count>1)
		SIMUL_BREAK(base::QuickFormat("Effect::Apply has been called too many times! Effect: %s\n",this->filename.c_str()))
	apply_count--;
	ID3DX11Effect *effect			=asD3DX11Effect();
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	currentTechnique=NULL;
	currentPass = NULL;
	//UnbindTextures(deviceContext);
}
void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	//if(apply_count!=1)
	//	SIMUL_BREAK(base::QuickFormat("UnbindTextures can only be called after Apply and before Unapply! Effect: %s\n",this->filename.c_str()))
	ID3DX11Effect *effect			=asD3DX11Effect();
	dx11::unbindTextures(effect);
	if(apply_count!=1)
	{
		V_CHECK(effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0,deviceContext.asD3D11DeviceContext()));
	}
}