#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Scene/Texture.h"
#include <string>
#include <map>
#include <d3d11.h>

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Texture:public simul::scene::Texture
		{
		public:
			Texture(ID3D11Device* d);
			~Texture();
			void InvalidateDeviceObjects();
			void LoadFromFile(const char *pFilePathUtf8);
			
			ID3D11Device*				device;
			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*   shaderResourceView;
		};
	}
}
