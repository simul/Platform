#include "Simul/Platform/CrossPlatform/Texture.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;
SamplerState::SamplerState():default_slot(-1)
{
}

SamplerState::~SamplerState()
{
}

Texture::Texture(const char *n):cubemap(false)
				,width(0)
				,length(0)
				,depth(0)
				,arraySize(0)
				,dim(0)
				,mips(1)
				,pixelFormat(crossplatform::UNKNOWN)
				,renderPlatform(NULL)
				,num_rt(0)
{
	if(n)
		name=n;
}

Texture::~Texture()
{
}


bool Texture::IsCubemap() const
{
	return cubemap;
}

void Texture::SetName(const char *n)
{
	name=n;
}