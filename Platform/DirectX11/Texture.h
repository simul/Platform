#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Texture:public simul::crossplatform::Texture
		{
		public:
			Texture(ID3D11Device* d=NULL);
			~Texture();
			void InvalidateDeviceObjects();
			void LoadFromFile(const char *pFilePathUtf8);
			bool IsValid() const;
			void *AsVoidPointer()
			{
				return shaderResourceView;
			}
			void InitFromExternalSRV(ID3D11ShaderResourceView *srv);
			
			ID3D11Device*				device;

			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*   shaderResourceView;
			ID3D11UnorderedAccessView*  unorderedAccessView;
			
			ID3D11UnorderedAccessView**  unorderedAccessViewMips;
			ID3D11RenderTargetView*		renderTargetView;
			ID3D11Resource*				stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			int width,length,depth;
			DXGI_FORMAT format;
			void copyToMemory(ID3D11Device *pd3dDevice,ID3D11DeviceContext *context,void *target,int start_texel=0,int texels=0);
			void setTexels(ID3D11DeviceContext *context,const void *src,int texel_index,int num_texels);
			void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			void ensureTexture3DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,int d,DXGI_FORMAT f,bool computable,int mips=1);
			void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,unsigned f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0);
			void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT f,bool computable=false);
			void map(ID3D11DeviceContext *context);
			bool isMapped() const;
			void unmap();
			void activateRenderTarget(crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			virtual int GetLength() const
			{
				return length;
			}
			virtual int GetWidth() const
			{
				return width;
			}
			int GetSampleCount() const;
		private:
			ID3D11DeviceContext *last_context;
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
		};
	}
}

namespace std
{
	template<> inline void swap(simul::dx11::Texture& _Left, simul::dx11::Texture& _Right)
	{
		std::swap(_Left.shaderResourceView	,_Right.shaderResourceView);
		std::swap(_Left.unorderedAccessView	,_Right.unorderedAccessView);
		std::swap(_Left.renderTargetView	,_Right.renderTargetView);
		std::swap(_Left.stagingBuffer		,_Right.stagingBuffer);
		std::swap(_Left.texture				,_Right.texture);
		std::swap(_Left.width				,_Right.width);
		std::swap(_Left.length				,_Right.length);
		std::swap(_Left.depth				,_Right.depth);
		std::swap(_Left.mapped				,_Right.mapped);
		std::swap(_Left.format				,_Right.format);
	}
}