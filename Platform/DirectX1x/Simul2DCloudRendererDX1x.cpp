// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRenderer.cpp A renderer for 2D cloud layers.

#include "Simul2DCloudRendererdx1x.h"


	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	#define PIXBeginNamedEvent(a,b) D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent D3DPERF_EndEvent
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UINT;
	const bool big_endian=false;
	static DWORD default_texture_usage=D3DUSAGE_AUTOGENMIPMAP;
	static unsigned default_mip_levels=0;
	static unsigned bits[4]={	(unsigned)0x0F00,
								(unsigned)0x000F,
								(unsigned)0x00F0,
								(unsigned)0xF000};
	static unsigned bits8[4]={	(unsigned)0x00FF0000,
								(unsigned)0x000000FF,
								(unsigned)0x0000FF00,
								(unsigned)0xFF000000};
	static D3DPOOL default_d3d_pool=D3DPOOL_MANAGED;

#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX1x.h"
//
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;
static float cloud_interp=0.f;
static float interp_step_time=.2f;
static float interp_time_1=0.f;
static simul::math::Vector3 wind_vector(20,20,0);
static simul::math::Vector3 next_sun_direction;
unsigned scale=5;
struct float2
{
	float x,y;
	float2()
	{}
	float2(float X,float Y)
	{
		x=X;
		y=Y;
	}
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
	}
};
struct float3
{
	float x,y,z;
	float3()
	{}
	float3(float X,float Y,float Z)
	{
		x=X;
		y=Y;
		z=Z;
	}
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
		z=f[2];
	}
};

struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};

#define MAX_VERTICES (5000)

static void SetBits4()
{
	simul::clouds::TextureGenerator::SetBits(bits[0],bits[1],bits[2],bits[3],2,big_endian);
}
static void SetBits8()
{
	simul::clouds::TextureGenerator::SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
}


Simul2DCloudRenderer::Simul2DCloudRenderer() :
	skyInterface(NULL),
	fadeTableInterface(NULL),
	cloudInterface(NULL),
	m_pd3dDevice(NULL),
	m_pVtxDecl(NULL),
	m_pCloudEffect(NULL),
	noise_texture(NULL),
	image_texture(NULL),
	cloud_texel_index(0),
	texture_scale(0.25f)
{
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	cloudNode=new simul::clouds::FastCloudNode;
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);
	cloudInterface=cloudNode.get();
	{
		cloudInterface->SetThinLayer(true);

		cloudInterface->SetGridLength(64);
		cloudInterface->SetGridWidth(64);
		cloudInterface->SetGridHeight(1);

		cloudInterface->SetCloudBaseZ(20000.f);
		cloudInterface->SetCloudWidth(240000.f);
		cloudInterface->SetCloudLength(240000.f);
		cloudInterface->SetCloudHeight(500.f);
		cloudInterface->SetOpticalDensity(.5f);
		cloudInterface->SetHumidity(.45f);

		cloudInterface->SetExtinction(0.00f);
		cloudInterface->SetLightResponse(1.f);
		cloudInterface->SetSecondaryLightResponse(1.f);
		cloudInterface->SetAmbientLightResponse(1.f);
		cloudInterface->SetNoiseResolution(4);
		cloudInterface->SetNoiseOctaves(3);
		cloudInterface->SetNoisePersistence(0.5f);
	}
	cloudNode->SetDiffusivity(.5f);

	helper=new simul::clouds::Cloud2DGeometryHelper();
	helper->SetYVertical(true);
	helper->Initialize(24,320000.f);
	helper->SetGrid(6,9);
	helper->SetCurvedEarth(true);
	helper->SetEffectiveEarthRadius(0.f);
	
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
}

void Simul2DCloudRenderer::SetSkyInterface(simul::sky::SkyInterface *si)
{
	skyInterface=si;
}

void Simul2DCloudRenderer::SetFadeTableInterface(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
}

HRESULT Simul2DCloudRenderer::Create( ID3D10Device* dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	return hr;
}
struct Vertex2D_t
{
    float3 position;
    float2 texCoords;
    float3 loss;
    float3 inscatter;
    float2 texCoordNoise;
	float2 imageCoords;
};
Vertex2D_t *vertices=NULL;

HRESULT Simul2DCloudRenderer::RestoreDeviceObjects( ID3D10Device* dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0, 20	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0, 32	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,	44	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		{0,	52	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,4},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	V_RETURN(CreateNoiseTexture());
	V_RETURN(CreateImageTexture());
	V_RETURN(CreateCloudTextures());
	V_RETURN(FillInCloudTextures());
	V_RETURN(CreateCloudEffect());

	m_hTechniqueCloud	=m_pCloudEffect->GetTechniqueByName("simul_clouds_2d");

	worldViewProj		=m_pCloudEffect->GetVariableByName("worldViewProj");
	eyePosition			=m_pCloudEffect->GetVariableByName("eyePosition");
	lightResponse		=m_pCloudEffect->GetVariableByName("lightResponse");
	lightDir			=m_pCloudEffect->GetVariableByName("lightDir");
	skylightColour		=m_pCloudEffect->GetVariableByName("ambientColour");
	sunlightColour		=m_pCloudEffect->GetVariableByName("sunlightColour");
	fractalScale		=m_pCloudEffect->GetVariableByName("fractalScale");
	interp				=m_pCloudEffect->GetVariableByName("interp");

	layerDensity		=m_pCloudEffect->GetVariableByName("layerDensity");
	imageEffect			=m_pCloudEffect->GetVariableByName("imageEffect");

	cloudDensity1		=m_pCloudEffect->GetVariableByName("cloudDensity1");
	cloudDensity2		=m_pCloudEffect->GetVariableByName("cloudDensity2");
	noiseTexture		=m_pCloudEffect->GetVariableByName("noiseTexture");
	imageTexture		=m_pCloudEffect->GetVariableByName("imageTexture");

	return hr;
}

HRESULT Simul2DCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(image_texture);
	return hr;
}

HRESULT Simul2DCloudRenderer::Destroy()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pVtxDecl);
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(image_texture);
	return hr;
}

Simul2DCloudRenderer::~Simul2DCloudRenderer()
{
	Destroy();
}

HRESULT Simul2DCloudRenderer::CreateNoiseTexture()
{
	HRESULT hr=S_OK;
	int size=512;
	SAFE_RELEASE(noise_texture);
	// NOTE: We specify ONE mipmap for this texture, NOT ZERO. If we use zero, that means
	// automatically generate mipmaps.
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,DXGI_FORMAT_R8G8B8A8_UINT,D3DPOOL_MANAGED,&noise_texture)))
		return hr;
	D3D10_MAPPED_TEXTURE2D mapped={0};
	if(FAILED(hr=noise_texture->Map(0,&mapped,NULL,NULL)))
		return hr;
	SetBits8();
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(mapped.pData),size,16,8,0.8f);
	hr=noise_texture->Unmap(0);
	//noise_texture->GenerateMipSubLevels();
	return hr;
}

HRESULT Simul2DCloudRenderer::CreateImageTexture()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(image_texture);
	if(FAILED(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/Cirrus2.jpg",&image_texture)))
		return hr;
	return hr;
}

HRESULT Simul2DCloudRenderer::UpdateCloudTexture()
{
	HRESULT hr=S_OK;
	unsigned	w=cloudInterface->GetGridWidth(),
				h=cloudInterface->GetGridLength();
	unsigned total_texels=w*h;
	unsigned next_index=(unsigned)((float)(total_texels*(3.3f*(cloud_interp-.6f))));
	unsigned texels=next_index-cloud_texel_index;
	SetBits8();
	if(cloud_interp-.6f>0.f)
	{
		D3D10_MAPPED_TEXTURE2D mapped={0};
		if(FAILED(hr=cloud_textures[2]->Map(0,&mapped,NULL,NULL)))
			return hr;
		// RGBA bit-shift is 12,8,4,0
		// ARGB however, is 8,0,4,12
		simul::clouds::TextureGenerator::Partial2DCloudTextureFill(
			cloudInterface,cloud_texel_index,texels,(unsigned char *)(mapped.pData));
		hr=cloud_textures[2]->Unmap(0);
	}
	cloud_texel_index+=texels;
	return hr;
}

HRESULT Simul2DCloudRenderer::CreateCloudTextures()
{
	HRESULT hr=S_OK;
	unsigned w=cloudNode->GetGridWidth(),l=cloudNode->GetGridLength();
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,w,l,1,0,cloud_tex_format,default_d3d_pool,&cloud_textures[i])))
			return hr;
	}
	return hr;
}

HRESULT Simul2DCloudRenderer::FillInCloudTextures()
{
	if(!skyInterface)
		return S_OK;
	HRESULT hr=S_OK;
	float current_hour=skyInterface->GetHourOfTheDay();
	if(!interp_time_1)
		interp_time_1=current_hour;
	cloud_interp=(current_hour-interp_time_1)/interp_step_time;
	while(cloud_interp>1.f)
	{
		interp_time_1+=interp_step_time;
		cloud_interp=(current_hour-interp_time_1)/interp_step_time;
	}
	while(cloud_interp<0.f)
	{
		interp_time_1-=interp_step_time;
		cloud_interp=(current_hour-interp_time_1)/interp_step_time;
	}
	cloudInterface->Generate();
	for(int i=0;i<2;i++)
	{
		D3D10_MAPPED_TEXTURE2D mapped={0};
		if(FAILED(hr=cloud_textures[i]->Map(0,&mapped,NULL,NULL)))
			return hr;
		// RGBA bit-shift is 12,8,4,0
		// ARGB however, is 8,0,4,12
		SetBits8();
		
		skyInterface->SetHourOfTheDay(interp_time_1+i*interp_step_time);
		cloudInterface->ReLight(simul::math::Vector3(skyInterface->GetDirectionToSun()));

		if(!simul::clouds::TextureGenerator::Make2DCloudTexture(cloudInterface,(unsigned char *)(mapped.pData)))
			return S_FALSE;
		hr=cloud_textures[i]->Unmap(0);
	}
	skyInterface->SetHourOfTheDay(interp_time_1+2*interp_step_time);
	next_sun_direction=simul::math::Vector3(skyInterface->GetDirectionToSun());
	cloudInterface->SetLightDirection(next_sun_direction);
	skyInterface->SetHourOfTheDay(current_hour);
	return hr;
}


HRESULT Simul2DCloudRenderer::CreateCloudEffect()
{
	return CreateEffect(m_pd3dDevice,m_pCloudEffect,"media\\HLSL\\simul_clouds_2d.fx");
}
void Simul2DCloudRenderer::Update(float dt)
{
	HRESULT hr=S_OK;
	if(!cloud_textures[2])
		return;
	if(!cloudInterface)
		return;
	float current_hour=skyInterface->GetHourOfTheDay();
	if(!interp_time_1)
		interp_time_1=current_hour;
	cloud_interp=(current_hour-interp_time_1)/interp_step_time;
	if(!cloudInterface->UpdatePartial(1.7f*cloud_interp))
	{
		UpdateCloudTexture();
	}
	if(cloud_interp>1.f)
	{
		cloud_texel_index=0;
		while(cloud_interp>1.f)
		{
			interp_time_1+=interp_step_time;
			cloud_interp=(current_hour-interp_time_1)/interp_step_time;
		}

		std::swap(cloud_textures[0],cloud_textures[1]);
		std::swap(cloud_textures[1],cloud_textures[2]);

		// Get the sun direction for the next time step:
		skyInterface->SetHourOfTheDay(interp_time_1+2*interp_step_time);
		next_sun_direction=simul::math::Vector3(skyInterface->GetDirectionToSun());
		cloudInterface->SetLightDirection(next_sun_direction);
		skyInterface->SetHourOfTheDay(current_hour);
		cloudInterface->StartMarching();
	}
	while(cloud_interp<0.f)
	{
		cloud_texel_index=0;
		interp_time_1-=interp_step_time;
		cloud_interp=(current_hour-interp_time_1)/interp_step_time;
	}
		
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	simul::math::AddFloatTimesVector(wind_offset,dt,wind_vector);
	cloudInterface->SetWindOffset(wind_offset);
}
HRESULT Simul2DCloudRenderer::Render()
{
	//IDirect3DStateBlock9* pStateBlock = NULL;
//pd3dDevice->CreateStateBlock( D3DSBT_ALL, &pStateBlock );
	HRESULT hr=S_OK;

	RenderCloudsToBuffer();

	return hr;
}

HRESULT Simul2DCloudRenderer::RenderCloudsToBuffer()
{
	PIXBeginNamedEvent(0, "Render Clouds Layers");
//    IDirect3DStateBlock9* m_pStateBlock;
	HRESULT hr;
	if(!vertices)
		vertices=new Vertex2D_t[MAX_VERTICES];
	static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

	m_pCloudEffect->SetTechnique( m_hTechniqueCloud );


	m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
	m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
	m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);
	m_pCloudEffect->SetTexture(imageTexture					,image_texture);

	// Mess with the proj matrix to extend the far clipping plane:
	// According to the documentation for D3DXMatrixPerspectiveFovLH:
	// proj._33=zf/(zf-zn)  = 1/(1-zn/zf)
	// proj._43=-zn*zf/(zf-zn)
	// so proj._43/proj._33=-zn.

	float zNear=-proj._43/proj._33;
	float zFar=helper->GetMaxCloudDistance()*1.1f;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
		
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pCloudEffect->SetMatrix(worldViewProj, &tmp1);
	D3DXMatrixIdentity(&tmp1);
	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	simul::sky::float4 view_km=(const float*)cam_pos;
	helper->Update((const float*)cam_pos,cloudInterface->GetWindOffset(),view_dir,up);
	view_km*=0.001f;
	float alt_km=cloudInterface->GetCloudBaseZ()*0.001f;
static float light_mult=.05f;
	simul::sky::float4 light_response(	light_mult*(cloudInterface->GetLightResponse()+cloudInterface->GetSecondaryLightResponse()),
										light_mult*cloudInterface->GetAnisotropicLightResponse(),
										0,0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToSun();
//	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 sky_light_colour=skyInterface->GetSkyColour(simul::sky::float4(0,0,1,0),alt_km);
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);

	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->Set2DNoiseTexturing(-0.8f,2.f);
	helper->MakeGeometry(cloudInterface);
	helper->CalcInscatterFactors(cloudInterface,skyInterface,fadeTableInterface);
	float image_scale=1.f/texture_scale;
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	// Make the angular inscatter multipliers:
	unsigned el_start,el_end,az_start,az_end;
	helper->GetCurrentGrid(el_start,el_end,az_start,az_end);
static float image_effect=0.5f;
	D3DXVECTOR4 interp_vec(cloud_interp,1.f-cloud_interp,0,0);
	m_pCloudEffect->SetVector	(interp				,(D3DXVECTOR4*)(&interp_vec));
	m_pCloudEffect->SetVector	(eyePosition		,&cam_pos);
	m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
	m_pCloudEffect->SetVector	(lightDir			,(D3DXVECTOR4*)(&sun_dir));
	m_pCloudEffect->SetVector	(skylightColour		,(D3DXVECTOR4*)(&sky_light_colour));
	m_pCloudEffect->SetVector	(sunlightColour		,(D3DXVECTOR4*)(&sunlight));
	m_pCloudEffect->SetVector	(fractalScale		,(D3DXVECTOR4*)(&fractal_scales));

	m_pCloudEffect->AsScalar()->SetFloat	(layerDensity		,1.f-exp(-cloudInterface->GetCloudHeight()*0.001f*cloudInterface->GetOpticalDensity()));
	m_pCloudEffect->AsScalar()->SetFloat	(imageEffect		,image_effect);
	int startv=0;
	int v=0;
	hr=m_pd3dDevice->IASetInputLayout( m_pVtxDecl );
	UINT passes=1;
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
	// blending for alpha:
	m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
	m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);

	startv=v;
	simul::math::Vector3 pos;
	simul::sky::float4 loss2,inscatter2;
	int i=0;
	const std::vector<int> &quad_strip_vertices=helper->GetQuadStripVertices();
	size_t qs_vert=0;
	for(std::vector<simul::clouds::CloudGeometryHelper::QuadStrip>::const_iterator j=helper->GetQuadStrips().begin();
		j!=helper->GetQuadStrips().end();j++,i++)
	{
		// The distance-fade for these clouds. At distance dist, how much of the cloud's colour is lost?
		simul::sky::float4 loss;
		// And how much sunlight is scattered in from the air in front of the cloud?
		simul::sky::float4 inscatter1;
		
		bool bit=false;

		for(size_t k=0;k<(j)->num_vertices;k++,qs_vert++,v++,bit=!bit)
		{
			Vertex2D_t &vertex=vertices[v];
			const simul::clouds::CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
			
			simul::sky::float4 inscatter;
			pos.Define(V.x,V.y,V.z);
			if(v>=MAX_VERTICES)
				break;
			vertex.position=float3(V.x,V.y,V.z);
			vertex.texCoords=float2(V.cloud_tex_x,V.cloud_tex_y);
			vertex.texCoordNoise=float2(V.noise_tex_x,V.noise_tex_y);
			inscatter=helper->GetInscatter(0,V);
			loss=helper->GetLoss(0,V);
			vertex.loss=float3(loss.x,loss.y,loss.z);
			vertex.inscatter=float3(inscatter.x,inscatter.y,inscatter.z);
			vertex.imageCoords=float2(vertex.texCoords.x*image_scale,vertex.texCoords.y*image_scale);
		}
		if(v>=MAX_VERTICES)
			break;
		Vertex2D_t &vertex=vertices[v];
		vertex.position=pos;
		v++;
	}
	if((v-startv)>2)
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,(v-startv)-2,&(vertices[startv]),sizeof(Vertex2D_t));
	hr=m_pCloudEffect->EndPass();
	hr=m_pCloudEffect->End();

	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	return hr;
}

void Simul2DCloudRenderer::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}

void Simul2DCloudRenderer::SetWindVelocity(float x,float y)
{
	wind_vector.Define(x,0,y);
}

simul::clouds::CloudInterface *Simul2DCloudRenderer::GetCloudInterface()
{
	return cloudInterface;
}
