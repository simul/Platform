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
	for(crossplatform::TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
	{
		delete i->second;
	}
	techniques.clear();
	SIMUL_ASSERT(apply_count==0);
}

EffectTechnique *EffectTechniqueGroup::GetTechniqueByName(const char *name)
{
	TechniqueMap::iterator i=techniques.find(name);
	if(i==techniques.end())
		return NULL;
	return i->second;
}

EffectTechnique *EffectTechniqueGroup::GetTechniqueByIndex(int index)
{
	return techniques_by_index[index];
}

EffectTechniqueGroup *Effect::GetTechniqueGroupByName(const char *name)
{
	return groups[name];
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
	if(groupname.size()>0)
	{
		if(groups.find(groupname)==groups.end())
		{
			groups[groupname]=new crossplatform::EffectTechniqueGroup;
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
		}
		techname=(groupname+"::")+techname;
	}
	if(techniques.find(techname)!=techniques.end())
	{
		if(!tech)
			tech=techniques[techname];
		else
			techniques[techname]=tech;
	}
	else
	{
		if(!tech)
			tech					=CreateTechnique(); 
		techniques[techname]		=tech;
		int index					=(int)techniques_by_index.size();
		techniques_by_index[index]	=tech;
	}
	return tech;
}

const char *Effect::GetTechniqueName(const EffectTechnique *t) const
{
	for(crossplatform::TechniqueMap::const_iterator i=techniques.begin();i!=techniques.end();i++)
	{
		if(i->second==t)
			return i->first.c_str();
	}
	return "";
}

void Effect::StoreConstantBufferLink(crossplatform::ConstantBufferBase *b)
{
	linkedConstantBuffers.insert(b);
}

bool Effect::IsLinkedToConstantBuffer(crossplatform::ConstantBufferBase*b) const
{
	return (linkedConstantBuffers.find(b)!=linkedConstantBuffers.end());
}
