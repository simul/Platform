#include "Effect.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;

EffectTechnique::EffectTechnique()
	:platform_technique(NULL)
{
}

Effect::Effect()
	:platform_effect(NULL)
{
}

Effect::~Effect()
{
	std::cout<<"~Effect"<<std::endl;
}

