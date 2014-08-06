#pragma once
#include <tchar.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/CubemapFramebuffer.h"
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/BaseLightningRenderer.h"
#include "Simul/Platform/DirectX11/Export.h"

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS LightningRenderer: public simul::clouds::BaseLightningRenderer
		{
		public:
			LightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk);
			~LightningRenderer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void RecompileShaders();
			void InvalidateDeviceObjects();
			void Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex,simul::sky::float4 depthViewportXYWH,crossplatform::Texture *cloud_depth_tex);
		protected:
			ID3D11Device *	m_pd3dDevice;
			ID3D11InputLayout* inputLayout;
			VertexBuffer<LightningVertex>				vertexBuffer;
			crossplatform::ConstantBuffer<LightningConstants>			lightningConstants;
			crossplatform::ConstantBuffer<LightningPerViewConstants>	lightningPerViewConstants;
			void Walk(const simul::clouds::LightningProperties &props
							,float time
							,const simul::math::Vector3 &cam_pos
							,int viewportWidth
							,LightningVertex *vertices
							 ,int &v
							,int level
							 ,int branchIndex
							,std::vector<int> &start
							,std::vector<int> &count
							,std::vector<bool> &thick
							 ,const simul::clouds::LightningRenderInterface *lightningRenderInterface
							,const simul::clouds::LightningRenderInterface::Branch *parentBranch=NULL);
		};
	}
}