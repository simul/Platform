#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"

#include <string>

using namespace simul;
using namespace dx11;

dx11::Effect::Effect(void *device,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=NULL;
	HRESULT hr=CreateEffect((ID3D11Device *)device,&e,filename_utf8,defines,D3DCOMPILE_OPTIMIZATION_LEVEL3);
	platform_effect=e;
}

dx11::Effect::~Effect()
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	for(TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
	{
		delete i->second;
	}
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
	crossplatform::EffectTechnique *tech=new crossplatform::EffectTechnique;
	tech->platform_technique=e->GetTechniqueByName(name);
	techniques[name]=tech;
	return tech;
}

void dx11::Effect::SetTexture(const char *name,void *tex)
{
	simul::dx11::setTexture(asD3DX11Effect(),name,(ID3D11ShaderResourceView *)tex);
}

void dx11::Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
	dx11::Texture *T=(dx11::Texture*)&t;
	simul::dx11::setTexture(asD3DX11Effect(),name,T->shaderResourceView);
}