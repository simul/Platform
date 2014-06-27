#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;

Effect::Effect()
	:platform_effect(NULL)
	,apply_count(0)
	,currentTechnique(NULL)
	,currentPass(0)
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
	return techniques[name];
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