
// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRendererDX1x.cpp A renderer for skies.
#define NOMINMAX
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
#include "Simul/Math/Vector3.h"
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
	,moon_texture_SRV(NULL)
	,loss_2d(NULL)
	,inscatter_2d(NULL)
	,skylight_2d(NULL)
	,sun_occlusion(0.f)
	,d3dQuery(NULL)
	,mapped_fade(-1)
	,cycle(0)
{
	for(int i=0;i<3;i++)
	{
		loss_textures[i]=NULL;
		inscatter_textures[i]=NULL;
		skylight_textures[i]=NULL;
		loss_textures_SRV[i]=NULL;
		insc_textures_SRV[i]=NULL;
		skyl_textures_SRV[i]=NULL;
	}
//EnableColourSky(false);
	SetCameraPosition(0,0,400.f);
	skyKeyframer->SetCalculateAllAltitudes(true);
	loss_2d		=new FramebufferDX1x(0,0);
	inscatter_2d=new FramebufferDX1x(0,0);
	skylight_2d	=new FramebufferDX1x(0,0);
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
		loss_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		loss_2d->RestoreDeviceObjects(dev);
	}
	if(inscatter_2d)
	{
		inscatter_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		inscatter_2d->RestoreDeviceObjects(dev);
	}
	if(skylight_2d)
	{
		skylight_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		skylight_2d->RestoreDeviceObjects(dev);
	}
	SAFE_RELEASE(moon_texture_SRV);
	MoonTexture="Moon.png";
	moon_texture_SRV=simul::dx1x_namespace::LoadTexture(MoonTexture.c_str());
	SetPlanetImage(moon_index,moon_texture_SRV);
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
	if(skylight_2d)
	{
		skylight_2d->InvalidateDeviceObjects();
	}
	UnmapFade();
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pVtxDecl);

	SAFE_RELEASE(flare_texture);

	SAFE_RELEASE(flare_texture_SRV);
	SAFE_RELEASE(moon_texture_SRV);

	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(loss_textures_SRV[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(insc_textures_SRV[i]);
		SAFE_RELEASE(skylight_textures[i]);
		SAFE_RELEASE(skyl_textures_SRV[i]);
	}
	// Set the stored texture sizes to zero, so the textures will be re-created.
	numFadeDistances=numFadeElevations=numAltitudes=0;
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
	if(FAILED(hr=Map3D(skylight_textures[s],&skyl_texture_mapped)))
		return;
	mapped_fade=s;
}

void SimulSkyRendererDX1x::UnmapFade()
{
	if(mapped_fade==-1)
		return;
	Unmap3D(loss_textures[mapped_fade]);
	Unmap3D(inscatter_textures[mapped_fade]);
	Unmap3D(skylight_textures[mapped_fade]);
	mapped_fade=-1;
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
	if(numFadeDistances==num_dist&&numFadeElevations==num_elev&&numAltitudes==num_alt&&skyTexSize==num_elev)
		return;
	if(!m_pd3dDevice)
		return;
	numFadeDistances=num_dist;
	numFadeElevations=num_elev;
	numAltitudes=num_alt;
	for(int i=0;i<3;i++)
	{
		fade_texture_iterator[i].texture_index=i;
	}
	skyTexSize=num_elev;
	CreateFadeTextures();
}

void SimulSkyRendererDX1x::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		bool reset=false;
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=skyKeyframer->GetSequentialFadeTextureFill(i,fade_texture_iterator[i]);
		if(reset)
		{
			fade_texture_iterator[i].texel_index=0;
		}
		if(texture_fill.num_texels)
		{
			FillFadeTex(i,texture_fill.texel_index,texture_fill.num_texels,(const simul::sky::float4*)texture_fill.float_array_1,(const simul::sky::float4*)texture_fill.float_array_2,(const simul::sky::float4*)texture_fill.float_array_3);
		}
	}
	if(mapped_fade!=2)
		UnmapFade();
}

void SimulSkyRendererDX1x::CycleTexturesForward()
{
	cycle++;

	// DON'T unmap, because DX11 can't cope with this.
	//UnmapFade();
	// Instead, whatever was the mapped texture is cycled too.
	if(mapped_fade!=-1)
	{
		mapped_fade--;
		if(mapped_fade<0)
			mapped_fade+=3;
	}

	std::swap(loss_textures[0],loss_textures[1]);
	std::swap(loss_textures[1],loss_textures[2]);

	std::swap(inscatter_textures[0],inscatter_textures[1]);
	std::swap(inscatter_textures[1],inscatter_textures[2]);

	std::swap(skylight_textures[0],skylight_textures[1]);
	std::swap(skylight_textures[1],skylight_textures[2]);
	
	std::swap(loss_textures_SRV[0],loss_textures_SRV[1]);
	std::swap(loss_textures_SRV[1],loss_textures_SRV[2]);
	
	std::swap(insc_textures_SRV[0],insc_textures_SRV[1]);
	std::swap(insc_textures_SRV[1],insc_textures_SRV[2]);
	
	std::swap(skyl_textures_SRV[0],skyl_textures_SRV[1]);
	std::swap(skyl_textures_SRV[1],skyl_textures_SRV[2]);

	std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
	std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
	for(int i=0;i<3;i++)
		fade_texture_iterator[i].texture_index=i;
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
		numAltitudes,
		numFadeElevations,
		numFadeDistances,
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
		SAFE_RELEASE(skylight_textures[i]);
		SAFE_RELEASE(skyl_textures_SRV[i]);
		V_CHECK(m_pd3dDevice->CreateTexture3D(&desc,NULL,&loss_textures[i]));
		V_CHECK(m_pd3dDevice->CreateTexture3D(&desc,NULL,&inscatter_textures[i]));
		V_CHECK(m_pd3dDevice->CreateTexture3D(&desc,NULL,&skylight_textures[i]));

		V_CHECK(m_pd3dDevice->CreateShaderResourceView(loss_textures[i],NULL,&loss_textures_SRV[i]));
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(inscatter_textures[i],NULL,&insc_textures_SRV[i]));
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(skylight_textures[i],NULL,&skyl_textures_SRV[i]));
	}
	if(loss_2d)
	{
		loss_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		loss_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
	if(inscatter_2d)
	{
		inscatter_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		inscatter_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
	if(skylight_2d)
	{
		skylight_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		skylight_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
}

void SimulSkyRendererDX1x::FillFadeTex(int texture_index,int texel_index,int num_texels,
						const simul::sky::float4 *loss_float4_array,
						const simul::sky::float4 *insc_float4_array,
						const simul::sky::float4 *skyl_float4_array)
{
	MapFade(texture_index);
	int slice_size	=numFadeElevations*numAltitudes;
	int end_slice	=(texel_index+num_texels-1)/(slice_size);
	int end_row		=(texel_index+num_texels-1)/numAltitudes-end_slice*numFadeElevations;
		
	int row_width	=loss_texture_mapped.RowPitch/sizeof(simul::sky::float4);
	int row_skip	=row_width-numAltitudes;
	int slice_width	=loss_texture_mapped.DepthPitch/sizeof(simul::sky::float4);
	int slice_skip	=slice_width-numFadeElevations*row_width;
	int end_texel=texel_index+num_texels;
	int last_row=0,last_slice=0;
	while(texel_index<end_texel)
	{
		int num_left=end_texel-texel_index;
		int slice	=texel_index/slice_size;
		int row		=texel_index/numAltitudes-slice*numFadeElevations;
		int col		=texel_index-(row+slice*numFadeElevations)*numAltitudes;
		simul::sky::float4 *loss_ptr=(simul::sky::float4 *)(loss_texture_mapped.pData);
		simul::sky::float4 *insc_ptr=(simul::sky::float4 *)(insc_texture_mapped.pData);
		simul::sky::float4 *skyl_ptr=(simul::sky::float4 *)(skyl_texture_mapped.pData);
		loss_ptr+=slice*slice_width+row*row_width+col;
		insc_ptr+=slice*slice_width+row*row_width+col;
		skyl_ptr+=slice*slice_width+row*row_width+col;
		if(slice!=end_slice)
		{
			num_left=(slice+1)*slice_size-texel_index;
			if(row!=numFadeElevations-1)
				num_left=slice*slice_size+(row+1)*numAltitudes-texel_index;
		}
		else
			if(row!=end_row)
				num_left=slice*slice_size+(row+1)*numAltitudes-texel_index;
		memcpy(loss_ptr,loss_float4_array	,num_left*sizeof(simul::sky::float4));
		memcpy(insc_ptr,insc_float4_array	,num_left*sizeof(simul::sky::float4));
		memcpy(skyl_ptr,skyl_float4_array	,num_left*sizeof(simul::sky::float4));
		loss_float4_array+=num_left;
		insc_float4_array+=num_left;
		skyl_float4_array+=num_left;
		last_row=row;
		last_slice=slice;
		texel_index+=num_left;
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

	colour				=m_pSkyEffect->GetVariableByName("colour")->AsVector();
	m_hTechniqueSun		=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniqueFlare	=m_pSkyEffect->GetTechniqueByName("simul_flare");
	m_hTechniquePlanet	=m_pSkyEffect->GetTechniqueByName("simul_planet");
	flareTexture		=m_pSkyEffect->GetVariableByName("flareTexture")->AsShaderResource();

	inscTexture			=m_pSkyEffect->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture			=m_pSkyEffect->GetVariableByName("skylTexture")->AsShaderResource();

	fadeTexture1		=m_pSkyEffect->GetVariableByName("fadeTexture1")->AsShaderResource();
	fadeTexture2		=m_pSkyEffect->GetVariableByName("fadeTexture2")->AsShaderResource();

	m_hTechniqueQuery	=m_pSkyEffect->GetTechniqueByName("simul_query");

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

void SimulSkyRendererDX1x::RenderSun()
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	float max_bright=std::max(std::max(sunlight.x,sunlight.y),sunlight.z);
	if(max_bright>1.f)
	{
		sunlight*=1.f/max_bright;
		sunlight.w=max_bright;
	}
	colour->SetFloatVector(sunlight);
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
	RenderAngledQuad(m_pd3dDevice,sun_dir,y_vertical,sun_angular_size,m_pSkyEffect,m_hTechniqueSun,view,proj,sun_dir);
	// Start the query
/*d3dQuery->Begin();
	hr=RenderAngledQuad(sun_dir,sun_angular_size);
	// End the query, get the data
    d3dQuery->End();

    // Loop until the data becomes available
    UINT64 pixelsVisible = 0;
    
    while (d3dQuery->GetData((void *) &pixelsVisible,sizeof(UINT64),0) == S_FALSE);*/
}

bool SimulSkyRendererDX1x::RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting)
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	if(do_lighting)
		ApplyPass(m_hTechniquePlanet->GetPassByIndex(0));
	else
		ApplyPass(m_hTechniqueFlare->GetPassByIndex(0));
	flareTexture->SetResource((ID3D1xShaderResourceView*)tex);
	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();
	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);
	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(y_vertical?planet_dir4.y:planet_dir4.z);
	planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);
	D3DXVECTOR4 planet_dir(dir);
	//m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&planet_colour));
	ID3D1xEffectVectorVariable*	colour=m_pSkyEffect->GetVariableByName("colour")->AsVector();
	colour->SetFloatVector((const float *)(&planet_colour));
	
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	RenderAngledQuad(m_pd3dDevice,planet_dir,y_vertical,rad,m_pSkyEffect,m_hTechniquePlanet,view,proj
		,sun_dir);
	return true;
}

bool SimulSkyRendererDX1x::RenderFlare(float exposure)
{
HRESULT hr=S_OK;
/*	if(!m_pSkyEffect)
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
	colour->SetFloatVector(sunlight);
	//m_pSkyEffect->SetTechnique(m_hTechniqueFlare);
	//flareTexture->SetResource(flare_texture_SRV);
	float sun_angular_size=3.14159f/180.f/2.f;
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
//	hr=RenderAngledQuad(sun_dir,sun_angular_size*20.f*magnitude);
	RenderAngledQuad(m_pd3dDevice,sun_dir,y_vertical,sun_angular_size*20.f*magnitude,m_pSkyEffect,m_hTechniqueFlare,view,proj,sun_dir);*/
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::Render2DFades()
{
	if(!m_hTechniqueFade3DTo2D)
		return false;
	if(!loss_2d||!inscatter_2d||!skylight_2d)
		return false;
	HRESULT hr;
	// Clear the screen to black:
	float clearColor[4]={0.0,0.0,0.0,0.0};
	skyInterp->SetFloat(skyKeyframer->GetInterpolation());
	altitudeTexCoord->SetFloat(skyKeyframer->GetAltitudeTexCoord());
	{
		V_CHECK(fadeTexture1->SetResource(loss_textures_SRV[0]));
		V_CHECK(fadeTexture2->SetResource(loss_textures_SRV[1]));
		V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		loss_2d->SetTargetWidthAndHeight(numFadeDistances,numFadeElevations);
		loss_2d->Activate();
		
		m_pImmediateContext->ClearRenderTargetView(loss_2d->m_pHDRRenderTarget,clearColor);
		if(loss_2d->m_pBufferDepthSurface)
			m_pImmediateContext->ClearDepthStencilView(loss_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
		loss_2d->RenderBufferToCurrentTarget();
		loss_2d->Deactivate();
	}
	{
		V_CHECK(fadeTexture1->SetResource(insc_textures_SRV[0]));
		V_CHECK(fadeTexture2->SetResource(insc_textures_SRV[1]));
		V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		
		inscatter_2d->SetTargetWidthAndHeight(numFadeDistances,numFadeElevations);
		inscatter_2d->Activate();
			
		m_pImmediateContext->ClearRenderTargetView(inscatter_2d->m_pHDRRenderTarget,clearColor);
		if(inscatter_2d->m_pBufferDepthSurface)
			m_pImmediateContext->ClearDepthStencilView(inscatter_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|
			D3D1x_CLEAR_STENCIL, 1.f, 0);
		inscatter_2d->RenderBufferToCurrentTarget();
		inscatter_2d->Deactivate();
	}
	{
		V_CHECK(fadeTexture1->SetResource(skyl_textures_SRV[0]));
		V_CHECK(fadeTexture2->SetResource(skyl_textures_SRV[1]));
		V_CHECK(ApplyPass(m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		
		skylight_2d->SetTargetWidthAndHeight(numFadeDistances,numFadeElevations);
		skylight_2d->Activate();
			
		m_pImmediateContext->ClearRenderTargetView(skylight_2d->m_pHDRRenderTarget,clearColor);
		if(skylight_2d->m_pBufferDepthSurface)
			m_pImmediateContext->ClearDepthStencilView(skylight_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|
			D3D1x_CLEAR_STENCIL, 1.f, 0);
		skylight_2d->RenderBufferToCurrentTarget();
		skylight_2d->Deactivate();
	}
	
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

	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
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
	inscTexture->SetResource(inscatter_2d->hdr_buffer_texture_SRV);
	skylTexture->SetResource(skylight_2d->hdr_buffer_texture_SRV);

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

	D3DXMATRIX ident,trans;
	D3DXMatrixIdentity(&ident);
    D3DXMatrixOrthoLH(&ident,(float)width,-(float)h,-100.f,100.f);
//	D3DXVECTOR3 center = D3DXVECTOR3(width/2, h/2, 0);
	//static float x=1,y=1;
	//x=2.f/(float)width;
	//y=2.f/(float)h;
	D3DXMatrixTranslation(&trans,-1.f,1.f,0);
	D3DXMatrixTranspose(&trans,&trans);
	D3DXMatrixMultiply(&ident,&trans,&ident);
	ID3D1xEffectMatrixVariable*	worldViewProj=m_pSkyEffect->GetVariableByName("worldViewProj")->AsMatrix();
	worldViewProj->SetMatrix(ident);

	ID3D1xEffectTechnique*	techniqueShowSky	=m_pSkyEffect->GetTechniqueByName("simul_show_sky_texture");

	ID3D1xEffectTechnique*	techniqueShowFade	=m_pSkyEffect->GetTechniqueByName("simul_show_fade_texture");
	ID3D1xEffectShaderResourceVariable*	inscTexture	=m_pSkyEffect->GetVariableByName("inscTexture")->AsShaderResource();

	inscTexture->SetResource(inscatter_2d->hdr_buffer_texture_SRV);
	RenderTexture(m_pd3dDevice,8,8,size,size,techniqueShowSky);
	inscTexture->SetResource(loss_2d->hdr_buffer_texture_SRV);
	RenderTexture(m_pd3dDevice,8,16+size,size,size,techniqueShowSky);
	int x=16+size;
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		altitudeTexCoord->SetFloat(atc);
		fadeTexture1->SetResource(insc_textures_SRV[0]);
		RenderTexture(m_pd3dDevice,x+16+0*(size+8)	,(i)*(size+8)+8,size,size,techniqueShowFade);
		fadeTexture1->SetResource(insc_textures_SRV[1]);
		RenderTexture(m_pd3dDevice,x+16+1*(size+8)	,(i)*(size+8)+8,size,size,techniqueShowFade);
		fadeTexture1->SetResource(loss_textures_SRV[0]);
		RenderTexture(m_pd3dDevice,x+16+2*(size+8)	,(i)*(size+8)+8,size,size,techniqueShowFade);
		fadeTexture1->SetResource(loss_textures_SRV[1]);
		RenderTexture(m_pd3dDevice,x+16+3*(size+8)	,(i)*(size+8)+8,size,size,techniqueShowFade);
	}
	
	return (hr==S_OK);
}

void SimulSkyRendererDX1x::DrawCubemap(ID3D1xShaderResourceView *m_pCubeEnvMapSRV)
{
	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixIdentity(&world);
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);
	simul::math::Vector3 pos((const float*)cam_pos);
	float size_req=tan_x*.1f;
	float d=2.f*size/size_req;
	simul::math::Vector3 offs0(-.8f*(tan_x-size_req)*d,.8f*(tan_y-size_req)*d,-d);
	simul::math::Vector3 offs;
	Multiply3(offs,*((const simul::math::Matrix4x4*)(const float*)view),offs0);
	pos+=offs;
	world._41=pos.x;
	world._42=pos.y;
	world._43=pos.z;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
	ID3D1xEffectTechnique*				tech		=m_pSkyEffect->GetTechniqueByName("draw_cubemap");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pSkyEffect->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(tech->GetPassByIndex(0));
	DrawCube();
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
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);

	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pImmediateContext->Draw(36,0);
}

void SimulSkyRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulSkyRendererDX1x::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void* *s)
{
	if(loss_2d)
		*l1=(void*)loss_2d->hdr_buffer_texture_SRV;
	else
		*l1=NULL;
	if(inscatter_2d)
		*i1=(void*)inscatter_2d->hdr_buffer_texture_SRV;
	else
		*l1=NULL;
	if(skylight_2d)
		*s=(void*)skylight_2d->hdr_buffer_texture_SRV;
	else
		*s=NULL;
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
	simul::dx11::UtilityRenderer::DrawLines((simul::dx11::UtilityRenderer::VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulSkyRendererDX1x::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	simul::dx11::UtilityRenderer::PrintAt3dPos(p,text,colr,offsetx,offsety);
}
