
#include "Utilities.h"
#include "MacrosDX1x.h"
#include "Simul\Base\StringToWString.h"
#include "Simul/Sky/Float4.h"
#include <d3dx11.h>
using namespace simul;
using namespace dx11;
static ID3D1xDevice		*m_pd3dDevice		=NULL;

ComputableTexture::ComputableTexture()
	:g_pTex_Output(NULL)
	,g_pUAV_Output(NULL)
	,g_pSRV_Output(NULL)
{
}

ComputableTexture::~ComputableTexture()
{
}

void ComputableTexture::release()
{
	SAFE_RELEASE(g_pTex_Output);
	SAFE_RELEASE(g_pUAV_Output);
	SAFE_RELEASE(g_pSRV_Output);
}

void ComputableTexture::init(ID3D11Device *pd3dDevice,int w,int h)
{
	release();
    D3D11_TEXTURE2D_DESC tex_desc;
	ZeroMemory(&tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	tex_desc.ArraySize			= 1;
    tex_desc.BindFlags			= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.Usage				= D3D11_USAGE_DEFAULT;
    tex_desc.Width				= w;
    tex_desc.Height				= h;
    tex_desc.MipLevels			= 1;
    tex_desc.SampleDesc.Count	= 1;
	tex_desc.Format				= DXGI_FORMAT_R32_UINT;
    pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Output);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uav_desc.Format				= tex_desc.Format;
	uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice	= 0;
	pd3dDevice->CreateUnorderedAccessView(g_pTex_Output, &uav_desc, &g_pUAV_Output);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    srv_desc.Format						= tex_desc.Format;
    srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels		= 1;
    srv_desc.Texture2D.MostDetailedMip	= 0;
    pd3dDevice->CreateShaderResourceView(g_pTex_Output, &srv_desc, &g_pSRV_Output);
}

int UtilityRenderer::instance_count=0;
int UtilityRenderer::screen_width=0;
int UtilityRenderer::screen_height=0;
D3DXMATRIX UtilityRenderer::view,UtilityRenderer::proj;
ID3D1xEffect *UtilityRenderer::m_pDebugEffect=NULL;
ID3D11InputLayout *UtilityRenderer::m_pBufferVertexDecl=NULL;
ID3D1xBuffer* UtilityRenderer::m_pVertexBuffer=NULL;
UtilityRenderer utilityRenderer;

UtilityRenderer::UtilityRenderer()
{
	instance_count++;
}

UtilityRenderer::~UtilityRenderer()
{
	InvalidateDeviceObjects();
}

struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};


void UtilityRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice *)dev;
	RecompileShaders();

	D3D1x_BUFFER_DESC bdesc=
	{
        4*sizeof(Vertext),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_VERTEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
	};
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData,sizeof(D3D1x_SUBRESOURCE_DATA));
//   InitData.pSysMem			=vertices;
   // InitData.SysMemPitch		=sizeof(Vertext);
    InitData.SysMemSlicePitch	=0;
	SAFE_RELEASE(m_pVertexBuffer);
	
	m_pd3dDevice->CreateBuffer(&bdesc,NULL,&m_pVertexBuffer);
}

void UtilityRenderer::RecompileShaders()
{
	SAFE_RELEASE(m_pDebugEffect);
	CreateEffect(m_pd3dDevice,&m_pDebugEffect,_T("simul_debug.fx"));
}

void UtilityRenderer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pDebugEffect);
}

void UtilityRenderer::SetMatrices(D3DXMATRIX v,D3DXMATRIX p)
{
	view=v;
	proj=p;
}

void UtilityRenderer::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void UtilityRenderer::PrintAt3dPos(ID3D11DeviceContext* pd3dImmediateContext,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
}

void UtilityRenderer::DrawLines(ID3D11DeviceContext* m_pImmediateContext,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	PIXWrapper(0xFF0000FF,"DrawLines")
	{
	HRESULT hr=S_OK;
	D3DXMATRIX world, tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	ID3D1xEffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("simul_direct");
	ID3D1xEffectMatrixVariable*	worldViewProj=m_pDebugEffect->GetVariableByName("worldViewProj")->AsMatrix();

	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
	
	ID3D1xBuffer *					vertexBuffer=NULL;
#if 1
	// Create the vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        vertex_count*sizeof(VertexXyzRgba),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(VertexXyzRgba);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);

	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=tech->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);

	ID3D1xInputLayout*				m_pVtxDecl=NULL;
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	ID3D11InputLayout* previousInputLayout;
	m_pImmediateContext->IAGetInputLayout( &previousInputLayout );
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(strip?D3D1x_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D1x_PRIMITIVE_TOPOLOGY_LINELIST);
	UINT stride = sizeof(VertexXyzRgba);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = stride;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
												1,				// the number of buffers in the array
												&vertexBuffer,	// the array of vertex buffers
												&stride,		// array of stride values, one for each buffer
												&offset);		// array of 
	hr=ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(vertex_count,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(m_pVtxDecl);
#endif
	}
}

void UtilityRenderer::RenderTexture(ID3D11DeviceContext *m_pImmediateContext,int x1,int y1,int dx,int dy,ID3D1xEffectTechnique* tech)
{
	RenderTexture(m_pImmediateContext,(float)x1,(float)y1,(float)dx,(float)dy,tech);
}

void UtilityRenderer::RenderTexture(ID3D11DeviceContext *m_pImmediateContext,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech)
{
	HRESULT hr=S_OK;
	if(m_pBufferVertexDecl==NULL)
	{
		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	16,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
		};
		D3DX11_PASS_DESC PassDesc;
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);

		hr=m_pd3dDevice->CreateInputLayout(decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pBufferVertexDecl);
	}
	Vertext vertices[4]=
	{
		D3DXVECTOR4(x1		,y1		,0.f,	1.f), D3DXVECTOR2(0.f,0.f),
		D3DXVECTOR4(x1+dx	,y1		,0.f,	1.f), D3DXVECTOR2(1.f,0.f),
		D3DXVECTOR4(x1		,y1+dy	,0.f,	1.f), D3DXVECTOR2(0.f,1.f),
		D3DXVECTOR4(x1+dx	,y1+dy	,0.f,	1.f), D3DXVECTOR2(1.f,1.f),
	};
    D3D11_MAPPED_SUBRESOURCE mapped;
	MapBuffer(m_pImmediateContext,m_pVertexBuffer,&mapped);
	memcpy(mapped.pData,vertices,4*sizeof(Vertext));
	UnmapBuffer(m_pImmediateContext,m_pVertexBuffer);

	UINT stride	=sizeof(Vertext);
	UINT offset	=0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0]	=0;
    Offsets[0]	=0;
	m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
												1,					// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset);			// array of offset values, one for each buffer
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ID3D11InputLayout* previousInputLayout;
	m_pImmediateContext->IAGetInputLayout(&previousInputLayout );
	m_pImmediateContext->IASetInputLayout(m_pBufferVertexDecl);
	ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(4,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}