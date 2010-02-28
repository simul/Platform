// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulObjectRenderer.cpp A renderer for rain, snow etc.

#include "SimulObjectRenderer.h"

#ifdef XBOX
	#include <dxerr9.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr9.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif
#include "CreateDX9Effect.h"
#include <DXUT.h>
#include <DXUTmisc.h>
#include <DXUTenum.h>
#include <DXUTgui.h>
#include <DXUTsettingsDlg.h>

#include <SDKMesh.h>
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/Float4.h"
#include "Macros.h"

#pragma optimize("",off)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;
tstring maketstring(const char *txt)
{
	tstring str;
	const char *t=txt;
	while(*t)
	{
		str+=*t++;
	}
	return str;
}

//helper to set proper directories and find paths and such
static HRESULT	LoadMeshHelperFunction(CDXUTXFileMesh* mesh, const char *filename, IDirect3DDevice9* pd3dDevice)
{
	HRESULT hr;
	tstring path;
	tstring mediaFileDir;
	tstring::size_type lastSlashPos;
	tstring meshFile=maketstring(filename);

	path = meshFile;
	lastSlashPos = path.find_last_of(TEXT("\\"), path.size());
	if (lastSlashPos != path.npos)
		mediaFileDir = path.substr(0, lastSlashPos);
	else
		mediaFileDir = TEXT(".");

	if(path.empty())
		return E_FAIL;

	TCHAR currDir[512];
	GetCurrentDirectory(512,currDir);

	//note the mesh needs the current working directory to be set so that it
	//can properly load the textures
	SetCurrentDirectory(mediaFileDir.c_str());
	hr = mesh->Create( pd3dDevice, path.c_str() );
	SetCurrentDirectory(currDir);

	if( FAILED(hr) )
        return E_FAIL;
	return S_OK;
}

SimulObjectRenderer::SimulObjectRenderer(const char *file) :
	m_pd3dDevice(NULL),
	m_pEffect(NULL),
	mesh(NULL),
	sky_loss_texture_1(NULL),
	sky_loss_texture_2(NULL),
	sky_inscatter_texture_1(NULL),
	sky_inscatter_texture_2(NULL),
	skyInterface(NULL)
{
	filename=file;
}

SimulObjectRenderer::~SimulObjectRenderer()
{
	Destroy();
}

HRESULT SimulObjectRenderer::RestoreDeviceObjects( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	mesh				= new CDXUTXFileMesh();
	LoadMeshHelperFunction(mesh,filename.c_str(),m_pd3dDevice);
	if(mesh)
		mesh->RestoreDeviceObjects(m_pd3dDevice);

	HRESULT hr = CreateDX9Effect(m_pd3dDevice,m_pEffect,"Media\\HLSL\\simul_object.fx");
    if (FAILED(hr))
        return hr;

	m_hTechnique		=m_pEffect->GetTechniqueByName("simul_object");
	worldViewProj		=m_pEffect->GetParameterByName(NULL,"worldViewProj");
	worldparam			=m_pEffect->GetParameterByName(NULL,"world");

	skyLossTexture1		=m_pEffect->GetParameterByName(NULL,"skyLossTexture1");
	skyLossTexture2		=m_pEffect->GetParameterByName(NULL,"skyLossTexture2");
	skyInscatterTexture1=m_pEffect->GetParameterByName(NULL,"skyInscatterTexture1");
	skyInscatterTexture2=m_pEffect->GetParameterByName(NULL,"skyInscatterTexture2");

	cloudTexture1		=m_pEffect->GetParameterByName(NULL,"cloudTexture1");
	cloudTexture2		=m_pEffect->GetParameterByName(NULL,"cloudTexture2");
	
	hazeEccentricity	=m_pEffect->GetParameterByName(NULL,"hazeEccentricity");
	mieRayleighRatio	=m_pEffect->GetParameterByName(NULL,"mieRayleighRatio");
	lightDir			=m_pEffect->GetParameterByName(NULL,"lightDir");
	fadeInterp			=m_pEffect->GetParameterByName(NULL,"fadeInterp");
	cloudInterp			=m_pEffect->GetParameterByName(NULL,"cloudInterp");
	cloudCorner			=m_pEffect->GetParameterByName(NULL,"cloudCorner");
	cloudScales			=m_pEffect->GetParameterByName(NULL,"cloudScales");
	cloudLightVector			=m_pEffect->GetParameterByName(NULL,"cloudLightVector");

	eyePosition			=m_pEffect->GetParameterByName(NULL,"eyePosition");
	exposureParam		=m_pEffect->GetParameterByName(NULL,"exposure");
	sunlightColour		=m_pEffect->GetParameterByName(NULL,"sunlightColour");
	ambientColour		=m_pEffect->GetParameterByName(NULL,"ambientColour");
	return S_OK;
}


HRESULT SimulObjectRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pEffect)
        hr=m_pEffect->OnLostDevice();
	if(mesh)
		mesh->InvalidateDeviceObjects();
	SAFE_DELETE(mesh);
	SAFE_RELEASE(m_pEffect);
	sky_loss_texture_1=NULL;
	sky_loss_texture_2=NULL;
	skyInscatterTexture1=NULL;
	skyInscatterTexture2=NULL;
	return hr;
}

HRESULT SimulObjectRenderer::Destroy()
{
	HRESULT hr=S_OK;
	if(mesh)
		mesh->Destroy();
	SAFE_DELETE(mesh);
	SAFE_RELEASE(m_pEffect);
	sky_loss_texture_1=NULL;
	sky_loss_texture_2=NULL;
	skyInscatterTexture1=NULL;
	skyInscatterTexture2=NULL;
	return hr;
}

void SimulObjectRenderer::SetSkyInterface(simul::sky::SkyInterface *si)
{
	skyInterface=si;
}

void SimulObjectRenderer::SetCloudInterface(simul::clouds::CloudInterface *ci)
{
	cloudInterface=ci;
}

static const D3DXVECTOR4 *MakeD3DVector(const simul::sky::float4 v)
{
	static D3DXVECTOR4 x;
	x.x=v.x;
	x.y=v.y;
	x.z=v.z;
	x.w=v.w;
	return &x;
}

static const D3DXVECTOR4 *MakeD3DVector(const float *v)
{
	static D3DXVECTOR4 x;
	x.x=v[0];
	x.y=v[1];
	x.z=v[2];
	x.w=v[3];
	return &x;
}

HRESULT SimulObjectRenderer::Render()
{
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos(tmp1._41,tmp1._42,tmp1._43,0);
	simul::sky::float4 sun_dir(skyInterface->GetDirectionToLight());

	m_pEffect->SetTechnique(m_hTechnique);
	m_pEffect->SetFloat(hazeEccentricity,skyInterface->GetMieEccentricity());
	m_pEffect->SetFloat(fadeInterp,fade_interp);
	m_pEffect->SetFloat(cloudInterp,cloud_interp);
	m_pEffect->SetFloat(exposureParam,exposure);

	simul::math::Vector3 cloud_corner(-cloudInterface->GetCloudWidth()/2.f,
		-cloudInterface->GetCloudLength()/2.f,
		cloudInterface->GetCloudBaseZ());
	cloud_corner-=cloudInterface->GetWindOffset();
	simul::sky::float4 cloud_scales(cloudInterface->GetCloudWidth(),
		cloudInterface->GetCloudLength(),
		cloudInterface->GetCloudHeight(),0);
	simul::sky::float4 cloud_light_vector(sun_dir.x/cloudInterface->GetCloudWidth()/sun_dir.z,
		sun_dir.y/cloudInterface->GetCloudLength()/sun_dir.z,
		1.f,0);
	m_pEffect->SetVector(cloudCorner,MakeD3DVector(cloud_corner));
	m_pEffect->SetVector(cloudScales,MakeD3DVector(cloud_scales));
	m_pEffect->SetVector(cloudLightVector,MakeD3DVector(cloud_light_vector));

	static float ff=0.35f;
	m_pEffect->SetVector(sunlightColour,MakeD3DVector(ff*skyInterface->GetLocalIrradiance(cam_pos.y*0.001f)));
	m_pEffect->SetVector(ambientColour,MakeD3DVector(skyInterface->GetAmbientLight(cam_pos.y*0.001f)));
	m_pEffect->SetVector(mieRayleighRatio,MakeD3DVector(skyInterface->GetMieRayleighRatio()));
	// Because y is vertical in this renderer:
	std::swap(sun_dir.y,sun_dir.z);
	m_pEffect->SetVector(lightDir,MakeD3DVector(sun_dir));

	D3DXMatrixIdentity(&world);
	D3DXMatrixInverse(&tmp1,NULL,&view);
	m_pEffect->SetVector(eyePosition,&cam_pos);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	m_pEffect->SetMatrix(worldparam,(const D3DXMATRIX *)(&world));
	UINT passes=1;
	mesh->UseMeshMaterials(false);
	m_pEffect->SetTexture(skyLossTexture1	,sky_loss_texture_1);
	m_pEffect->SetTexture(skyLossTexture2	,sky_loss_texture_2);
	m_pEffect->SetTexture(skyInscatterTexture1,sky_inscatter_texture_1);
	m_pEffect->SetTexture(skyInscatterTexture2,sky_inscatter_texture_2);
	m_pEffect->SetTexture(cloudTexture1	,cloud_texture_1);
	m_pEffect->SetTexture(cloudTexture2	,cloud_texture_2);
	hr=m_pEffect->Begin( &passes, 0 );
	for(unsigned i=0;i<passes;++i)
	{
		hr=m_pEffect->BeginPass(i);
		mesh->Render(m_pd3dDevice);
		hr=m_pEffect->EndPass();
	}
	hr=m_pEffect->End();
	return hr;
}

void SimulObjectRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}