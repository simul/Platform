#include "Simul/Platform/CrossPlatform/Effect.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;

Effect::Effect()
	:platform_effect(NULL)
{
}

Effect::~Effect()
{
	//std::cout<</"~texture"<<std::endl;
}

