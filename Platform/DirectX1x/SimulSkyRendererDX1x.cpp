
// Copyright (c) 2007-2011 Simul Software Ltd
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
static DXGI_FORMAT sky_tex_format=DXGI_FORMAT_R32G32B32A32_FLOAT;
extern 	D3DXMATRIX view_matrices[6];

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Sky.h"
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

SimulSkyRendererDX1x::SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk)
	:simul::sky::BaseSkyRenderer(sk)
	,m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,m_pVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,m_pSkyEffect(NULL)
	,m_hTechniqueSky(NULL)
	,m_hTechniqueSky_CUBEMAP(NULL)
	,m_hTechniqueFade3DTo2D(NULL)
	,m_hTechniqueSun(NULL)
	,m_hTechniqueQuery(NULL)	
	,m_hTechniqueFlare(NULL)
	,m_hTechniquePlanet(NULL)
	,flare_texture(NULL)
	,flare_texture_SRV(NULL)
	,loss_2d(NULL)
	,inscatter_2d(NULL)
	,sun_occlusion(0.f)
	,d3dQuery(NULL)
	,mapped_sky(-1)
	,mapped_fade(-1)
	,cycle(0)
{
	for(int i=0;i<3;i++)
	{
		sky_textures[i]=NULL;
		sky_textures_SRV[i]=NULL;
		loss_textures[i]=NULL;
		inscatter_textures[i]=NULL;
		loss_textures_SRV[i]=NULL;
		insc_textures_SRV[i]=NULL;
	}
//EnableColourSky(false);
	SetCameraPosition(0,0,400.f);
	skyKeyframer->SetCalculateAllAltitudes(true);
	loss_2d		=new FramebufferDX1x(0,0);
	inscatter_2d=new FramebufferDX1x(0,0);
}

void SimulSkyRendererDX1x::SetStepsPerDay(unsigned steps)
{
	skyKeyframer->SetUniformKeyframes(steps);
}

void SimulSkyRendererDX1x::RestoreDeviceObjects( void* dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	D3D1x_QUERY_DESC qdesc=
	{
		D3D1x_QUERY_OCCLUSION,0
	};
    m_pd3dDevice->CreateQuery(&qdesc,&d3dQuery);
	HRESULT hr=S_OK;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	//hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	RecompileShaders();

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
    V_CHECK(m_pd3dDevice->CreateShaderResourceView( flare_texture,NULL,&flare_texture_SRV));

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

	if(loss_2d)
	{
		loss_2d->SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
		loss_2d->RestoreDeviceObjects(dev);
	}
	if(inscatter_2d)
	{
		inscatter_2d->SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
		inscatter_2d->RestoreDeviceObjects(dev);
	}
	ClearIterators();
}

void SimulSkyRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(loss_2d)
	{
		loss_2d->InvalidateDeviceObjects();
	}
	if(inscatter_2d)
	{
		inscatter_2d->InvalidateDeviceObjects();
	}
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
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(insc_textures_SRV[i]);
	}
	// Set the stored texture sizes to zero, so the textures will be re-created.
	fadeTexWidth=fadeTexHeight=numAltitudes=0;
	SAFE_RELEASE(d3dQuery);
}

bool SimulSkyRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return true;
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
	if(FAILED(hr=Map3D(inscatter_textures[s],&insc_texture_mapped)))
		return;
	mapped_fade=s;
}

void SimulSkyRendererDX1x::UnmapFade()
{
	if(mapped_fade==-1)
		return;
	Unmap3D(loss_textures[mapped_fade]);
	Unmap3D(inscatter_textures[mapped_fade]);
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

void SimulSkyRendererDX1x::FillSkyTex(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	MapSky(texture_index);
	if(!sky_texture_mapped.pData)
		return;
	float *float_ptr=(float *)(sky_texture_mapped.pData);
	texel_index+=alt_index*skyTexSize;
	float_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*float_ptr++=(*float4_array++);
}

float SimulSkyRendererDX1x::GetFadeInterp() const
{
	return skyKeyframer->GetInterpolation();
}

void SimulSkyRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::sky::BaseKeyframer::int3 i=skyKeyframer->GetTextureSizes();
	int num_dist=i.x;
	int num_elev=i.y;
	int num_alt=i.z;
	if(!num_dist||!num_elev||!num_alt)
		return;
	if(fadeTexWidth==num_dist&&fadeTexHeight==num_elev&&numAltitudes==num_alt&&skyTexSize==num_elev&&sky_texture_iterator[0].size()==numAltitudes)
		return;
	if(!m_pd3dDevice)
		return;
	fadeTexWidth=num_dist;
	fadeTexHeight=num_elev;
	numAltitudes=num_alt;
	for(int i=0;i<3;i++)
	{
		fade_texture_iterator[i].resize(numAltitudes);
		sky_texture_iterator[i].resize(numAltitudes);
		for(int j=0;j<numAltitudes;j++)
		{
			fade_texture_iterator[i][j].texture_index=i;
			sky_texture_iterator[i][j].texture_index=i;
		}
	}
	skyTexSize=num_elev;
	CreateSkyTextures();
	CreateFadeTextures();
	//CreateSunlightTextures();
}

void SimulSkyRendererDX1x::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		for(int j=0;j<numAltitudes;j++)
		{
			simul::sky::BaseKeyframer::seq_texture_fill texture_fill=skyKeyframer->GetSkyTextureFill(j,i,sky_texture_iterator[i][j]);
			if(texture_fill.num_texels&&sky_textures[i])
			{
				FillSkyTex(j,i,texture_fill.texel_index,texture_fill.num_texels,(const float*)texture_fill.float_array_1);
			}
			texture_fill=skyKeyframer->GetSequentialFadeTextureFill(j,i,fade_texture_iterator[i][j]);
			if(texture_fill.num_texels&&sky_textures[i])
			{
				FillFadeTex(j,i,texture_fill.texel_index,texture_fill.num_texels,(const float*)texture_fill.float_array_1,(const float*)texture_fill.float_array_2);
			}
		}
	}
	if(mapped_sky!=2)
		UnmapSky();
	if(mapped_fade!=2)
		UnmapFade();
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

	std::swap(inscatter_textures[0],inscatter_textures[1]);
	std::swap(inscatter_textures[1],inscatter_textures[2]);
	
	std::swap(sky_textures_SRV[0],sky_textures_SRV[1]);
	std::swap(sky_textures_SRV[1],sky_textures_SRV[2]);
	
	std::swap(loss_textures_SRV[0],loss_textures_SRV[1]);
	std::swap(loss_textures_SRV[1],loss_textures_SRV[2]);
	
	std::swap(insc_textures_SRV[0],insc_textures_SRV[1]);
	std::swap(insc_textures_SRV[1],insc_textures_SRV[2]);


	std::swap(sky_texture_iterator[0],sky_texture_iterator[1]);
	std::swap(sky_texture_iterator[1],sky_texture_iterator[2]);
}


void SimulSkyRendererDX1x::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		this->CycleTexturesForward();
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
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
		D3D1x_CPU_ACCESS_WRITE,		//D3D1x_CPU_ACCESS_READ|
		0
	};
	HRESULT hr;
	UnmapFade();
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(loss_textures_SRV[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(insc_textures_SRV[i]);
		V_CHECK(m_pd3dDevice->CreateTexture3D(&desc,NULL,&loss_textures[i]));
		V_CHECK(m_pd3dDevice->CreateTexture3D(&desc,NULL,&inscatter_textures[i]));

		V_CHECK(m_pd3dDevice->CreateShaderResourceView(loss_textures[i],NULL,&loss_textures_SRV[i]));
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(inscatter_textures[i],NULL,&insc_textures_SRV[i]));
	}
	if(loss_2d)
	{
		loss_2d->SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
		loss_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
	if(inscatter_2d)
	{
		inscatter_2d->SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
		inscatter_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
}

void SimulSkyRendererDX1x::FillFadeTex(int alt_index,int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array)
{
	texel_index+=alt_index*fadeTexWidth*fadeTexHeight;
	unsigned sub=D3D1xCalcSubresource(0,0,1);
	HRESULT hr=S_OK;
	MapFade(texture_index);
	float *float_ptr=(float *)(loss_texture_mapped.pData);
	float_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*float_ptr++=(*loss_float4_array++);

	float_ptr=(float *)(insc_texture_mapped.pData);
	float_ptr+=4*texel_index;
	for(int i=0;i<num_texels*4;i++)
		*float_ptr++=(*inscatter_float4_array++);
}

void SimulSkyRendererDX1x::CreateSkyTextures()
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
		D3D1x_CPU_ACCESS_WRITE,		//D3D1x_CPU_ACCESS_READ|
		0
	};
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
		SAFE_RELEASE(sky_textures_SRV[i]);
		V_CHECK(m_pd3dDevice->CreateTexture2D(&desc,NULL, &sky_textures[i]));
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(sky_textures[i],NULL,&sky_textures_SRV[i]));
	}
}

void SimulSkyRendererDX1x::RecompileShaders()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pSkyEffect);
	if(!m_pd3dDevice)
		return ;
	std::map<std::string,std::string> defines;
	if(y_vertical)
		defines["Y_VERTICAL"]="1";
	else
		defines["Z_VERTICAL"]="1";
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pSkyEffect,L"simul_sky.fx",defines));
	m_hTechniqueSky				=m_pSkyEffect->GetTechniqueByName("simul_sky");
	m_hTechniqueSky_CUBEMAP		=m_pSkyEffect->GetTechniqueByName("simul_sky_CUBEMAP");
	worldViewProj				=m_pSkyEffect->GetVariableByName("worldViewProj")->AsMatrix();

	projMatrix					=m_pSkyEffect->GetVariableByName("proj")->AsMatrix();
	cubemapViews				=m_pSkyEffect->GetVariableByName("cubemapViews")->AsMatrix();

	lightDirection				=m_pSkyEffect->GetVariableByName("lightDir")->AsVector();
	mieRayleighRatio			=m_pSkyEffect->GetVariableByName("mieRayleighRatio")->AsVector();
	hazeEccentricity			=m_pSkyEffect->GetVariableByName("hazeEccentricity")->AsScalar();
	skyInterp					=m_pSkyEffect->GetVariableByName("skyInterp")->AsScalar();
	altitudeTexCoord			=m_pSkyEffect->GetVariableByName("altitudeTexCoord")->AsScalar();
	
	m_hTechniqueFade3DTo2D=m_pSkyEffect->GetTechniqueByName("simul_fade_3d_to_2d");

	sunlightColour		=m_pSkyEffect->GetVariableByName("sunlightColour")->AsVector();
	m_hTechniqueSun		=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniqueFlare	=m_pSkyEffect->GetTechniqueByName("simul_flare");
	m_hTechniquePlanet	=m_pSkyEffect->GetTechniqueByName("simul_planet");
	flareTexture		=m_pSkyEffect->GetVariableByName("flareTexture")->AsShaderResource();

	skyTexture1			=m_pSkyEffect->GetVariableByName("skyTexture1")->AsShaderResource();
	skyTexture2			=m_pSkyEffect->GetVariableByName("skyTexture2")->AsShaderResource();

	fadeTexture1		=m_pSkyEffect->GetVariableByName("fadeTexture1")->AsShaderResource();
	fadeTexture2		=m_pSkyEffect->GetVariableByName("fadeTexture2")->AsShaderResource();

	m_hTechniqueQuery	=m_pSkyEffect->GetTechniqueByName("simul_query");

}

extern void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj);


bool SimulSkyRendererDX1x::RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians)
{
	float Yaw=atan2(dir.x,y_vertical?dir.z:dir.y);
	float Pitch=-asin(y_vertical?dir.y:dir.z);
	HRESULT hr=S_OK;
	return (hr==S_OK);
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
	m_hTechniqueSky->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		//hr=m_pSkyEffect->BeginPass(i);
		//m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
		m_pImmediateContext->Draw( 2,0 );
		//hr=m_pSkyEffect->EndPass();
	}
	//hr=m_pSkyEffect->End();
	return (hr==S_OK);*/
}

float SimulSkyRendererDX1x::CalcSunOcclusion(float cloud_occlusion)
{
	sun_occlusion=cloud_occlusion;
	if(!m_hTechniqueQuery)
		return sun_occlusion;
//	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToSun());
	float sun_angular_size=3.14159f/180.f/2.f;

	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	float zNear=-proj._43/proj._33;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_size);
	FixProjectionMatrix(proj,zFar*ff,zFar,IsYVertical());
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
	return sun_occlusion;
}
simul::sky::float4 sunlight;
bool SimulSkyRendererDX1x::RenderSun()
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=2700.f;
	sunlightColour->SetFloatVector(sunlight);

//	m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToSun());
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
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting)
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);

	if(do_lighting)
		ApplyPass(m_hTechniquePlanet->GetPassByIndex(0));
	else
		ApplyPass(m_hTechniqueFlare->GetPassByIndex(0));

	//m_pSkyEffect->SetTexture(flareTexture,(LPDIRECT3DTEXTURE9)tex);

	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();

	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(y_vertical?planet_dir4.y:planet_dir4.z);
	planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);
	D3DXVECTOR4 planet_dir(dir);

	// Make it bigger than it should be?
	static float size_mult=1.f;
	float planet_angular_size=size_mult*rad;
	// Start the query
	//m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&planet_colour));
	HRESULT hr=RenderAngledQuad(planet_dir,planet_angular_size);
	return hr==S_OK;
}

bool SimulSkyRendererDX1x::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
	if(!m_pSkyEffect)
		return (hr==S_OK);
	float magnitude=exposure*(1.f-sun_occlusion);
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
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
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToSun());
	hr=RenderAngledQuad(sun_dir,sun_angular_size*20.f*magnitude);
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::Render2DFades()
{
	if(!m_hTechniqueFade3DTo2D)
		return false;
	if(!loss_2d||!inscatter_2d)
		return false;
	HRESULT hr;
	skyInterp->SetFloat(skyKeyframer->GetInterpolation());
	altitudeTexCoord->SetFloat(skyKeyframer->GetAltitudeTexCoord());

	V_CHECK(fadeTexture1->SetResource(loss_textures_SRV[0]));
	V_CHECK(fadeTexture2->SetResource(loss_textures_SRV[1]));
	V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
	loss_2d->SetTargetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	loss_2d->Activate();
// Clear the screen to black:
    float clearColor[4]={0.0,0.0,0.0,0.0};
	m_pImmediateContext->ClearRenderTargetView(loss_2d->m_pHDRRenderTarget,clearColor);
	if(loss_2d->m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(loss_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
	loss_2d->RenderBufferToCurrentTarget();
	loss_2d->Deactivate();

	V_CHECK(fadeTexture1->SetResource(insc_textures_SRV[0]));
	V_CHECK(fadeTexture2->SetResource(insc_textures_SRV[1]));
	V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
	
	inscatter_2d->SetTargetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	inscatter_2d->Activate();
		
	m_pImmediateContext->ClearRenderTargetView(inscatter_2d->m_pHDRRenderTarget,clearColor);
	if(inscatter_2d->m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(inscatter_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|
		D3D1x_CLEAR_STENCIL, 1.f, 0);
	inscatter_2d->RenderBufferToCurrentTarget();
	inscatter_2d->Deactivate();
	
	V_CHECK(fadeTexture1->SetResource(NULL));
	V_CHECK(fadeTexture2->SetResource(NULL));
	V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
	return true;
}

bool SimulSkyRendererDX1x::Render(bool blend)
{
	HRESULT hr=S_OK;
	EnsureTexturesAreUpToDate();
	skyInterp->SetFloat(skyKeyframer->GetInterpolation());
	altitudeTexCoord->SetFloat(skyKeyframer->GetAltitudeTexCoord());
	//if(!cubemap)
		Render2DFades();
	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);
	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
// Fix projection
	FixProjectionMatrix(proj,1000.f,IsYVertical());

	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
#if 0 // Code for single-pass cubemap. This DX11 feature is not so hot.
	if(cubemap)
	{
		D3DXMATRIX cube_proj;
		D3DXMatrixPerspectiveFovLH(&cube_proj,
			3.1415926536f/2.f,
			1.f,
			1.f,
			200000.f);
		B_RETURN(projMatrix->SetMatrix(&cube_proj._11));
		B_RETURN(cubemapViews->SetMatrixArray((const float*)view_matrices,0,6));
	}
#endif
	PIXBeginNamedEvent(0,"Render Sky");
	skyTexture1->SetResource(sky_textures_SRV[0]);
	skyTexture2->SetResource(sky_textures_SRV[1]);

	simul::sky::float4 mie_rayleigh_ratio=skyKeyframer->GetMieRayleighRatio();
	simul::sky::float4 sun_dir(skyKeyframer->GetDirectionToSun());
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

	lightDirection->SetFloatVector(sun_dir);
	mieRayleighRatio->SetFloatVector(mie_rayleigh_ratio);
	hazeEccentricity->SetFloat	(skyKeyframer->GetMieEccentricity());
/*if(cubemap)
		hr=ApplyPass(m_hTechniqueSky_CUBEMAP->GetPassByIndex(0));
	else*/
		hr=ApplyPass(m_hTechniqueSky->GetPassByIndex(0));

	DrawCube();

	PIXEndNamedEvent();
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::RenderFades(int width,int h)
{
	HRESULT hr=S_OK;
	int size=width/4;
	if(h/(numAltitudes+2)<size)
		size=h/(numAltitudes+2);

	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
    D3DXMatrixOrthoLH(&ident,(float)width,(float)h,-100.f,100.f);
	ID3D1xEffectMatrixVariable*	worldViewProj=m_pSkyEffect->GetVariableByName("worldViewProj")->AsMatrix();
	worldViewProj->SetMatrix(ident);

	ID3D1xEffectTechnique*				tech		=m_pSkyEffect->GetTechniqueByName("simul_show_sky_texture");
	ID3D1xEffectShaderResourceVariable*	skyTexture	=m_pSkyEffect->GetVariableByName("skyTexture1")->AsShaderResource();

	skyTexture->SetResource(sky_textures_SRV[0]);
	RenderTexture(m_pd3dDevice,16+size		,32,8,size,tech);
	skyTexture->SetResource(sky_textures_SRV[1]);
	RenderTexture(m_pd3dDevice,32+size		,32,8,size,tech);
	UnmapSky();
	skyTexture->SetResource(sky_textures_SRV[2]);
	RenderTexture(m_pd3dDevice,48+size		,32,8,size,tech);
/*
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		m_pSkyEffect->SetFloat	(altitudeTexCoord	,atc);
		m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[0]);
		RenderTexture(m_pd3dDevice,8+2*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
		m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[1]);
		RenderTexture(m_pd3dDevice,8+3*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	
		m_pSkyEffect->SetTexture(fadeTexture2D, sky_textures[0]);
		RenderTexture(m_pd3dDevice,8+4*(size+8)		,(i)*(size+8)+8,8,size,sky_textures[0],m_pSkyEffect,m_hTechniqueShowSkyTexture);
		m_pSkyEffect->SetTexture(fadeTexture2D, sky_textures[1]);
		RenderTexture(m_pd3dDevice,8+4*(size+8)+16	,(i)*(size+8)+8,8,size,sky_textures[1],m_pSkyEffect,m_hTechniqueShowSkyTexture);
	}*/
	return (hr==S_OK);
}

void SimulSkyRendererDX1x::DrawCubemap(ID3D1xShaderResourceView*		m_pCubeEnvMapSRV)
{
	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixTranslation(&world,0.f,495.f,-20.f);
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
	ID3D1xEffectTechnique*				tech=m_pSkyEffect->GetTechniqueByName("draw_cubemap");
	ID3D1xEffectShaderResourceVariable*	cubeTexture=m_pSkyEffect->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(tech->GetPassByIndex(0));
return;
	//DrawCube();
}

void SimulSkyRendererDX1x::DrawCube()
{
	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );
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
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl );

	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pImmediateContext->Draw(36,0);
}

void SimulSkyRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
void SimulSkyRendererDX1x::Get3DLossAndInscatterTextures(void* *l1,void* *l2,
		void* *i1,void* *i2)
{
	*l1=(void*)loss_textures[0];
	*l2=(void*)loss_textures[1];
	*i1=(void*)inscatter_textures[0];
	*i2=(void*)inscatter_textures[1];
}
void SimulSkyRendererDX1x::Get2DLossAndInscatterTextures(void* *l1,
		void* *i1)
{
	if(loss_2d)
		*l1=(void*)loss_2d->hdr_buffer_texture;
	else
		*l1=NULL;
	if(inscatter_2d)
		*i1=(void*)inscatter_2d->hdr_buffer_texture;
	else
		*l1=NULL;
}


const char *SimulSkyRendererDX1x::GetDebugText() const
{
	static char txt[200];
	int hours=(int)(skyKeyframer->GetDaytime()*24.f);
	int minutes=(int)(60.f*(skyKeyframer->GetDaytime()*24.f-(float)hours));
	sprintf_s(txt,200,"Time: %02d:%02d\ncycle %d interp %3.3g",hours,minutes,cycle,skyKeyframer->GetInterpolation());
	return txt;
}

void SimulSkyRendererDX1x::SetYVertical(bool y)
{
	y_vertical=y;
	RecompileShaders();
}
void SimulSkyRendererDX1x::DrawLines(Vertext *lines,int vertex_count,bool strip)
{
}

void SimulSkyRendererDX1x::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
}