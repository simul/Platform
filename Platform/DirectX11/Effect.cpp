#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"

#include <string>

using namespace simul;
using namespace dx11;

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
	memset(&mapped,0,sizeof(mapped));
	ID3DX11EffectShaderResourceVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsShaderResource();
	var->SetResource(shaderResourceView);
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

dx11::Effect::Effect()
{
}

dx11::Effect::Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	Load(renderPlatform,filename_utf8,defines);
}

void dx11::Effect::Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	if(!renderPlatform)
		return;
	HRESULT hr		=CreateEffect(renderPlatform->AsD3D11Device(),&e,filename_utf8,defines,0);//D3DCOMPILE_OPTIMIZATION_LEVEL3);
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
	tech->platform_technique=e->GetTechniqueByName(name);
	techniques[name]=tech;
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
		simul::dx11::setUnorderedAccessView(asD3DX11Effect(),name,T->unorderedAccessView);
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
	simul::dx11::setTexture(asD3DX11Effect(),name,T->shaderResourceView);
}


void dx11::Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *t)
{
	if(t)
	{
		dx11::Texture *T=(dx11::Texture*)t;
		simul::dx11::setTexture(asD3DX11Effect(),name,T->shaderResourceView);
	}
	else
		simul::dx11::setTexture(asD3DX11Effect(),name,NULL);
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
	ID3DX11EffectPass *pass			=tech->GetPassByIndex(pass_num);
	HRESULT hr=pass->Apply(0,deviceContext.asD3D11DeviceContext());
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
		ID3DX11EffectPass *pass			=tech->GetPassByName(passname);
		HRESULT hr=pass->Apply(0,deviceContext.asD3D11DeviceContext());
		V_CHECK(hr);
	}
	else
		SIMUL_BREAK("Technique not found!")
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK("Effect::Reapply can only be called after Apply and before Unapply!")
	ID3DX11Effect *effect			=asD3DX11Effect();
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	ID3DX11EffectPass *pass			=tech->GetPassByIndex(0);
	HRESULT hr=pass->Apply(0,deviceContext.asD3D11DeviceContext());
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	apply_count--;
	ID3DX11Effect *effect			=asD3DX11Effect();
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	ID3DX11EffectPass *pass			=tech->GetPassByIndex(0);
	HRESULT hr=pass->Apply(0,deviceContext.asD3D11DeviceContext());
	currentTechnique=NULL;
}
void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK("UnbindTextures can only be called after Apply and before Unapply!")
	ID3DX11Effect *effect			=asD3DX11Effect();
	dx11::unbindTextures(effect);
}