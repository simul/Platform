#include "Texture.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"

#include <string>

using namespace simul;
using namespace dx11;

dx11::Texture::Texture(ID3D11Device* d)
	:device(d)
	,texture(NULL)
	,shaderResourceView(NULL)
{
}

dx11::Texture::~Texture()
{
	InvalidateDeviceObjects();
}
void dx11::Texture::InvalidateDeviceObjects()
{
	SAFE_RELEASE(texture);
	SAFE_RELEASE(shaderResourceView);
}

// Load a texture file
void dx11::Texture::LoadFromFile(const char *pFilePathUtf8)
{
	InvalidateDeviceObjects();
	//shaderResourceView	=simul::dx11::LoadTexture(device,pFilePathUtf8);
}

bool dx11::Texture::IsValid() const
{
	return (shaderResourceView!=NULL);
}