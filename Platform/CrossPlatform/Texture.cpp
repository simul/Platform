#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Base/RuntimeError.h"
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
				,fence(0)
				,unfenceable(false)
				,width(0)
				,length(0)
				,depth(0)
				,arraySize(0)
				,dim(0)
				,mips(1)
				,pixelFormat(crossplatform::UNKNOWN)
				,renderPlatform(NULL)
{
	if(n)
		name=n;
}

Texture::~Texture()
{
}

void Texture::activateRenderTarget(DeviceContext &deviceContext,int array_index,int mip_index)
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
unsigned long long Texture::GetFence() const
{
	return fence;
}

void Texture::SetFence(unsigned long long f)
{
	if(fence!=0&&f!=fence)
	{
		// This is probably ok. We might be adding to a different part of a texture.
	//	SIMUL_CERR<<"Setting fence when the last one is not cleared yet\n"<<std::endl;
	}
	fence=f;
}


void Texture::ClearFence()
{
	fence=0;
}
