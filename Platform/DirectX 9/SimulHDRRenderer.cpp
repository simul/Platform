// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulHDRRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulHDRRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	static tstring filepath=TEXT("game:\\");
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
#endif
#include "CreateDX9Effect.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRenderer.h"
#include "SimulPrecipitationRenderer.h"
#include "Simul2DCloudRenderer.h"
#include "SimulSkyRenderer.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Macros.h"
#include "SimulAtmosphericsRenderer.h"
#include "Resources.h"


#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

SimulHDRRenderer::SimulHDRRenderer(int width,int height) :
	m_pBufferVertexDecl(NULL),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL),
	hdr_buffer_texture(NULL),
	faded_texture(NULL),
	buffer_depth_texture(NULL),
	brightpass_buffer_texture(NULL),
	bloom_texture(NULL),
	m_pHDRRenderTarget(NULL),
	m_pBufferDepthSurface(NULL),
	m_pFadedRenderTarget(NULL),
	exposure(1.f),
	gamma(1.f/2.2f),
	BufferWidth(width),
	BufferHeight(height),
	timing(0.f),
	exposure_multiplier(1.f),
	atmospherics(NULL)
{
}

bool SimulHDRRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr=S_OK;
	if(!m_pTonemapEffect)
#ifdef  MONTE_CARLO_BLUR
	{
		char blur_size[20];
		sprintf_s(blur_size,20,"%d",BLUR_SIZE);
		std::map<std::string,std::string> defines;
		defines["BLUR_SIZE"]=blur_size;
		B_RETURN(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx",defines));
	}
#else
		B_RETURN(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx"));
#endif
	ToneMapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_tonemap");
	ToneMapZWriteTechnique	=m_pTonemapEffect->GetTechniqueByName("simul_tonemap_zwrite");
	BrightpassTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_brightpass");
	BlurTechnique			=m_pTonemapEffect->GetTechniqueByName("simul_blur");
	Exposure				=m_pTonemapEffect->GetParameterByName(NULL,"exposure");
	Gamma					=m_pTonemapEffect->GetParameterByName(NULL,"gamma");
	hdrTexture				=m_pTonemapEffect->GetParameterByName(NULL,"hdrTexture");
	bloomTexture			=m_pTonemapEffect->GetParameterByName(NULL,"bloomTexture");
	B_RETURN(CreateBuffers());
	return (hr==S_OK);
}

bool SimulHDRRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
	SAFE_RELEASE(m_pFadedRenderTarget)
	if(m_pTonemapEffect)
        hr=m_pTonemapEffect->OnLostDevice();
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(faded_texture);
	SAFE_RELEASE(brightpass_buffer_texture);
	SAFE_RELEASE(bloom_texture);
	SAFE_RELEASE(buffer_depth_texture);
	return (hr==S_OK);
}

SimulHDRRenderer::~SimulHDRRenderer()
{
	InvalidateDeviceObjects();
}


bool SimulHDRRenderer::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}

void SimulHDRRenderer::SetBufferSize(int w,int h)
{
	BufferWidth=w;
	BufferHeight=h;
}

bool SimulHDRRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(hdr_buffer_texture);
#ifndef XBOX
	D3DFORMAT hdr_format=D3DFMT_A16B16G16R16F;
#else
	D3DFORMAT hdr_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=CanUse16BitFloats(m_pd3dDevice);
	if(hr!=S_OK)
#ifndef XBOX
		hdr_format=D3DFMT_A32B32G32R32F;
#else
		hdr_format=D3DFMT_LIN_A32B32G32R32F;
#endif
	B_RETURN(CanUseTexFormat(m_pd3dDevice,hdr_format));
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&hdr_buffer_texture,
											NULL
										));
	SAFE_RELEASE(faded_texture);
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&faded_texture,
											NULL
										));
	SAFE_RELEASE(brightpass_buffer_texture);
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth/4,
											BufferHeight/4,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&brightpass_buffer_texture,
											NULL
										));
	SAFE_RELEASE(bloom_texture);
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth/4,
											BufferHeight/4,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&bloom_texture,
											NULL
										));
	
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));


	D3DFORMAT fmtDepthTex = D3DFMT_UNKNOWN;
//	D3DFORMAT possibles[]={D3DFMT_D32,D3DFMT_D24S8,D3DFMT_D24FS8,D3DFMT_D24X8,D3DFMT_D16,D3DFMT_UNKNOWN};
	LPDIRECT3DSURFACE9 g_BackBuffer;
    m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_BackBuffer);
	D3DSURFACE_DESC desc;
	g_BackBuffer->GetDesc(&desc);
	SAFE_RELEASE(g_BackBuffer);
    D3DDISPLAYMODE d3ddm;
#ifdef XBOX
	if(FAILED(hr=m_pd3dDevice->GetDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
    {
        return (hr==S_OK);
    }
#else
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
	B_RETURN(d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm ));
#endif
	SAFE_RELEASE(m_pHDRRenderTarget);
	m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
	SAFE_RELEASE(m_pFadedRenderTarget)
	m_pFadedRenderTarget=MakeRenderTarget(faded_texture);
	/*m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
	m_pHDRRenderTarget->GetDesc(&desc);
	for(int i=0;i<100;i++)
	{
		D3DFORMAT possible=possibles[i];
		if(possible==D3DFMT_UNKNOWN)
			break;
		HRESULT h=CanUseDepthFormat(m_pd3dDevice,possible);
		if(FAILED(h))
			continue;
		h=IsDepthFormatOk(possible,d3ddm.Format,desc.Format);
		if(SUCCEEDED(h))
		{
			fmtDepthTex = possible;
			std::cout<<"Depth format found: "<<possible<<std::endl;
			break;
		}
	}*/
	SAFE_RELEASE(buffer_depth_texture);
	// Try creating a depth texture
	if(fmtDepthTex!=D3DFMT_UNKNOWN)
	{
		/*V_CHECK((m_pd3dDevice->CreateTexture(BufferWidth,
										BufferHeight,
										1,
										D3DUSAGE_DEPTHSTENCIL,
										fmtDepthTex,
										D3DPOOL_DEFAULT,
										&buffer_depth_texture,
										NULL
									)));*/
		hr=S_OK;
	}
	SAFE_RELEASE(m_pBufferDepthSurface);
	if(buffer_depth_texture)
		m_pBufferDepthSurface=MakeRenderTarget(buffer_depth_texture);
	return (hr==S_OK);
}

LPDIRECT3DSURFACE9 SimulHDRRenderer::MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture)
{
	LPDIRECT3DSURFACE9 pRenderTarget;
#ifdef XBOX
	XGTEXTURE_DESC desc;
	XGGetTextureDesc( pTexture, 0, &desc );
	D3DSURFACE_PARAMETERS SurfaceParams = {0};
	HRESULT hr=m_pd3dDevice->CreateRenderTarget(
		desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &pRenderTarget,&SurfaceParams);
	if(hr!=S_OK)
	{
		std::cout<<"SimulHDRRenderer::MakeRenderTarget - Failed to create render target!\n";
		wchar_t message[500];
		wsprintf(message,_T("SimulHDRRenderer::MakeRenderTarget - Failed to create render target from format %d, width %d, height %d",desc.Format,desc.Width,desc.Height);
		MessageBox(NULL,message, _T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
		return (hr==S_OK);
	}
#else
	if(!pTexture)
	{
		MessageBox(NULL, _T("Trying to create RenderTarget from NULL texture!"), _T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
	}
	HRESULT hr=S_OK;
	V_CHECK(pTexture->GetSurfaceLevel(0,&pRenderTarget));
#endif
	return pRenderTarget;
}
	static float depth_start=1.f;


bool SimulHDRRenderer::RenderBrightpass()
{
	HRESULT hr=S_OK;
	// Use the hdr buffer to make the brightpass:
	m_pBrightpassRenderTarget=MakeRenderTarget(brightpass_buffer_texture);
	m_pd3dDevice->SetRenderTarget(0,m_pBrightpassRenderTarget);
	B_RETURN(m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L));
	m_pTonemapEffect->SetTechnique(BrightpassTechnique);
	brightpassThreshold	=m_pTonemapEffect->GetParameterByName(NULL,"brightpassThreshold");
	brightPassOffsets	=m_pTonemapEffect->GetParameterByName(NULL,"brightpassOffsets");
	B_RETURN(m_pTonemapEffect->SetFloat(brightpassThreshold,10.f));
	D3DXVECTOR4 brightpass_offsets[4];

	D3DSURFACE_DESC desc;
	brightpass_buffer_texture->GetLevelDesc(0,&desc);

	// Because the source and destination are NOT the same sizes, we
	// need to provide offsets to correctly map between them.
	float sU = (2.0f / static_cast< float >(desc.Width));
	float sV = (2.0f / static_cast< float >(desc.Height));

	// The last two components (z,w) are unused.
	brightpass_offsets[0]=D3DXVECTOR4(-0.5f*sU, 0.5f*sV,0.0f,0.0f);
	brightpass_offsets[1]=D3DXVECTOR4( 0.5f*sU, 0.5f*sV,0.0f,0.0f);
	brightpass_offsets[2]=D3DXVECTOR4(-0.5f*sU,-0.5f*sV,0.0f,0.0f);
	brightpass_offsets[3]=D3DXVECTOR4( 0.5f*sU,-0.5f*sV,0.0f,0.0f);

	B_RETURN(m_pTonemapEffect->SetVectorArray(brightPassOffsets,brightpass_offsets,4));
	B_RETURN(m_pTonemapEffect->SetTexture(hdrTexture,last_texture));

	brightpass_buffer_texture->GetLevelDesc(0,&desc);
	RenderBufferToCurrentTarget(desc.Width,desc.Height,true);
	return (hr==S_OK);
}

float gaussian( float x, float mean, float std_deviation )
{
    return ( 1.0f / sqrt( 2.0f * D3DX_PI * std_deviation * std_deviation ) ) 
            * expf( (-((x-mean)*(x-mean)))/(2.0f * std_deviation * std_deviation) );
}

bool SimulHDRRenderer::RenderBloom()
{
	HRESULT hr=S_OK;
	// Use the hdr buffer to make the brightpass:
	LPDIRECT3DSURFACE9	BloomRenderTarget;
	BloomRenderTarget=MakeRenderTarget(bloom_texture);
	m_pd3dDevice->SetRenderTarget(0,BloomRenderTarget);
	B_RETURN(m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L));
	m_pTonemapEffect->SetTechnique(BlurTechnique);
	bloomOffsets	=m_pTonemapEffect->GetParameterByName(NULL,"bloomOffsets");
	bloomWeights	=m_pTonemapEffect->GetParameterByName(NULL,"bloomWeights");
	D3DSURFACE_DESC desc;
	brightpass_buffer_texture->GetLevelDesc(0,&desc);
	static float offset_scale=1.f;
	static float                       g_GaussMultiplier           = 0.0025f; 
    static float                       g_GaussStdDev               = 0.005f; 
#ifdef MONTE_CARLO_BLUR
simul::math::RandomNumberGenerator r;
	D3DXVECTOR4 bloom_offsets[BLUR_SIZE];
	float bloom_weights[BLUR_SIZE];
	for(int i=0;i<BLUR_SIZE;i++ )
	{
		int x=i/3;
		int y=i-3*x;
		float offsetx=r.FRand();
		float offsety=r.FRand();
		float X=(float)x+offsetx;
		float Y=(float)y+offsety;
		float angle=X/4.f*2.f*pi;
		float rad=4.f*sqrt(Y/4.f)/(float)desc.Width;
		float tx=rad*cos(angle);
		float ty=rad*sin(angle);
		bloom_offsets[i].x=offset_scale*tx;
		bloom_offsets[i].y=offset_scale*ty;
		bloom_offsets[i].z=0;
		bloom_offsets[i].w=0;
		bloom_weights[i] = g_GaussMultiplier*gaussian(rad,0,g_GaussStdDev );
	}
	B_RETURN(m_pTonemapEffect->SetVectorArray(bloomOffsets,bloom_offsets,BLUR_SIZE));
	B_RETURN(m_pTonemapEffect->SetFloatArray(bloomWeights,bloom_weights,BLUR_SIZE));
#else
	D3DXVECTOR4 bloom_offsets[BLUR_SIZE*2+1];
	float bloom_weights[BLUR_SIZE*2+1];
	static float weight_mult=5e-7f;
	int j=0;
	for( int i = 0; i < BLUR_SIZE+1; i++ )
	{
       //     i =  0,  1,  2,  3, 4,  5,  6,  7,  8
       //Offset = -4, -3, -2, -1, 0, +1, +2, +3, +4
       // 'x' is just an alias to map the [0,32] range down to a [-1,+1]
       float x = 2.f*((float)i/(float)BLUR_SIZE) - 1.f;
		bloom_offsets[j].x=offset_scale*x*(float)BLUR_SIZE/(float)desc.Width;
		
		bloom_offsets[j].y=0;
		bloom_offsets[j].z=0;
		bloom_offsets[j].w=0;
		if(x<0)
			x*=-1.f;
       bloom_weights[j] = weight_mult /(x+0.001f);//g_GaussMultiplier*gaussian( x, 0,g_GaussStdDev );
	   j++;
	}
	for( int i = 0; i < BLUR_SIZE+1; i++ )
	{
		if(i==BLUR_SIZE/2)
			continue;
       //     i =  0,  1,  2,  3, 4,  5,  6,  7,  8
       //Offset = -4, -3, -2, -1, 0, +1, +2, +3, +4
       // 'x' is just an alias to map the [0,32] range down to a [-1,+1]
       float x = 2.f*((float)i/(float)BLUR_SIZE) - 1.f;
		bloom_offsets[j].y=offset_scale*x*(float)BLUR_SIZE/(float)desc.Width;
		
		bloom_offsets[j].x=0;
		bloom_offsets[j].z=0;
		bloom_offsets[j].w=0;
		if(x<0)
			x*=-1.f;
       bloom_weights[j] = weight_mult /(x+0.001f);//g_GaussMultiplier*gaussian( x, 0,g_GaussStdDev );
	   j++;
	}
	hr=m_pTonemapEffect->SetVectorArray(bloomOffsets,bloom_offsets,BLUR_SIZE*2+1);
	hr=m_pTonemapEffect->SetFloatArray(bloomWeights,bloom_weights,BLUR_SIZE*2+1);
#endif

	B_RETURN(m_pTonemapEffect->SetTexture(hdrTexture,last_texture));

	bloom_texture->GetLevelDesc(0,&desc);
	RenderBufferToCurrentTarget(desc.Width,desc.Height,true);
	//hr=m_pTonemapEffect->SetTexture(hdrTexture,bloom_texture);
	//RenderBufferToCurrentTarget(desc.Width,desc.Height,true);
	return (hr==S_OK);
}

bool SimulHDRRenderer::StartRender()
{		 
	PIXBeginNamedEvent(0xFF88FFFF,"SimulHDRRenderer::StartRender to FinishRender");
	HRESULT hr=S_OK;
	m_pOldRenderTarget		=NULL;
	m_pOldDepthSurface		=NULL;
	
	hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
	hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
	
	hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
	
	if(m_pBufferDepthSurface)
		hr=(m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface));
	
	hr=m_pd3dDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE);
	hr=m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
	hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	static float depth_start=1.f;
	hr=m_pd3dDevice->Clear(0L, NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L);

	last_texture=hdr_buffer_texture;
	return (hr==S_OK);;
}

bool SimulHDRRenderer::ApplyFade()
{
	HRESULT hr=S_OK;
	if(!atmospherics)
		return (hr==S_OK);
	atmospherics->SetInputTextures(hdr_buffer_texture,buffer_depth_texture);
	m_pd3dDevice->SetRenderTarget(0,m_pFadedRenderTarget);
	if(m_pBufferDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
	else if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	B_RETURN(m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET,0xFF000000,1.f,0L));
	last_texture=faded_texture;
	return (hr==S_OK);
}

bool SimulHDRRenderer::FinishRender()
{
	HRESULT hr=S_OK;
	D3DSURFACE_DESC desc;
#ifdef XBOX
	m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);

	// Create the brightpass - eliminate pixels with brightness below a certain threshold.
//	RenderBrightpass();
	// Use the brightpass to create the blurred bloom texture/
	//RenderBloom();

	// using gamma, render the hdr image to the LDR buffer:
	m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	m_pOldRenderTarget->GetDesc(&desc);

	B_RETURN(m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET,0xFF000000,depth_start,0L));
	m_pTonemapEffect->SetTechnique(ToneMapTechnique);
	B_RETURN(m_pTonemapEffect->SetFloat(Exposure,exposure*exposure_multiplier));
	B_RETURN(m_pTonemapEffect->SetFloat(Gamma,gamma));
	B_RETURN(m_pTonemapEffect->SetTexture(hdrTexture,last_texture));
	B_RETURN(m_pTonemapEffect->SetTexture(bloomTexture,bloom_texture));

	RenderBufferToCurrentTarget(desc.Width,desc.Height,true);

	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	PIXEndNamedEvent();
	return (hr==S_OK);
}

void SimulHDRRenderer::SetExposure(float ex)
{
	exposure=ex;
}

void SimulHDRRenderer::SetGamma(float g)
{
	gamma=g;
}

bool SimulHDRRenderer::RenderBufferToCurrentTarget(int width,int height,bool do_tonemap)
{
	width;height;
	HRESULT hr=S_OK;
	DrawFullScreenQuad(m_pd3dDevice,do_tonemap?m_pTonemapEffect:NULL);
	return (hr==S_OK);
}

const char *SimulHDRRenderer::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

float SimulHDRRenderer::GetTiming() const
{
	return timing;
}