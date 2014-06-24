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
