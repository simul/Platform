#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;
using namespace std;

bool EffectPass::usesTextureSlot(int s) const
{
	if(s>=1000)
		return usesRwTextureSlot(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlots&m)!=0;
}

bool EffectPass::usesTextureSlotForSB(int s) const
{
	if(s>=1000)
		return usesRwTextureSlotForSB(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlotsForSB&m)!=0;
}

bool EffectPass::usesRwTextureSlotForSB(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlotsForSB&m)!=0;
}

bool EffectPass::usesBufferSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (bufferSlots&m)!=0;
}

bool EffectPass::usesSamplerSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return true;//(samplerSlots&m)!=0;
}

bool EffectPass::usesRwTextureSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlots&m)!=0;
}


bool EffectPass::usesTextures() const
{
	return (textureSlots+rwTextureSlots)!=0;
}

bool EffectPass::usesSBs() const
{
	return (textureSlotsForSB+rwTextureSlotsForSB)!=0;
}

bool EffectPass::usesBuffers() const
{
	return bufferSlots!=0;
}

bool EffectPass::usesSamplers() const
{
	return samplerSlots!=0;
}


void EffectPass::SetUsesBufferSlots(unsigned s)
{
	bufferSlots|=s;
}

void EffectPass::SetUsesTextureSlots(unsigned s)
{
	textureSlots|=s;
}

void EffectPass::SetUsesTextureSlotsForSB(unsigned s)
{
	textureSlotsForSB|=s;
}

void EffectPass::SetUsesRwTextureSlots(unsigned s)
{
	rwTextureSlots|=s;
}

void EffectPass::SetUsesRwTextureSlotsForSB(unsigned s)
{
	rwTextureSlotsForSB|=s;
}

void EffectPass::SetUsesSamplerSlots(unsigned s)
{
	samplerSlots|=s;
}

EffectTechniqueGroup::~EffectTechniqueGroup()
{
	for (crossplatform::TechniqueMap::iterator i = techniques.begin(); i != techniques.end(); i++)
	{
		delete i->second;
	}
	techniques.clear();
	charMap.clear();
}

Effect::Effect()
	:renderPlatform(NULL)
	,apply_count(0)
	,currentPass(0)
	,currentTechnique(NULL)
	,platform_effect(NULL)
{
}

Effect::~Effect()
{
	InvalidateDeviceObjects();
	SIMUL_ASSERT(apply_count==0);
}

void Effect::InvalidateDeviceObjects()
{
	shaderResources.clear();
	for (auto i = groups.begin(); i != groups.end(); i++)
	{
		delete i->second;
	}
	groups.clear();
	groupCharMap.clear();
}

EffectTechnique::EffectTechnique()
	:platform_technique(NULL)
{
}

EffectTechnique::~EffectTechnique()
{
}

EffectTechnique *EffectTechniqueGroup::GetTechniqueByName(const char *name)
{
	TechniqueCharMap::iterator i=charMap.find(name);
	if(i!=charMap.end())
		return i->second;
	TechniqueMap::iterator j=techniques.find(name);
	if(j==techniques.end())
		return NULL;
	charMap[name]=j->second;
	return j->second;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	return groupCharMap[0]->GetTechniqueByName(name);
}


EffectTechnique *EffectTechniqueGroup::GetTechniqueByIndex(int index)
{
	return techniques_by_index[index];
}

EffectTechniqueGroup *Effect::GetTechniqueGroupByName(const char *name)
{
	auto i=groupCharMap.find(name);
	if(i!=groupCharMap.end())
		return i->second;
	auto j=groups.find(name);
	if(j!=groups.end())
		return j->second;
	return nullptr;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	return o;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	return o;
}
EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2,const char *option3)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	o.options.push_back(std::string(option3));
	return o;
}

EffectTechnique *Effect::EnsureTechniqueExists(const string &groupname,const string &techname_,const string &passname)
{
	EffectTechnique *tech=NULL;
	string techname=techname_;
	{
		if(groups.find(groupname)==groups.end())
		{
			groups[groupname]=new crossplatform::EffectTechniqueGroup;
			if(groupname.length()==0)
				groupCharMap[nullptr]=groups[groupname];
		}
		crossplatform::EffectTechniqueGroup *group=groups[groupname];
		if(group->techniques.find(techname)!=group->techniques.end())
			tech=group->techniques[techname];
		else
		{
			tech								=CreateTechnique(); 
			int index							=(int)group->techniques.size();
			group->techniques[techname]			=tech;
			group->techniques_by_index[index]	=tech;
			if(groupname.size())
				techname=(groupname+"::")+techname;
			techniques[techname]			=tech;
			techniques_by_index[index]		=tech;
			tech->name						=techname;
		}
	}
	return tech;
}

const char *Effect::GetTechniqueName(const EffectTechnique *t) const
{
	for(auto g=groups.begin();g!=groups.end();g++)
	for(crossplatform::TechniqueMap::const_iterator i=g->second->techniques.begin();i!=g->second->techniques.end();i++)
	{
		if(i->second==t)
			return i->first.c_str();
	}
	return "";
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,const char *pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,int pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}

void Effect::StoreConstantBufferLink(crossplatform::ConstantBufferBase *b)
{
	linkedConstantBuffers.insert(b);
}

bool Effect::IsLinkedToConstantBuffer(crossplatform::ConstantBufferBase*b) const
{
	return (linkedConstantBuffers.find(b)!=linkedConstantBuffers.end());
}
