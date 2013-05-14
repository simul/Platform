#pragma once
#include <d3d11.h>
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"

namespace simul
{
	namespace dx11
	{
		struct ComputableTexture
		{
			ComputableTexture();
			~ComputableTexture();

			ID3D11Texture2D*            g_pTex_Output;
			ID3D11UnorderedAccessView*  g_pUAV_Output;
			ID3D11ShaderResourceView*   g_pSRV_Output;

			void release();
			void init(ID3D11Device *pd3dDevice,int w,int h);
		};
		class UtilityRenderer
		{
			static int instance_count;
			static int screen_width;
			static int screen_height;
			static D3DXMATRIX view;
			static D3DXMATRIX proj;
			static ID3D1xEffect *m_pDebugEffect;
			static ID3D11InputLayout *m_pBufferVertexDecl;
			static ID3D1xBuffer* m_pVertexBuffer;
		public:
			UtilityRenderer();
			~UtilityRenderer();
			static void SetMatrices(D3DXMATRIX v,D3DXMATRIX p);
			static void RestoreDeviceObjects(void *m_pd3dDevice);
			static void RecompileShaders();
			static void SetScreenSize(int w,int h);
			static void InvalidateDeviceObjects();
			static void PrintAt3dPos(ID3D11DeviceContext* pd3dImmediateContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			static void DrawLines(ID3D11DeviceContext* pd3dImmediateContext,VertexXyzRgba *lines,int vertex_count,bool strip);
			static void RenderAngledQuad(ID3D1xDevice *m_pd3dDevice,ID3D11DeviceContext *context,const float *dir,bool y_vertical,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj,D3DXVECTOR3 sun_dir);
			static void RenderTexture(ID3D11DeviceContext *m_pImmediateContext,int x1,int y1,int dx,int dy,ID3D1xEffectTechnique* tech);
			static void RenderTexture(ID3D11DeviceContext *m_pImmediateContext,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech);
		};
	}
}