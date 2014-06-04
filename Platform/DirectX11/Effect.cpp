#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Utilities.h"

#include <string>

using namespace simul;
using namespace dx11;


Effect::Effect()
{
}

Effect::Effect(void *device,const char *filename,const std::map<std::string,std::string>&defines)
{
	ID3DX11Effect *d3deffect=NULL;
	CreateEffect((ID3D11Device*)device,&d3deffect,filename,defines);
	platform_effect=d3deffect;
}


Effect::~Effect()
{
	ID3DX11Effect *d3deffect=(ID3DX11Effect*)platform_effect;
	SAFE_RELEASE(d3deffect);
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	ID3DX11Effect *d3deffect=(ID3DX11Effect*)platform_effect;
	if(!d3deffect)
		return NULL;
	crossplatform::EffectTechnique *tech=NULL;
	if(techniques.find(name)!=techniques.end())
		tech=techniques[name];
	else
	{
		tech=new crossplatform::EffectTechnique;
		techniques[name]=tech;
	}
	tech->platform_technique=d3deffect->GetTechniqueByName(name);
	return tech;
}

void Effect::SetTexture(const char *name,void *tex)
{
	ID3DX11Effect *d3deffect=(ID3DX11Effect*)platform_effect;
	if(!d3deffect)
		return;
	dx11::setTexture(d3deffect,name,(ID3D11ShaderResourceView*)tex);
}