#define NOMINMAX
#include "Utilities.h"
#include "MacrosDX1x.h"
#include "Simul\Base\StringToWString.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Math/Vector3.h"
#if WINVER<0x602
#include <d3dx11.h>
#endif
#include <algorithm>			// for std::min / max
#include "D3dx11effect.h"
using namespace simul;
using namespace dx11;

// Stored states
static ID3D11DepthStencilState* m_pDepthStencilStateStored11=NULL;
static ID3D11RasterizerState* m_pRasterizerStateStored11=NULL;
static ID3D11BlendState* m_pBlendStateStored11=NULL;
static ID3D11SamplerState* m_pSamplerStateStored11=NULL;
static UINT m_StencilRefStored11;
static float m_BlendFactorStored11[4];
static UINT m_SampleMaskStored11;

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

/*
create the associated Texture2D resource as a Texture2D array resource, and then create a shader resource view for that resource.

1. Load all of the textures using D3DX10CreateTextureFromFile, with a D3DX10_IMAGE_LOAD_INFO specifying that you want D3D10_USAGE_STAGING.
		all of your textures need to be the same size, have the same format, and have the same number of mip levels
2. Map every mip level of every texture
3. Set up an array of D3D10_SUBRESOURCE_DATA's with number of elements == number of textures * number of mip levels. 
4. For each texture, set the pSysMem member of a D3D10_SUBRESOURCE_DATA to the pointer you got when you mapped each mip level of each texture. Make sure you also set the SysMemPitch to the pitch you got when you mapped that mip level
5. Call CreateTexture2D with the desired array size, and pass the array of D3D10_SUBRESOURCE_DATA's
6. Create a shader resource view for the texture 
*/
void ArrayTexture::create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files)
{
	release();
	std::vector<ID3D11Texture2D *> textures;
	for(unsigned i=0;i<texture_files.size();i++)
	{
		textures.push_back(simul::dx11::LoadStagingTexture(pd3dDevice,texture_files[i].c_str()));
	}
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[textures.size()];
	ID3D11DeviceContext *pContext=NULL;
	pd3dDevice->GetImmediateContext(&pContext);
	for(int i=0;i<(int)textures.size();i++)
	{
		if(!textures[i])
			return;
		textures[i]->GetDesc(&desc);
	/*	D3D11_MAPPED_SUBRESOURCE mapped_res;
		HRESULT hr=pContext->Map(textures[i],0,D3D11_MAP_READ,0,&mapped_res);	
		if(hr==S_OK)
		{
		subResources[i].pSysMem			=mapped_res.pData;
		subResources[i].SysMemPitch		=mapped_res.RowPitch;
		subResources[i].SysMemSlicePitch=mapped_res.DepthPitch;
		}*/
	}
	static int num_mips=5;
	desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
	desc.Usage=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags=0;
	desc.ArraySize=(UINT)textures.size();
	desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels=num_mips;
	pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture);

	if(m_pArrayTexture)
	for(unsigned i=0;i<textures.size();i++)
	{
		// Copy the resource directly, no CPU mapping
		pContext->CopySubresourceRegion(
						m_pArrayTexture
						,i*num_mips
						,0
						,0
						,0
						,textures[i]
						,0
						,NULL
						);
		//pContext->UpdateSubresource(m_pArrayTexture,i*num_mips, NULL,subResources[i].pSysMem,subResources[i].SysMemPitch,subResources[i].SysMemSlicePitch);
	}
	pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV);
	//delete [] subResources;
	for(unsigned i=0;i<textures.size();i++)
	{
	//	pContext->Unmap(textures[i],0);
		SAFE_RELEASE(textures[i])
	}
	pContext->GenerateMips(m_pArrayTexture_SRV);
	SAFE_RELEASE(pContext)
}

void ArrayTexture::create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable)
{
	release();
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[num];
	//ID3D11DeviceContext *pContext=NULL;
	//pd3dDevice->GetImmediateContext(&pContext);
	static int num_mips		=5;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=f;
	desc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET ;
	desc.Usage				=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags		=0;
	desc.ArraySize			=num;
	desc.MiscFlags			=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels			=num_mips;
	desc.SampleDesc.Count	=1;
	desc.SampleDesc.Quality	=0;
	V_CHECK(pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture));
	V_CHECK(pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV));
	V_CHECK(pd3dDevice->CreateUnorderedAccessView(m_pArrayTexture,NULL,&unorderedAccessView));
	//SAFE_RELEASE(pContext)
}

int UtilityRenderer::instance_count=0;
int UtilityRenderer::screen_width=0;
int UtilityRenderer::screen_height=0;
crossplatform::Effect *UtilityRenderer::m_pDebugEffect=NULL;
ID3D11InputLayout *UtilityRenderer::m_pCubemapVtxDecl=NULL;
ID3D1xBuffer* UtilityRenderer::m_pVertexBuffer=NULL;
crossplatform::RenderPlatform* UtilityRenderer::renderPlatform=NULL;
UtilityRenderer utilityRenderer;

UtilityRenderer::UtilityRenderer()
{
	instance_count++;
}

UtilityRenderer::~UtilityRenderer()
{
	// Now calling this manually instead to avoid global destruction when memory has already been freed.
	//InvalidateDeviceObjects();
}

struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};
struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

void UtilityRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform	*r)
{
	renderPlatform=r;
	RecompileShaders();
	SAFE_RELEASE(m_pVertexBuffer);
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		crossplatform::EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("vec3_input_signature");
		tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		SAFE_RELEASE(m_pCubemapVtxDecl);
		V_CHECK(renderPlatform->AsD3D11Device()->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pCubemapVtxDecl));
	}
	D3D11_BUFFER_DESC desc=
	{
        36*sizeof(vec3),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem		=vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(renderPlatform->AsD3D11Device()->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));
}

void UtilityRenderer::RecompileShaders()
{
	if(!renderPlatform)
		return;
	delete m_pDebugEffect;
	std::map<std::string,std::string> defines;
	m_pDebugEffect=new dx11::Effect(renderPlatform,"simul_debug.fx",defines);
}

void UtilityRenderer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pCubemapVtxDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	delete m_pDebugEffect;
	m_pDebugEffect=NULL;
    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

crossplatform::Effect		*UtilityRenderer::GetDebugEffect()
{
	return m_pDebugEffect;
}

void UtilityRenderer::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void UtilityRenderer::GetScreenSize(int& w,int& h)
{
	w=screen_width;
	h=screen_height;
}


void UtilityRenderer::PrintAt3dPos(ID3D11DeviceContext* pd3dImmediateContext,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
}

#include "Simul/Platform/CrossPlatform/DeviceContext.h"
void UtilityRenderer::DrawLines(crossplatform::DeviceContext &deviceContext,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		crossplatform::EffectTechnique *tech			=m_pDebugEffect->GetTechniqueByName("simul_debug");
		ID3D1xEffectMatrixVariable*	worldViewProj		=m_pDebugEffect->asD3DX11Effect()->GetVariableByName("worldViewProj")->AsMatrix();

		D3DXMATRIX wvp;
		camera::MakeWorldViewProjMatrix((float*)&wvp,(const float*)&world,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		worldViewProj->SetMatrix(&wvp._11);
	
		ID3D1xBuffer *					vertexBuffer=NULL;
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
		hr=renderPlatform->AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D1x_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3D1x_PASS_DESC PassDesc;
		ID3D1xEffectPass *pass=tech->asD3DX11EffectTechnique()->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=renderPlatform->AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		pContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,				// the first input slot for binding
													1,				// the number of buffers in the array
													&vertexBuffer,	// the array of vertex buffers
													&stride,		// array of stride values, one for each buffer
													&offset);		// array of 
		hr=ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}
void UtilityRenderer::Draw2dLines(crossplatform::DeviceContext &deviceContext,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		ID3DX11EffectTechnique *tech			=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("lines_2d");
		
		unsigned int num_v=1;
		D3D11_VIEWPORT								viewport;
		pContext->RSGetViewports(&num_v,&viewport);
		dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"rect",vec4(-1.0,-1.0,2.0f/viewport.Width,2.0f/viewport.Height));

		ID3D1xBuffer *					vertexBuffer=NULL;
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
		hr=renderPlatform->AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D1x_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=renderPlatform->AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		pContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,				// the first input slot for binding
										1,				// the number of buffers in the array
										&vertexBuffer,	// the array of vertex buffers
										&stride,		// array of stride values, one for each buffer
										&offset);		// array of 
		hr=ApplyPass(pContext,tech->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}


void UtilityRenderer::DrawQuad(ID3D11DeviceContext *m_pContext)
{
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	m_pContext->IAGetPrimitiveTopology(&previousTopology);
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(NULL);
	m_pContext->Draw(4,0);
	m_pContext->IASetPrimitiveTopology(previousTopology);
}			

void UtilityRenderer::DrawQuad2(ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3DX11Effect* eff,ID3DX11EffectTechnique* tech)
{
	DrawQuad2(pContext
		,2.f*(float)x1/(float)screen_width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)screen_height
		,2.f*(float)dx/(float)screen_width
		,2.f*(float)dy/(float)screen_height
		,eff,tech);
}

void UtilityRenderer::DrawQuad2(ID3D11DeviceContext *pContext,float x1,float y1,float dx,float dy,ID3DX11Effect* eff,ID3DX11EffectTechnique* tech)
{
	HRESULT hr=S_OK;
	setParameter(eff,"rect",x1,y1,dx,dy);
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void UtilityRenderer::RenderAngledQuad(ID3D11DeviceContext *pContext
									   ,const float *dr
									   ,float half_angle_radians
										,ID3DX11Effect* effect
										,ID3DX11EffectTechnique* tech
										,D3DXMATRIX view
										,D3DXMATRIX proj
										,D3DXVECTOR3 sun_dir)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	if(effect)
	{
//		setMatrix(effect,"worldViewProj",tmp1);
		//setParameter(effect,"lightDir",sun2);
	//	setParameter(effect,"radiusRadians",half_angle_radians);
	}
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}


void UtilityRenderer::DrawCube(void *context)
{
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(m_pCubemapVtxDecl);

	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void UtilityRenderer::DrawSphere(void *context,int latitudes,int longitudes)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// The number of indices per lat strip is (longitudes+1)*2.
	// The number of lat strips is (latitudes+1)
	pContext->Draw((longitudes+1)*(latitudes+1)*2,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void UtilityRenderer::DrawCubemap(crossplatform::DeviceContext &deviceContext,ID3D11ShaderResourceView *m_pCubeEnvMapSRV,float offsetx,float offsety)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	unsigned int num_v=0;
	D3D11_VIEWPORT								m_OldViewports[4];
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width		=m_OldViewports[0].Width*0.3f;
	viewport.Height		=m_OldViewports[0].Height*0.3f;
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	viewport.TopLeftX	=0.5f*(1.f+offsetx)*m_OldViewports[0].Width-viewport.Width/2;
	viewport.TopLeftY	=0.5f*(1.f-offsety)*m_OldViewports[0].Height-viewport.Height/2;
	pContext->RSSetViewports(1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=camera::Camera::MakeProjectionMatrix((float)viewport.Width/(float)viewport.Height,1.f,1.f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.Identity();
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float size=3.f;
	float d=2.0f*size/size_req;
	simul::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	simul::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._41=offs.x;
	world._42=offs.y;
	world._43=offs.z;
	camera::MakeWorldViewProjMatrix(wvp,world,view,proj);
	simul::dx11::setMatrix(m_pDebugEffect->asD3DX11Effect(),"worldViewProj",&wvp._11);
	//ID3DX11EffectTechnique*			tech		=m_pDebugEffect->GetTechniqueByName("draw_cubemap");
	ID3DX11EffectTechnique*				tech		=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("draw_cubemap_sphere");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pDebugEffect->asD3DX11Effect()->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"latitudes",16);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"longitudes",32);
	static float rr=6.f;
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"radius",rr);
	UtilityRenderer::DrawSphere(pContext,16,32);
	pContext->RSSetViewports(num_v,m_OldViewports);
}


void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
	SetDebugObjectName(m_pDepthStencilStateStored11,"m_pDepthStencilStateStored11");
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
	SetDebugObjectName(m_pRasterizerStateStored11,"m_pRasterizerStateStored11");
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
	SetDebugObjectName(m_pBlendStateStored11,"m_pBlendStateStored11");
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
	SetDebugObjectName(m_pSamplerStateStored11,"m_pSamplerStateStored11");
}

void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}