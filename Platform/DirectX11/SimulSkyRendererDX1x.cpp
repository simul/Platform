
// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRendererDX1x.cpp A renderer for skies.
#define NOMINMAX


#include <tchar.h>
#include <d3d10_1.h>
#include <d3dx10.h>
#include <dxerr.h>
#include <string>
#include "SimulSkyRendererDX1x.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Camera/Camera.h"

using namespace simul::dx11;

SimulSkyRendererDX1x::SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem)
	:simul::sky::BaseSkyRenderer(sk,mem)
	,m_pd3dDevice(NULL)
	,m_pStarsVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,m_pStarsVertexBuffer(NULL)
	,m_pSkyEffect(NULL)
	,m_hTechniqueFade3DTo2D(NULL)
	,m_hTechniqueSun(NULL)
	,m_hTechniqueQuery(NULL)	
	,m_hTechniqueFlare(NULL)
	,m_hTechniquePlanet(NULL)
	,m_hTechniquePointStars(NULL)
	,flare_texture_SRV(NULL)
	,moon_texture_SRV(NULL)
	,loss_2d(NULL)
	,inscatter_2d(NULL)
	,overcast_2d(NULL)
	,skylight_2d(NULL)
	,d3dQuery(NULL)
	,cycle(0)
{
	SetCameraPosition(0,0,400.f);
	skyKeyframer->SetCalculateAllAltitudes(true);
	loss_2d		=new(memoryInterface) simul::dx11::Framebuffer(0,0);
	inscatter_2d=new(memoryInterface) simul::dx11::Framebuffer(0,0);
	overcast_2d	=new(memoryInterface) simul::dx11::Framebuffer(0,0);
	skylight_2d	=new(memoryInterface) simul::dx11::Framebuffer(0,0);
}

void SimulSkyRendererDX1x::SetStepsPerDay(unsigned steps)
{
	skyKeyframer->SetUniformKeyframes(steps);
}

void SimulSkyRendererDX1x::RestoreDeviceObjects( void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	D3D1x_QUERY_DESC qdesc=
	{
		D3D1x_QUERY_OCCLUSION,0
	};
    m_pd3dDevice->CreateQuery(&qdesc,&d3dQuery);
	HRESULT hr=S_OK;
	world.Identity();
	view.Identity();
	proj.Identity();
	gpuSkyGenerator.RestoreDeviceObjects(m_pd3dDevice);
	TextureStruct *loss[3],*insc[3],*skyl[3];
	for(int i=0;i<3;i++)
	{
		loss[i]=&loss_textures[i];
		insc[i]=&insc_textures[i];
		skyl[i]=&skyl_textures[i];
	}
	gpuSkyGenerator.SetDirectTargets(loss,insc,skyl,&light_table);
	RecompileShaders();

	flare_texture_SRV=simul::dx11::LoadTexture(m_pd3dDevice,"Sunburst.dds");

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
	if(overcast_2d)
	{
		overcast_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		overcast_2d->RestoreDeviceObjects(dev);
	}
	if(skylight_2d)
	{
		skylight_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		skylight_2d->RestoreDeviceObjects(dev);
	}
	illumination_fb.SetWidthAndHeight(64,numFadeElevations*4);
	SAFE_RELEASE(moon_texture_SRV);
	MoonTexture="Moon.png";
	moon_texture_SRV=simul::dx11::LoadTexture(m_pd3dDevice,MoonTexture.c_str());
	SetPlanetImage(moon_index,moon_texture_SRV);
	ClearIterators();
	
	// Stars vertex declaration
	{
		D3D1x_PASS_DESC PassDesc;
		m_hTechniquePointStars->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D1x_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32_FLOAT,			0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		SAFE_RELEASE(m_pStarsVtxDecl);
		hr=m_pd3dDevice->CreateInputLayout(decl,2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pStarsVtxDecl);
	}
	{
		SAFE_RELEASE(m_pStarsVertexBuffer);
	}
	earthShadowUniforms.RestoreDeviceObjects(m_pd3dDevice);
	skyConstants.RestoreDeviceObjects(m_pd3dDevice);
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
	if(overcast_2d)
	{
		overcast_2d->InvalidateDeviceObjects();
	}
	if(skylight_2d)
	{
		skylight_2d->InvalidateDeviceObjects();
	}
	illumination_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pVertexBuffer);

	SAFE_RELEASE(flare_texture_SRV);
	SAFE_RELEASE(moon_texture_SRV);

	SAFE_RELEASE(m_pStarsVtxDecl);
	SAFE_RELEASE(m_pStarsVertexBuffer);
	for(int i=0;i<3;i++)
	{
		loss_textures[i].release();
		insc_textures[i].release();
		skyl_textures[i].release();
	}
	light_table.release();
	light_table_2d.release();
	// Set the stored texture sizes to zero, so the textures will be re-created.
	numFadeDistances=numFadeElevations=numAltitudes=0;
	SAFE_RELEASE(d3dQuery);
	earthShadowUniforms.InvalidateDeviceObjects();
	skyConstants.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
	::operator delete[](star_vertices,memoryInterface);
	star_vertices=NULL;
}

bool SimulSkyRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	::operator delete(loss_2d,memoryInterface);
	::operator delete(inscatter_2d,memoryInterface);
	::operator delete(overcast_2d,memoryInterface);
	::operator delete(skylight_2d,memoryInterface);
	return true;
}

SimulSkyRendererDX1x::~SimulSkyRendererDX1x()
{
	Destroy();
}

void SimulSkyRendererDX1x::MapFade(ID3D11DeviceContext *context,int s)
{
	loss_textures[(texture_cycle+s)%3].map(context);
	insc_textures[(texture_cycle+s)%3].map(context);
	skyl_textures[(texture_cycle+s)%3].map(context);
}

void SimulSkyRendererDX1x::UnmapFade(int s)
{
	loss_textures[(texture_cycle+s)%3].unmap();
	insc_textures[(texture_cycle+s)%3].unmap();
	skyl_textures[(texture_cycle+s)%3].unmap();
 }

float SimulSkyRendererDX1x::GetFadeInterp() const
{
	return skyKeyframer->GetInterpolation();
}

void SimulSkyRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=skyKeyframer->GetTextureSizes();
	int num_dist=i.x;
	int num_elev=i.y;
	int num_alt=i.z;
	bool uav=gpuSkyGenerator.GetEnabled()&&skyKeyframer->GetGpuSkyGenerator()==&gpuSkyGenerator;
	for(int i=0;i<3;i++)
	{
		loss_textures[i].ensureTexture3DSizeAndFormat(m_pd3dDevice,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
		insc_textures[i].ensureTexture3DSizeAndFormat(m_pd3dDevice,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
		skyl_textures[i].ensureTexture3DSizeAndFormat(m_pd3dDevice,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
	}
	light_table_2d.ensureTexture2DSizeAndFormat(m_pd3dDevice,num_alt*32,4,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	
	if(!num_dist||!num_elev||!num_alt)
		return;
	if(numFadeDistances==num_dist&&numFadeElevations==num_elev&&numAltitudes==num_alt)
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
	CreateFadeTextures();
}

void SimulSkyRendererDX1x::CreateFadeTextures()
{
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
	if(overcast_2d)
	{
		overcast_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		overcast_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
	if(skylight_2d)
	{
		skylight_2d->SetWidthAndHeight(numFadeDistances,numFadeElevations);
		skylight_2d->RestoreDeviceObjects(m_pd3dDevice);
	}
	illumination_fb.SetWidthAndHeight(128,numFadeElevations);
	illumination_fb.RestoreDeviceObjects(m_pd3dDevice);
}

void SimulSkyRendererDX1x::EnsureTexturesAreUpToDate(void *c)
{
	ID3D11DeviceContext *context=(ID3D11DeviceContext *)c;
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	
	sky::GpuSkyParameters p;
	sky::GpuSkyAtmosphereParameters a;
	sky::GpuSkyInfraredParameters ir;
	for(int i=0;i<3;i++)
	{
		skyKeyframer->GetGpuSkyParameters(p,a,ir,i);
		int cycled_index=(texture_cycle+i)%3;
		if(gpuSkyGenerator.GetEnabled())
			gpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
		else
			skyKeyframer->cpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
	}
	if(gpuSkyGenerator.GetEnabled())
		return;
	for(int i=0;i<3;i++)
	{
		bool reset=false;
		int cycled_index=(texture_cycle+i)%3;
		simul::sky::seq_texture_fill texture_fill=skyKeyframer->cpuSkyGenerator.GetSequentialFadeTextureFill(cycled_index,fade_texture_iterator[i]);
		if(reset)
		{
			fade_texture_iterator[i].texel_index=0;
		}
		if(texture_fill.num_texels)
		{
			MapFade(context,i);
			FillFadeTex(context,i,texture_fill.texel_index,texture_fill.num_texels,(const simul::sky::float4*)texture_fill.float_array_1,(const simul::sky::float4*)texture_fill.float_array_2,(const simul::sky::float4*)texture_fill.float_array_3);
		}
		if(i!=2)
			UnmapFade(i);
	}
}

void SimulSkyRendererDX1x::CycleTexturesForward()
{
	cycle++;
	std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
	std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
	for(int i=0;i<3;i++)
		fade_texture_iterator[i].texture_index=i;
	for(int i=0;i<2;i++)
		UnmapFade(i);
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

void SimulSkyRendererDX1x::FillFadeTex(ID3D11DeviceContext *context,int texture_index,int texel_index,int num_texels,
						const simul::sky::float4 *loss_float4_array,
						const simul::sky::float4 *insc_float4_array,
						const simul::sky::float4 *skyl_float4_array)
{
	int slice_size	=numFadeElevations*numAltitudes;
	int end_slice	=(texel_index+num_texels-1)/(slice_size);
	int end_row		=(texel_index+num_texels-1)/numAltitudes-end_slice*numFadeElevations;
		
	int row_width	=loss_textures[(texture_cycle+texture_index)%3].mapped.RowPitch/sizeof(simul::sky::float4);
	int row_skip	=row_width-numAltitudes;
	int slice_width	=loss_textures[(texture_cycle+texture_index)%3].mapped.DepthPitch/sizeof(simul::sky::float4);
	int slice_skip	=slice_width-numFadeElevations*row_width;
	int end_texel=texel_index+num_texels;
	int last_row=0,last_slice=0;
	while(texel_index<end_texel)
	{
		int num_left=end_texel-texel_index;
		int slice	=texel_index/slice_size;
		int row		=texel_index/numAltitudes-slice*numFadeElevations;
		int col		=texel_index-(row+slice*numFadeElevations)*numAltitudes;
		simul::sky::float4 *loss_ptr=(simul::sky::float4 *)(loss_textures[(texture_cycle+texture_index)%3].mapped.pData);
		simul::sky::float4 *insc_ptr=(simul::sky::float4 *)(insc_textures[(texture_cycle+texture_index)%3].mapped.pData);
		simul::sky::float4 *skyl_ptr=(simul::sky::float4 *)(skyl_textures[(texture_cycle+texture_index)%3].mapped.pData);
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
		return;
	std::map<std::string,std::string> defines;
	defines["Z_VERTICAL"]="1";
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pSkyEffect,"simul_sky.fx",defines));

	m_hTechniqueFade3DTo2D		=m_pSkyEffect->GetTechniqueByName("simul_fade_3d_to_2d");

	m_hTechniqueSun				=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniquePointStars		=m_pSkyEffect->GetTechniqueByName("simul_stars");
	m_hTechniqueFlare			=m_pSkyEffect->GetTechniqueByName("simul_flare");
	m_hTechniquePlanet			=m_pSkyEffect->GetTechniqueByName("simul_planet");
	flareTexture				=m_pSkyEffect->GetVariableByName("flareTexture")->AsShaderResource();

	inscTexture					=m_pSkyEffect->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture					=m_pSkyEffect->GetVariableByName("skylTexture")->AsShaderResource();

	fadeTexture1				=m_pSkyEffect->GetVariableByName("fadeTexture1")->AsShaderResource();
	fadeTexture2				=m_pSkyEffect->GetVariableByName("fadeTexture2")->AsShaderResource();
	illuminationTexture			=m_pSkyEffect->GetVariableByName("illuminationTexture")->AsShaderResource();
	m_hTechniqueQuery			=m_pSkyEffect->GetTechniqueByName("simul_query");

	earthShadowUniforms.LinkToEffect(m_pSkyEffect,"EarthShadowUniforms");
	skyConstants.LinkToEffect(m_pSkyEffect,"SkyConstants");
	gpuSkyGenerator.RecompileShaders();
}
#include "Simul/Math/Pi.h"
float SimulSkyRendererDX1x::CalcSunOcclusion(float cloud_occlusion)
{
	sun_occlusion=cloud_occlusion;
	if(!m_hTechniqueQuery)
		return sun_occlusion;
//	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToSun());
	float sun_angular_radius=skyKeyframer->GetSkyInterface()->GetSunRadiusArcMinutes()/60.f*pi/180.f;

	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_radius);
/*
	// Start the query
	d3dQuery->Begin();
	RenderAngledQuad(sun_dir,sun_angular_radius);
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


void SimulSkyRendererDX1x::RenderSun(void *c,float exposure)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)c;
	float alt_km=0.001f*(cam_pos.z);
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular diameter of about 1/2 a degree,
	// which is pi/360 or 0.00873 radians (actually 0.0095), the angular area is 
	// equal to 2 pi/(1-cos(angular_radius)), or about  6.87×10−5 steradians;
	sunlight*=2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	float max_bright=std::max(std::max(sunlight.x,sunlight.y),sunlight.z);
	sunlight.w=1.0f/(max_bright*exposure);
	sunlight*=1.f-sun_occlusion;//pow(1.f-sun_occlusion,0.25f);
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
	SetConstantsForPlanet(skyConstants,view,proj,sun_dir,2.f*skyKeyframer->GetSkyInterface()->GetSunRadiusArcMinutes()/60.f*pi/180.f,sunlight,sun_dir);
	skyConstants.Apply(pContext);
	ApplyPass(pContext,m_hTechniqueSun->GetPassByIndex(0));
	UtilityRenderer::DrawQuad(pContext);

//	UtilityRenderer::RenderAngledQuad(context,sun_dir,sun_angular_radius*2.f,m_pSkyEffect,m_hTechniqueSun,view,proj,sun_dir);
	// Start the query
/*d3dQuery->Begin();
	hr=RenderAngledQuad(sun_dir,sun_angular_radius);
	// End the query, get the data
    d3dQuery->End();

    // Loop until the data becomes available
    UINT64 pixelsVisible = 0;
    
    while (d3dQuery->GetData((void *) &pixelsVisible,sizeof(UINT64),0) == S_FALSE);*/
}

void SimulSkyRendererDX1x::RenderPlanet(void *c,void* tex,float rad,const float *dir,const float *colr,bool do_lighting)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)c;
	float alt_km=0.001f*(cam_pos.z);
	flareTexture->SetResource((ID3D1xShaderResourceView*)tex);
	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();
	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);
	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(planet_dir4.z);
	//planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);
	D3DXVECTOR4 planet_dir(dir);
	//m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&planet_colour));
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
	SetConstantsForPlanet(skyConstants,view,proj,planet_dir,rad,planet_colour,sun_dir);
	skyConstants.Apply(pContext);
	
	ApplyPass(pContext,(do_lighting?m_hTechniquePlanet:m_hTechniqueFlare)->GetPassByIndex(0));
	UtilityRenderer::DrawQuad(pContext);
}

bool SimulSkyRendererDX1x::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
/*	if(!m_pSkyEffect)
		return (hr==S_OK);
	float magnitude=exposure*(1.f-sun_occlusion);
	float alt_km=0.001f*(cam_pos.z);
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
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
//	hr=RenderAngledQuad(sun_dir,sun_angular_radius*20.f*magnitude);
	RenderAngledQuad(m_pd3dDevice,sun_dir,sun_angular_radius*20.f*magnitude,m_pSkyEffect,m_hTechniqueFlare,view,proj,sun_dir);*/
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::Render2DFades(void *c)
{
	if(!m_hTechniqueFade3DTo2D)
		return false;
	if(!loss_2d||!inscatter_2d||!skylight_2d)
		return false;
	// First render the illumination buffer, to get earthShadow and overcast properties.
	ID3D11DeviceContext *context=(ID3D11DeviceContext *)c;
	RenderIlluminationBuffer(context);
	// Current keyframe properties:
	const simul::sky::SkyKeyframer::SkyKeyframe *K=skyKeyframer->GetInterpolatedKeyframe();
	// Clear the screen to black:
	static float clearColor[4]={0.0,0.0,0.0,0.0};
	skyConstants.skyInterp			=skyKeyframer->GetInterpolation();
	skyConstants.altitudeTexCoord	=skyKeyframer->GetAltitudeTexCoord();
	skyConstants.overcast			=skyKeyframer->GetSkyInterface()->GetOvercast();
	skyConstants.eyePosition		=cam_pos;
	skyConstants.cloudShadowRange	=sqrt(80.f/skyKeyframer->GetMaxDistanceKm());
	skyConstants.cycled_index		=texture_cycle%3;
	skyConstants.Apply(context);

	illuminationTexture->SetResource((ID3D11ShaderResourceView*)illumination_fb.GetColorTex());
	{
		V_CHECK(fadeTexture1->SetResource(loss_textures[(texture_cycle+0)%3].shaderResourceView));
		V_CHECK(fadeTexture2->SetResource(loss_textures[(texture_cycle+1)%3].shaderResourceView));
		V_CHECK(ApplyPass(context,m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		loss_2d->Activate(context);
			context->ClearRenderTargetView(loss_2d->m_pHDRRenderTarget,clearColor);
			if(loss_2d->m_pBufferDepthSurface)
				context->ClearDepthStencilView(loss_2d->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
			simul::dx11::UtilityRenderer::DrawQuad(context);
		loss_2d->Deactivate(context);
	}
	{
		V_CHECK(fadeTexture1->SetResource(insc_textures[(texture_cycle+0)%3].shaderResourceView));
		V_CHECK(fadeTexture2->SetResource(insc_textures[(texture_cycle+1)%3].shaderResourceView));
		V_CHECK(ApplyPass(context,m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		inscatter_2d->Activate(context);
			simul::dx11::UtilityRenderer::DrawQuad(context);
		inscatter_2d->Deactivate(context);
	}

	{
		V_CHECK(fadeTexture1->SetResource(skyl_textures[(texture_cycle+0)%3].shaderResourceView));
		V_CHECK(fadeTexture2->SetResource(skyl_textures[(texture_cycle+1)%3].shaderResourceView));
		//V_CHECK(ApplyPass(context,m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		V_CHECK(ApplyPass(context,m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
		skylight_2d->Activate(context);
			simul::dx11::UtilityRenderer::DrawQuad(context);
		skylight_2d->Deactivate(context);
	}
	ID3DX11EffectTechnique* hTechniqueOverc		=m_pSkyEffect->GetTechniqueByName("overcast_inscatter");
	// We will bake the overcast effect into the overcast_2d texture.
	{
		V_CHECK(inscTexture->SetResource((ID3D11ShaderResourceView*)inscatter_2d->GetColorTex()));
		V_CHECK(ApplyPass(context,hTechniqueOverc->GetPassByIndex(0)));
		overcast_2d->Activate(context);
			simul::dx11::UtilityRenderer::DrawQuad(context);
		overcast_2d->Deactivate(context);
	}
	// light_table - using compute.
	{
		simul::dx11::setTexture(m_pSkyEffect				,"sourceTexture"	,light_table.shaderResourceView);
		simul::dx11::setUnorderedAccessView(m_pSkyEffect	,"targetTexture"	,light_table_2d.unorderedAccessView);
		ID3DX11EffectTechnique* m_TechniqueLightTableInterp	=m_pSkyEffect->GetTechniqueByName("interp_light_table");
		V_CHECK(ApplyPass(context,m_TechniqueLightTableInterp->GetPassByIndex(0)));
		context->Dispatch(light_table_2d.width,light_table_2d.length,1);
		simul::dx11::setTexture(m_pSkyEffect				,"sourceTexture"	,(ID3D11ShaderResourceView*)NULL);
		simul::dx11::setUnorderedAccessView(m_pSkyEffect	,"targetTexture"	,NULL);
		V_CHECK(ApplyPass(context,m_TechniqueLightTableInterp->GetPassByIndex(0)));
	}
	V_CHECK(fadeTexture1->SetResource(NULL));
	V_CHECK(fadeTexture2->SetResource(NULL));
	V_CHECK(ApplyPass(context,m_hTechniqueFade3DTo2D->GetPassByIndex(0)));
	return true;
}

void SimulSkyRendererDX1x::RenderIlluminationBuffer(void *c)
{
	ID3D11DeviceContext *context=(ID3D11DeviceContext *)c;
	SetIlluminationConstants(earthShadowUniforms,skyConstants);
	earthShadowUniforms.Apply(context);
	skyConstants.Apply(context);
	// Clear the screen to black:
	static float clearColor[4]={0.0,1.0,0.0,1.0};
	{
		ID3DX11EffectTechnique *tech=m_pSkyEffect->GetTechniqueByName("illumination_buffer");
		ApplyPass(context,tech->GetPassByIndex(0));
		illumination_fb.Activate(context);
		context->ClearRenderTargetView(illumination_fb.m_pHDRRenderTarget,clearColor);
		//if(e.enable)
			simul::dx11::UtilityRenderer::DrawQuad(context);
		illumination_fb.Deactivate(context);
	}
}

void SimulSkyRendererDX1x::BuildStarsBuffer()
{
	SAFE_RELEASE(m_pStarsVertexBuffer);
	int current_num_stars=skyKeyframer->stars.GetNumStars();
	num_stars=current_num_stars;
	::operator delete[](star_vertices,memoryInterface);
	star_vertices=new(memoryInterface) StarVertext[num_stars];
	static float d=100.f;
	for(int i=0;i<num_stars;i++)
	{
		float ra=(float)skyKeyframer->stars.GetStar(i).ascension;
		float de=(float)skyKeyframer->stars.GetStar(i).declination;
		star_vertices[i].x=d*cos(de)*sin(ra);
		star_vertices[i].y=d*cos(de)*cos(ra);
		star_vertices[i].z=d*sin(de);
		star_vertices[i].b=(float)exp(-skyKeyframer->stars.GetStar(i).magnitude);
	}
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = star_vertices;
    InitData.SysMemPitch = sizeof(StarVertext);
	D3D11_BUFFER_DESC desc=
	{
        num_stars*sizeof(StarVertext),D3D11_USAGE_DEFAULT,D3D11_BIND_VERTEX_BUFFER,0,0
	};
	m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pStarsVertexBuffer);
}

bool SimulSkyRendererDX1x::RenderPointStars(void *context,float exposure)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1, tmp2,wvp;
	D3DXMatrixInverse(&tmp1,NULL,(const D3DXMATRIX*)&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);

	GetSiderealTransform((float*)&world);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixInverse(&tmp2,NULL,(const D3DXMATRIX*)&world);
	D3DXMatrixMultiply(&tmp1,(const D3DXMATRIX*)&world,(const D3DXMATRIX*)&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,(const D3DXMATRIX*)&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	skyConstants.worldViewProj=(const float *)(&tmp2);
	hr=ApplyPass(pContext,m_hTechniquePointStars->GetPassByIndex(0));

	skyConstants.starBrightness	= exposure * skyKeyframer->GetCurrentStarBrightness();
	if(skyConstants.starBrightness<minimumStarBrightness)
		return true;
	if(skyConstants.starBrightness<minimumStarBrightness)
		return true;
	skyConstants.Apply(pContext);

	int current_num_stars=skyKeyframer->stars.GetNumStars();
	if(!star_vertices||current_num_stars!=num_stars)
	{
		BuildStarsBuffer();
	}

	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetInputLayout( m_pStarsVtxDecl );
	UINT stride = sizeof(StarVertext);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;

	pContext->IASetVertexBuffers(	0,						// the first input slot for binding
									1,						// the number of buffers in the array
									&m_pStarsVertexBuffer,	// the array of vertex buffers
									&stride,				// array of stride values, one for each buffer
									&offset );				// array of offset values, one for each buffer

	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pContext->Draw(num_stars,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
	return true;
}

bool SimulSkyRendererDX1x::Render(void *context,bool blend)
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::RenderFades(void *c,int x0,int y0,int width,int height)
{
	int size=width/3;
	if(height/4<size)
		size=height/4;
	if(size<2)
		return false;
	int s							=size/numAltitudes-2;
	ID3D11DeviceContext *context	=(ID3D11DeviceContext *)c;

	ID3DX11EffectTechnique*	techniqueShowSky				=m_pSkyEffect->GetTechniqueByName("simul_show_sky_texture");
	ID3DX11EffectTechnique*	techniqueShowFade				=m_pSkyEffect->GetTechniqueByName("simul_show_fade_texture");
	ID3DX11EffectShaderResourceVariable*	inscTexture		=m_pSkyEffect->GetVariableByName("inscTexture")->AsShaderResource();

	ID3DX11EffectTechnique*	techniqueShowLightTable			=m_pSkyEffect->GetTechniqueByName("show_light_table");
	ID3DX11EffectTechnique*	techniqueShow2DLightTable		=m_pSkyEffect->GetTechniqueByName("show_2d_light_table");
	ID3DX11EffectTechnique*	techniqueShowIlluminationBuffer	=m_pSkyEffect->GetTechniqueByName("show_illumination_buffer");
	
	int y=y0+8;
	inscTexture->SetResource(loss_2d->buffer_texture_SRV);
	UtilityRenderer::DrawQuad2(context,x0+size+2,y,size,size,m_pSkyEffect,techniqueShowSky);
	y+=size+8;
	inscTexture->SetResource(inscatter_2d->buffer_texture_SRV);
	UtilityRenderer::DrawQuad2(context,x0+size+2,y,size,size,m_pSkyEffect,techniqueShowSky);
	inscTexture->SetResource(overcast_2d->buffer_texture_SRV);
	UtilityRenderer::DrawQuad2(context,x0,y,size,size,m_pSkyEffect,techniqueShowSky);
	y+=size+8;
	inscTexture->SetResource(skylight_2d->buffer_texture_SRV);
	UtilityRenderer::DrawQuad2(context,x0+size+2,y,size,size,m_pSkyEffect,techniqueShowSky);
	y+=size+8;
	inscTexture->SetResource(illumination_fb.buffer_texture_SRV);
	UtilityRenderer::DrawQuad2(context,x0+size+2,y,size,size,m_pSkyEffect,techniqueShowIlluminationBuffer);
	int x=16+size;
	y=y0+8;
	bool show_3=gpuSkyGenerator.GetEnabled()&&(skyKeyframer->GetGpuSkyGenerator()==&gpuSkyGenerator);

	for(int j=0;j<(show_3?3:2);j++)
	{
		int x=x0+9*j;
		fadeTexture1->SetResource(light_table.shaderResourceView);
		skyConstants.cycled_index=(texture_cycle+j)%3;
		skyConstants.Apply(context);
		UtilityRenderer::DrawQuad2(context,x	,y		,8,size	,m_pSkyEffect,techniqueShowLightTable);
	}
	simul::dx11::setTexture(m_pSkyEffect,"lightTable2DTexture",light_table_2d.shaderResourceView);
	UtilityRenderer::DrawQuad2(context,x0+9*4	,y,8,size	,m_pSkyEffect,techniqueShow2DLightTable);
	x0+=2*(size+8);
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		int x1=x0,x2=x0+s+8;
		int y=y0+i*(s+2);
		skyConstants.altitudeTexCoord=atc;
		skyConstants.Apply(context);
		for(int j=0;j<(show_3?3:2);j++)
		{
			int x=x0+(s+8)*j;
			fadeTexture1->SetResource(loss_textures[(texture_cycle+j)%3].shaderResourceView);
			UtilityRenderer::DrawQuad2(context,x	,y+8		,s,s	,m_pSkyEffect,techniqueShowFade);
			fadeTexture1->SetResource(insc_textures[(texture_cycle+j)%3].shaderResourceView);
			UtilityRenderer::DrawQuad2(context,x	,y+16+size	,s,s	,m_pSkyEffect,techniqueShowFade);
			fadeTexture1->SetResource(skyl_textures[(texture_cycle+j)%3].shaderResourceView);
			UtilityRenderer::DrawQuad2(context,x	,y+24+2*size,s,s	,m_pSkyEffect,techniqueShowFade);
		}
	}
	return true;
}

void SimulSkyRendererDX1x::DrawCubemap(void *context,ID3D1xShaderResourceView *m_pCubeEnvMapSRV,D3DXMATRIX view,D3DXMATRIX proj)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	D3DXMATRIX tmp1,tmp2,wvp;
	world.Identity();
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);
	simul::math::Vector3 pos((const float*)cam_pos);
	float size_req=tan_x*0.2f;
	static float size=5.f;
	float d=2.0f*size/size_req;
	simul::math::Vector3 offs0(-0.7f*(tan_x-size_req)*d,0.7f*(tan_y-size_req)*d,-d);
	simul::math::Vector3 offs;
	Multiply3(offs,*((const simul::math::Matrix4x4*)(const float*)view),offs0);
	pos+=offs;
	world._41=pos.x;
	world._42=pos.y;
	world._43=pos.z;
	camera::MakeWorldViewProjMatrix((float*)&wvp,world,view,proj);
	skyConstants.worldViewProj=&wvp._11;
	skyConstants.worldViewProj.transpose();
	skyConstants.Apply(pContext);
	ID3DX11EffectTechnique*				tech		=m_pSkyEffect->GetTechniqueByName("draw_cubemap");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pSkyEffect->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	UtilityRenderer::DrawCube(context);
}

void SimulSkyRendererDX1x::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
	view=v;
	proj=p;
}

void SimulSkyRendererDX1x::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void* *s,void* *o)
{
	if(loss_2d)
		*l1=(void*)loss_2d->buffer_texture_SRV;
	else
		*l1=NULL;
	if(inscatter_2d)
		*i1=(void*)inscatter_2d->buffer_texture_SRV;
	else
		*l1=NULL;
	if(skylight_2d)
		*s=(void*)skylight_2d->buffer_texture_SRV;
	else
		*s=NULL;
	if(overcast_2d)
		*o=(void*)overcast_2d->buffer_texture_SRV;
	else
		*o=NULL;
}

void *SimulSkyRendererDX1x::GetIlluminationTexture()
{
	return illumination_fb.buffer_texture_SRV;
}

void *SimulSkyRendererDX1x::GetLightTableTexture()
{
	return light_table_2d.shaderResourceView;
}


void SimulSkyRendererDX1x::DrawLines(void *context,Vertext *lines,int vertex_count,bool strip)
{
	simul::dx11::UtilityRenderer::DrawLines((ID3D11DeviceContext *)context,(VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulSkyRendererDX1x::PrintAt3dPos(void *context,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	simul::dx11::UtilityRenderer::PrintAt3dPos((ID3D11DeviceContext *)context,p,text,colr,offsetx,offsety);
}
