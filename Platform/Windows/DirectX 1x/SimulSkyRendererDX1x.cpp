
// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRendererDX1x.cpp A renderer for skies.

#include "SimulSkyRendererDX1x.h"

#include <tchar.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <dxerr.h>
#include <string>
typedef std::basic_string<TCHAR> tstring;
static tstring filepath=TEXT("");
static DXGI_FORMAT sky_tex_format=DXGI_FORMAT_R16G16B16A16_FLOAT;


#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/SkyNode.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/TextureGenerator.h"
#include "MacrosDX1x.h"
#include "CreateEffectDX1x.h"

struct Vertex_t
{
	float x,y,z;
};
static const float size=5.f;
static Vertex_t vertices[36] =
{
	{-size,		-size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{size,		size,	size},
	{-size,		size,	size},
	{-size,		-size,	size},
	
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
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{ size,		-size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,  -size},
	
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	 size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

typedef std::basic_string<TCHAR> tstring;

SimulSkyRendererDX1x::SimulSkyRendererDX1x() :
	simul::sky::BaseSkyRenderer(),
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pVtxDecl(NULL),
	m_pVertexBuffer(NULL),
	m_pSkyEffect(NULL),
	m_hTechniqueSky(NULL),
	m_hTechniqueSun(NULL),
	m_hTechniqueQuery(NULL),	
	m_hTechniqueFlare(NULL),
	flare_texture(NULL),
	flare_texture_SRV(NULL),
	sun_occlusion(0.f),
	d3dQuery(NULL),
	mapped_sky(-1),
	mapped_fade(-1),
	cycle(0)
{
	for(int i=0;i<3;i++)
	{
		sky_textures[i]=NULL;
		sky_textures_SRV[i]=NULL;
		loss_textures[i]=NULL;
		insc_textures[i]=NULL;
		loss_textures_SRV[i]=NULL;
		insc_textures_SRV[i]=NULL;
	}
	cam_pos.x=cam_pos.z=cam_pos.y=0.f;
	//loss_texture_mapped.pData=NULL;
	//insc_texture_mapped.pData=NULL;
	EnableColourSky(false);
	skyKeyframer->SetCalculateAllAltitudes(true);
}

void SimulSkyRendererDX1x::SetStepsPerDay(unsigned steps)
{
	skyKeyframer->SetUniformKeyframes(steps);
}

simul::sky::SkyInterface *SimulSkyRendererDX1x::GetSkyInterface()
{
	return skyInterface;
}

simul::sky::SkyKeyframer *SimulSkyRendererDX1x::GetSkyKeyframer()
{
	return skyKeyframer.get();
}
	

simul::sky::FadeTableInterface *SimulSkyRendererDX1x::GetFadeTableInterface()
{
	return skyKeyframer.get();
}
	
HRESULT SimulSkyRendererDX1x::RestoreDeviceObjects( ID3D1xDevice* dev)
{
	m_pd3dDevice=dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	D3D1x_QUERY_DESC qdesc=
	{
		D3D1x_QUERY_OCCLUSION,0
	};
    m_pd3dDevice->CreateQuery(&qdesc, &d3dQuery );
	HRESULT hr=S_OK;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	//hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	SAFE_RELEASE(m_pSkyEffect);
	hr=CreateSkyEffect();
	m_hTechniqueSky		=m_pSkyEffect->GetTechniqueByName("simul_sky");
	worldViewProj		=m_pSkyEffect->GetVariableByName("worldViewProj")->AsMatrix();
	lightDirection		=m_pSkyEffect->GetVariableByName("lightDir")->AsVector();
	mieRayleighRatio	=m_pSkyEffect->GetVariableByName("mieRayleighRatio")->AsVector();
	hazeEccentricity	=m_pSkyEffect->GetVariableByName("hazeEccentricity")->AsScalar();
	skyInterp			=m_pSkyEffect->GetVariableByName("skyInterp")->AsScalar();
	altitudeTexCoord	=m_pSkyEffect->GetVariableByName("altitudeTexCoord")->AsScalar();
	
	sunlightColour		=m_pSkyEffect->GetVariableByName("sunlightColour")->AsVector();
	m_hTechniqueSun		=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniqueFlare	=m_pSkyEffect->GetTechniqueByName("simul_flare");
	flareTexture		=m_pSkyEffect->GetVariableByName("flareTexture")->AsShaderResource();

	skyTexture1			=m_pSkyEffect->GetVariableByName("skyTexture1")->AsShaderResource();
	skyTexture2			=m_pSkyEffect->GetVariableByName("skyTexture2")->AsShaderResource();

	m_hTechniqueQuery	=m_pSkyEffect->GetTechniqueByName("simul_query");
	skyKeyframer->SetCallback(this);
	// CreateSkyTexture() will be called

	SAFE_RELEASE(flare_texture);
	D3DX1x_IMAGE_LOAD_INFO loadInfo;
	ID3D1xResource *res=NULL;
	hr=D3DX1xCreateTextureFromFile(  m_pd3dDevice,
									L"Media/Textures/Sunburst.dds",
									&loadInfo,
									NULL,
									&res,
									NULL
									);

	flare_texture=static_cast<ID3D1xTexture2D*>(res);
    V_RETURN(m_pd3dDevice->CreateShaderResourceView( flare_texture,NULL,&flare_texture_SRV));

    D3D1x_PASS_DESC PassDesc;
	m_hTechniqueSky->GetPassByIndex(0)->GetDesc(&PassDesc);
	D3D1x_INPUT_ELEMENT_DESC decl[]=
	{
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout(decl,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pVtxDecl);

	D3D1x_BUFFER_DESC desc=
	{
        36*sizeof(Vertex_t),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertex_t);

	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer);

	fadeTableInterface->SetCallback(NULL);
	fadeTableInterface->SetCallback(this);
	return hr;
}

HRESULT SimulSkyRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	UnmapSky();
	UnmapFade();
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pVtxDecl);

	SAFE_RELEASE(flare_texture);

	SAFE_RELEASE(flare_texture_SRV);

	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
		SAFE_RELEASE(sky_textures_SRV[i]);
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(loss_textures_SRV[i]);
		SAFE_RELEASE(insc_textures[i]);
		SAFE_RELEASE(insc_textures_SRV[i]);
	}

	SAFE_RELEASE(d3dQuery);
	skyKeyframer->SetCallback(NULL);
	return hr;
}

HRESULT SimulSkyRendererDX1x::Destroy()
{
	return InvalidateDeviceObjects();
}

SimulSkyRendererDX1x::~SimulSkyRendererDX1x()
{
	Destroy();
}

void SimulSkyRendererDX1x::MapFade(int s)
{
	if(mapped_fade==s)
		return;
	if(mapped_fade!=-1)
		UnmapFade();
	HRESULT hr;
	if(FAILED(hr=Map3D(loss_textures[s],&loss_texture_mapped)))
		return;	   
	if(FAILED(hr=Map3D(insc_textures[s],&insc_texture_mapped)))
		return;
	mapped_fade=s;
}

void SimulSkyRendererDX1x::UnmapFade()
{
	if(mapped_fade==-1)
		return;
	Unmap3D(loss_textures[mapped_fade]);
	Unmap3D(insc_textures[mapped_fade]);
	mapped_fade=-1;
}

void SimulSkyRendererDX1x::MapSky(int s)
{
	if(mapped_sky==s)
		return;
	if(mapped_sky!=-1)
		UnmapSky();
	HRESULT hr;
	if(FAILED(hr=Map2D(sky_textures[s],&sky_texture_mapped)))
		return;
	mapped_sky=s;
}
void SimulSkyRendererDX1x::UnmapSky()
{
	if(mapped_sky==-1)
		return;
	Unmap2D(sky_textures[mapped_sky]);
	mapped_sky=-1;
}
void SimulSkyRendererDX1x::FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	HRESULT hr;
	MapSky(texture_index);
	if(!sky_texture_mapped.pData)
		return;
	// Convert the array of floats into float16 values for the texture.
	short *short_ptr=(short *)(sky_texture_mapped.pData);
	texel_index+=alt_index*skyTexSize;
	short_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
}

float SimulSkyRendererDX1x::GetFadeInterp() const
{
	return skyKeyframer->GetInterpolation();
}

void SimulSkyRendererDX1x::SetSkyTextureSize(unsigned size)
{
	if(!m_pd3dDevice)
		return;
	skyTexSize=size;
	CreateSkyTexture();
}

void SimulSkyRendererDX1x::SetFadeTextureSize(unsigned width,unsigned length,unsigned depth)
{
	if(!m_pd3dDevice)
		return;
	if(fadeTexWidth!=width||fadeTexHeight!=length||numAltitudes!=depth)
	{
		fadeTexWidth=width;
		fadeTexHeight=length;
		numAltitudes=depth;
		CreateFadeTextures();
	}
}

void SimulSkyRendererDX1x::CreateFadeTextures()
{
	D3D1x_TEXTURE3D_DESC desc=
	{
		fadeTexWidth,
		fadeTexHeight,
		numAltitudes,
		1,
		sky_tex_format,
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	HRESULT hr;
	UnmapFade();
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(insc_textures[i]);
		hr=m_pd3dDevice->CreateTexture3D(&desc,NULL,&loss_textures[i]);
		hr=m_pd3dDevice->CreateTexture3D(&desc,NULL,&insc_textures[i]);
	}
}

void SimulSkyRendererDX1x::FillFadeTexturesSequentially(int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array)
{
	unsigned sub=D3D1xCalcSubresource(0,0,1);
	HRESULT hr=S_OK;
	MapFade(texture_index);
	// Convert the array of floats into float16 values for the texture.
	short *short_ptr=(short *)(loss_texture_mapped.pData);
	short_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*loss_float4_array++);

	short_ptr=(short *)(insc_texture_mapped.pData);
	short_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*inscatter_float4_array++);
}

void SimulSkyRendererDX1x::CycleTexturesForward()
{
	cycle++;
	UnmapSky();
	UnmapFade();

	std::swap(sky_textures[0],sky_textures[1]);
	std::swap(sky_textures[1],sky_textures[2]);

	std::swap(loss_textures[0],loss_textures[1]);
	std::swap(loss_textures[1],loss_textures[2]);

	std::swap(insc_textures[0],insc_textures[1]);
	std::swap(insc_textures[1],insc_textures[2]);
	
	std::swap(sky_textures_SRV[0],sky_textures_SRV[1]);
	std::swap(sky_textures_SRV[1],sky_textures_SRV[2]);
	
	std::swap(loss_textures_SRV[0],loss_textures_SRV[1]);
	std::swap(loss_textures_SRV[1],loss_textures_SRV[2]);
	
	std::swap(insc_textures_SRV[0],insc_textures_SRV[1]);
	std::swap(insc_textures_SRV[1],insc_textures_SRV[2]);
}

HRESULT SimulSkyRendererDX1x::CreateSkyTexture()
{
	HRESULT hr=S_OK;
	D3D1x_TEXTURE2D_DESC desc=
	{
		skyTexSize,numAltitudes,
		1,1,
		sky_tex_format,
		{1,0},
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
		SAFE_RELEASE(sky_textures_SRV[i]);
		V_RETURN(m_pd3dDevice->CreateTexture2D(&desc,NULL, &sky_textures[i]));
		V_RETURN(m_pd3dDevice->CreateShaderResourceView(sky_textures[i],NULL,&sky_textures_SRV[i]));
	}
	return hr;
}

HRESULT SimulSkyRendererDX1x::CreateSkyEffect()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pSkyEffect);
	V_RETURN(CreateEffect(m_pd3dDevice,&m_pSkyEffect,L"simul_sky.fx"));
	return hr;
}

extern void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj);


HRESULT SimulSkyRendererDX1x::RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians)
{
	float Yaw=atan2(dir.x,dir.y);
	float Pitch=-asin(dir.z);
	HRESULT hr=S_OK;
	return hr;
	/*
	D3DXMATRIX tmp1, tmp2,wvp;
	D3DXMatrixIdentity(&world);
	D3DXMatrixRotationYawPitchRoll(
		  &world,
		  Yaw,
		  Pitch,
		  0
		);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	MakeWorldViewProjMatrix(&wvp,world,view,proj);

	worldViewProj->SetMatrix((&wvp._11));

	struct Vertext
	{
		D3DXVECTOR3 position;
		D3DXVECTOR2 texcoords;
	};
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	float w=1.f;
	float d=w/tan(half_angle_radians);
	Vertext vertices[4] =
	{
		D3DXVECTOR3(-w,	-w,	d),D3DXVECTOR2(0.f,0.f),
		D3DXVECTOR3( w,	-w,	d),D3DXVECTOR2(1.f,0.f),
		D3DXVECTOR3( w,	 w,	d),D3DXVECTOR2(1.f,1.f),
		D3DXVECTOR3(-w,	 w,	d),D3DXVECTOR2(0.f,1.f),
	};
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	UINT passes=1;
	m_hTechniqueSky->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		//hr=m_pSkyEffect->BeginPass(i);
		//m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
		m_pImmediateContext->Draw( 2,0 );
		//hr=m_pSkyEffect->EndPass();
	}
	//hr=m_pSkyEffect->End();
	return hr;*/
}

void SimulSkyRendererDX1x::CalcSunOcclusion(float cloud_occlusion)
{
	if(!m_hTechniqueQuery)
		return;
//	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
	float sun_angular_size=3.14159f/180.f/2.f;

	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	float zNear=-proj._43/proj._33;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_size);
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
/*
	// Start the query
	d3dQuery->Begin();
	RenderAngledQuad(sun_dir,sun_angular_size);
	// End the query, get the data
	d3dQuery->End();
    // Loop until the data becomes available
    UINT64 pixelsVisible = 0;
    while (d3dQuery->GetData((void *) &pixelsVisible,sizeof(UINT64),0) == S_FALSE);
	sun_occlusion=1.f-(float)pixelsVisible/560.f;
	if(sun_occlusion<0)
		sun_occlusion=0;
	sun_occlusion=1.f-(1.f-cloud_occlusion)*(1.f-sun_occlusion);
	proj=tmp;*/
}
simul::sky::float4 sunlight;
HRESULT SimulSkyRendererDX1x::RenderSun()
{
	float alt_km=0.001f*cam_pos.y;
	sunlight=skyInterface->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=2700.f;
	sunlightColour->SetFloatVector(sunlight);

//	m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
	float sun_angular_size=3.14159f/180.f/2.f;
HRESULT hr=S_OK;
	// Start the query
/*d3dQuery->Begin();
	hr=RenderAngledQuad(sun_dir,sun_angular_size);
	// End the query, get the data
    d3dQuery->End();

    // Loop until the data becomes available
    UINT64 pixelsVisible = 0;
    
    while (d3dQuery->GetData((void *) &pixelsVisible,sizeof(UINT64),0) == S_FALSE);*/
	return hr;
}

HRESULT SimulSkyRendererDX1x::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
	if(!m_pSkyEffect)
		return hr;
	float magnitude=exposure*(1.f-sun_occlusion);
	float alt_km=0.001f*cam_pos.y;
	sunlight=skyInterface->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.5f;
	sunlight*=sun_mult*magnitude;
	sunlightColour->SetFloatVector(sunlight);
	//m_pSkyEffect->SetTechnique(m_hTechniqueFlare);
	//flareTexture->SetResource(flare_texture_SRV);
	float sun_angular_size=3.14159f/180.f/2.f;
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
	hr=RenderAngledQuad(sun_dir,sun_angular_size*20.f*magnitude);
	return hr;
}

HRESULT SimulSkyRendererDX1x::Render()
{
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1, tmp2,wvp;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
// Fix projection
	float zNear=-proj._43/proj._33;
	float zFar=1000.f;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);

	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->AsMatrix()->SetMatrix(&wvp._11);


	PIXBeginNamedEvent(0,"Render Sky");
	skyTexture1->SetResource(sky_textures_SRV[0]);
	skyTexture2->SetResource(sky_textures_SRV[1]);

	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );

	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();
	simul::sky::float4 sun_dir(skyInterface->GetDirectionToSun());
	//if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

	lightDirection->SetFloatVector(sun_dir);
	mieRayleighRatio->SetFloatVector(mie_rayleigh_ratio);
	hazeEccentricity->SetFloat	(skyInterface->GetMieEccentricity());
	skyInterp->SetFloat(skyKeyframer->GetInterpolation());
	altitudeTexCoord->SetFloat(skyKeyframer->GetAltitudeTexCoord());
	UINT passes=1;
	hr=ApplyPass(m_hTechniqueSky->GetPassByIndex(0));

	UINT stride = sizeof(Vertex_t);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
												1,					// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset );			// array of offset values, one for each buffer

	// Set the input layout
	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );

	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pImmediateContext->Draw(36,0);

	PIXEndNamedEvent();
	return hr;
}

void SimulSkyRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}


const char *SimulSkyRendererDX1x::GetDebugText() const
{
	static char txt[200];
	int hours=(int)(skyKeyframer->GetDaytime()*24.f);
	int minutes=(int)(60.f*(skyKeyframer->GetDaytime()*24.f-(float)hours));
	sprintf_s(txt,200,"Time: %02d:%02d\ncycle %d interp %3.3g",hours,minutes,cycle,skyKeyframer->GetInterpolation());
	return txt;
}