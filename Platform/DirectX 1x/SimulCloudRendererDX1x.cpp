// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRendererDX1x.cpp A renderer for 3d clouds.

#include "SimulCloudRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include <fstream>
#include <math.h>
#include <tchar.h>
#include <dxerr.h>
#include <string>

#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/ThunderCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "CreateEffectDX1x.h"

typedef std::basic_string<TCHAR> tstring;
static tstring filepath=TEXT("");
#define PIXBeginNamedEvent(a,b)		// D3DPERF_BeginEvent(a,L##b)
#define PIXEndNamedEvent()			// D3DPERF_EndEvent()
DXGI_FORMAT illumination_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
const bool big_endian=false;
static unsigned default_mip_levels=0;
static unsigned bits[4]={	(unsigned)0x0F00,
							(unsigned)0x000F,
							(unsigned)0x00F0,
							(unsigned)0xF000};

#ifndef FAIL_RETURN
#define FAIL_RETURN(x)    { hr = x; if( FAILED(hr) ) { return; } }
#endif


typedef std::basic_string<TCHAR> tstring;

struct float2
{
	float x,y;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
	}
};
struct float3
{
	float x,y,z;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
		z=f[2];
	}
};
struct CloudVertex_t
{
    float3 position;
    float3 texCoords;
    float layerFade;
    float2 texCoordsNoise;
};
struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};
CloudVertex_t *vertices=NULL;
PosTexVert_t *lightning_vertices=NULL;
#define MAX_VERTICES (4000)

class ExampleHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float ,float ,float z) const
	{
		static float base_layer=0.28f;
		static float transition=0.2f;
		static float mul=0.86f;
		static float default_=1.f;
		if(z>base_layer)
		{
			if(z>base_layer+transition)
				return mul;
			float i=(z-base_layer)/transition;
			return default_*(1.f-i)+mul*i;
		}
		return default_;
	}
};
ExampleHumidityCallback hm;

SimulCloudRendererDX1x::SimulCloudRendererDX1x(const char *license_key) :
	simul::clouds::BaseCloudRenderer(license_key),
	m_hTechniqueLightning(NULL),
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pVtxDecl(NULL),
	m_pLightningVtxDecl(NULL),
	m_pCloudEffect(NULL),
	vertexBuffer(NULL),
	m_pLightningEffect(NULL),
	noise_texture(NULL),
	lightning_texture(NULL),
	illumination_texture(NULL),
	sky_loss_texture_1(NULL),
	sky_inscatter_texture_1(NULL),
	skyLossTexture1Resource(NULL),
	skyInscatterTexture1Resource(NULL),
	noiseTextureResource(NULL),
	lightningIlluminationTextureResource(NULL),
	y_vertical(true)
	,enable_lightning(false)
	,lightning_active(false)
	,timing(0.f)
	,mapped(-1)
{
	mapped_cloud_texture.pData=NULL;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
	{
		cloud_textures[i]=NULL;
		cloudDensityResource[i]=NULL;
	}

	cloudNode->SetLicense(license_key);
	helper->SetYVertical(y_vertical);
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;
	cloudNode->AddHumidityCallback(&hm);
	cloudKeyframer->SetFillTexturesAsBlocks(false);
	cloudKeyframer->SetBits(simul::clouds::CloudKeyframer::BRIGHTNESS,
							simul::clouds::CloudKeyframer::SECONDARY,
							simul::clouds::CloudKeyframer::DENSITY,
							simul::clouds::CloudKeyframer::AMBIENT);
}

void SimulCloudRendererDX1x::SetSkyInterface(simul::sky::BaseSkyInterface *si)
{
	skyInterface=si;
	cloudKeyframer->SetSkyInterface(si);
}

void SimulCloudRendererDX1x::SetLossTextures(void *t)
{
	ID3D1xResource* t1=((ID3D1xResource*)t);
	if(sky_loss_texture_1!=t1)
	{
		sky_loss_texture_1=static_cast<ID3D1xTexture2D*>(t1);
		if(skyLossTexture1)
			skyLossTexture1->SetResource(NULL);
		SAFE_RELEASE(skyLossTexture1Resource);
		HRESULT hr;
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(sky_loss_texture_1,NULL,&skyLossTexture1Resource));
	}
}

void SimulCloudRendererDX1x::SetInscatterTextures(void *t)
{
	ID3D1xResource* t1=((ID3D1xResource*)t);
	if(sky_inscatter_texture_1!=t1)
	{
		sky_inscatter_texture_1=static_cast<ID3D1xTexture2D*>(t1);
		if(skyInscatterTexture1)
			skyInscatterTexture1->SetResource(NULL);
		SAFE_RELEASE(skyInscatterTexture1Resource);
		HRESULT hr;
		V_CHECK(m_pd3dDevice->CreateShaderResourceView(sky_inscatter_texture_1,NULL,&skyInscatterTexture1Resource));
	}
}

void SimulCloudRendererDX1x::SetNoiseTextureProperties(int s,int f,int o,float p)
{
	noise_texture_size=s;
	noise_texture_frequency=f;
	texture_octaves=o;
	texture_persistence=p;
	CreateNoiseTexture();
}

bool SimulCloudRendererDX1x::RestoreDeviceObjects( void* dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	HRESULT hr;
	B_RETURN(CreateNoiseTexture());
	B_RETURN(CreateLightningTexture());
	B_RETURN(CreateCloudEffect());

	D3D1x_SHADER_RESOURCE_VIEW_DESC texdesc;

	texdesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
	texdesc.ViewDimension=D3D1x_SRV_DIMENSION_TEXTURE3D;
	texdesc.Texture3D.MostDetailedMip=0;
	texdesc.Texture3D.MipLevels=1;
    B_RETURN(m_pd3dDevice->CreateShaderResourceView(noise_texture,NULL,&noiseTextureResource));

	noiseTexture				->SetResource(noiseTextureResource);

	B_RETURN(CreateEffect(m_pd3dDevice,&m_pLightningEffect,L"simul_lightning.fx"));
	if(m_pLightningEffect)
	{
		m_hTechniqueLightning	=m_pLightningEffect->GetTechniqueByName("simul_lightning");
		l_worldViewProj			=m_pLightningEffect->GetVariableByName("worldViewProj")->AsMatrix();
	}

	
	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	1, DXGI_FORMAT_R32_FLOAT,			0,	24,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	2, DXGI_FORMAT_R32G32_FLOAT,		0,	28,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
    };
	const D3D1x_INPUT_ELEMENT_DESC std_decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=m_hTechniqueCloud->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);
//return true;
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,4,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
	// Create the main vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        MAX_VERTICES*sizeof(CloudVertex_t),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
	if(!vertices)
		vertices=new CloudVertex_t[MAX_VERTICES];
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(CloudVertex_t);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);
//return true;
	if(m_hTechniqueLightning)
	{
		pass=m_hTechniqueLightning->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);
		hr=m_pd3dDevice->CreateInputLayout( std_decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pLightningVtxDecl);
	}
	cloudKeyframer->SetRenderCallback(NULL);
	cloudKeyframer->SetRenderCallback(this);
	if(lightningIlluminationTextureResource)
		lightningIlluminationTexture->SetResource(lightningIlluminationTextureResource);
	return (hr==S_OK);
}

bool SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	Unmap();
	if(illumination_texture)
		Unmap3D(illumination_texture);
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pLightningEffect);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		SAFE_RELEASE(cloudDensityResource[i]);
	}
	// Set the stored texture sizes to zero, so the textures will be re-created.
	cloud_tex_width_x=cloud_tex_length_y=cloud_tex_depth_z=0;
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(skyLossTexture1Resource);
	SAFE_RELEASE(skyInscatterTexture1Resource);

	SAFE_RELEASE(noiseTextureResource);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);

	return (hr==S_OK);
}

bool SimulCloudRendererDX1x::Destroy()
{
	HRESULT hr=S_OK;
	hr=InvalidateDeviceObjects();
	return (hr==S_OK);
}

SimulCloudRendererDX1x::~SimulCloudRendererDX1x()
{
	Destroy();
}

bool SimulCloudRendererDX1x::CreateNoiseTexture(bool override_file)
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);
	//if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,DXGI_FORMAT_R8G8B8A8_UINT,D3DPOOL_MANAGED,&noise_texture)))
	//	return (hr==S_OK);
	D3D1x_TEXTURE2D_DESC desc=
	{
		noise_texture_size,
		noise_texture_size,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		{1,0},
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	B_RETURN(m_pd3dDevice->CreateTexture2D(&desc,NULL,&noise_texture));
	D3D1x_MAPPED_TEXTURE2D mapped;
	if(FAILED(hr=Map2D(noise_texture,&mapped)))
		return false;
	simul::clouds::TextureGenerator::SetBits(0x000000FF,0x0000FF00,0x00FF0000,0xFF000000,(unsigned)4,big_endian);
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(mapped.pData),noise_texture_size,noise_texture_frequency,texture_octaves,texture_persistence);
	Unmap2D(noise_texture);
	return true;
}

bool SimulCloudRendererDX1x::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	unsigned size=64;
	SAFE_RELEASE(lightning_texture);
	D3D1x_TEXTURE1D_DESC desc=
	{
		size,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	if(FAILED(hr=m_pd3dDevice->CreateTexture1D(&desc,NULL,&lightning_texture)))
		return (hr==S_OK);
	D3D1x_MAPPED_TEXTURE2D resource;
	if(FAILED(hr=Map1D(lightning_texture,&resource)))
		return (hr==S_OK);
	unsigned char *lightning_tex_data=(unsigned char *)(resource.pData);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour.x*level;
		float g=lightning_colour.y*level;
		float b=lightning_colour.z*level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*r);
	}
	Unmap1D(lightning_texture);
	return (hr==S_OK);
}

void SimulCloudRendererDX1x::SetIlluminationGridSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	SAFE_RELEASE(illumination_texture);
	D3D1x_TEXTURE3D_DESC desc=
	{
		width_x,length_y,depth_z,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	HRESULT hr=m_pd3dDevice->CreateTexture3D(&desc,0,&illumination_texture);
	D3D1x_MAPPED_TEXTURE3D mapped;
	if(FAILED(hr=Map3D(illumination_texture,&mapped)))
		return;
	memset(mapped.pData,0,4*width_x*length_y*depth_z);
	Unmap3D(illumination_texture);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);
    FAILED(m_pd3dDevice->CreateShaderResourceView(illumination_texture,NULL,&lightningIlluminationTextureResource ));
	FAILED(hr=Map3D(illumination_texture,&mapped_illumination));
}

void SimulCloudRendererDX1x::FillIlluminationSequentially(int source_index,int texel_index,int num_texels,const unsigned char *uchar8_array)
{
	if(!num_texels)
		return;
	unsigned char *lightning_cloud_tex_data=(unsigned char *)(mapped_illumination.pData);
	unsigned *ptr=(unsigned *)(lightning_cloud_tex_data);
	ptr+=texel_index;
	for(int i=0;i<num_texels;i++)
	{
		unsigned ui=*uchar8_array;
		unsigned offset=8*(source_index); // xyzw
		ui<<=offset;
		unsigned msk=255<<offset;
		msk=~msk;
		(*ptr)&=msk;
		(*ptr)|=ui;
		uchar8_array++;
		ptr++;
	}
}
void SimulCloudRendererDX1x::Unmap()
{
	if(mapped!=-1)
	{
		if(mapped>=0)
		{
			Unmap3D(cloud_textures[mapped]);
		}
		mapped=-1;
		mapped_cloud_texture.pData=NULL;
	}
}

void SimulCloudRendererDX1x::Map(int texture_index)
{
	HRESULT hr=S_OK;
	if(mapped!=texture_index)
	{
		Unmap();
		hr=Map3D(cloud_textures[texture_index],&mapped_cloud_texture);
		mapped=texture_index;
	}
}

void SimulCloudRendererDX1x::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	HRESULT hr=S_OK;
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D1x_TEXTURE3D_DESC desc=
	{
		width_x,length_y,depth_z,
		1,
		cloud_tex_format,
		D3D1x_USAGE_DYNAMIC,
		D3D1x_BIND_SHADER_RESOURCE,
		D3D1x_CPU_ACCESS_WRITE,
		0
	};
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=m_pd3dDevice->CreateTexture3D(&desc,0,&cloud_textures[i])))
			return;
	}
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloudDensityResource[i]);
	    FAIL_RETURN(m_pd3dDevice->CreateShaderResourceView(cloud_textures[i],NULL,&cloudDensityResource[i]));
	}
	Map(2);
}

void SimulCloudRendererDX1x::FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array)
{
	Map(texture_index);
	unsigned *ptr=(unsigned *)mapped_cloud_texture.pData;
	if(!ptr)
		return;
	ptr+=texel_index;
	memcpy(ptr,uint32_array,num_texels*sizeof(unsigned));
	if(texture_index!=2)
		Unmap();
}

void SimulCloudRendererDX1x::CycleTexturesForward()
{
	Unmap();
	std::swap(cloud_textures[0],cloud_textures[1]);
	std::swap(cloud_textures[1],cloud_textures[2]);
	std::swap(cloudDensityResource[0],cloudDensityResource[1]);
	std::swap(cloudDensityResource[1],cloudDensityResource[2]);
	Map(2);
}

bool SimulCloudRendererDX1x::CreateCloudEffect()
{
	if(!m_pd3dDevice)
		return S_OK;
	std::map<std::string,std::string> defines;
	defines["DETAIL_NOISE"]='1';
	if(y_vertical)
		defines["Y_VERTICAL"]="1";
	else
		defines["Z_VERTICAL"]="1";
	HRESULT hr=CreateEffect(m_pd3dDevice,&m_pCloudEffect,L"simul_clouds_and_lightning_dx11.fx",defines);
	m_hTechniqueCloud					=m_pCloudEffect->GetTechniqueByName("simul_clouds");
	m_hTechniqueCloudsAndLightning		=m_pCloudEffect->GetTechniqueByName("simul_clouds_and_lightning");

	worldViewProj						=m_pCloudEffect->GetVariableByName("worldViewProj")->AsMatrix();
	eyePosition							=m_pCloudEffect->GetVariableByName("eyePosition")->AsVector();
	lightResponse						=m_pCloudEffect->GetVariableByName("lightResponse")->AsVector();
	lightDir							=m_pCloudEffect->GetVariableByName("lightDir")->AsVector();
	skylightColour						=m_pCloudEffect->GetVariableByName("skylightColour")->AsVector();
	sunlightColour						=m_pCloudEffect->GetVariableByName("sunlightColour")->AsVector();
	fractalScale						=m_pCloudEffect->GetVariableByName("fractalScale")->AsVector();
	interp								=m_pCloudEffect->GetVariableByName("interp")->AsScalar();
	mieRayleighRatio					=m_pCloudEffect->GetVariableByName("mieRayleighRatio")->AsVector();
	hazeEccentricity					=m_pCloudEffect->GetVariableByName("hazeEccentricity")->AsScalar();
	fadeInterp							=m_pCloudEffect->GetVariableByName("fadeInterp")->AsScalar();
	cloudEccentricity					=m_pCloudEffect->GetVariableByName("cloudEccentricity")->AsScalar();
	altitudeTexCoord					=m_pCloudEffect->GetVariableByName("altitudeTexCoord")->AsScalar();

	//if(enable_lightning)
	{
		lightningMultipliers			=m_pCloudEffect->GetVariableByName("lightningMultipliers")->AsVector();
		lightningColour					=m_pCloudEffect->GetVariableByName("lightningColour")->AsVector();
		illuminationOrigin				=m_pCloudEffect->GetVariableByName("illuminationOrigin")->AsVector();
		illuminationScales				=m_pCloudEffect->GetVariableByName("illuminationScales")->AsVector();
	}

	cloudDensity1						=m_pCloudEffect->GetVariableByName("cloudDensity1")->AsShaderResource();
	cloudDensity2						=m_pCloudEffect->GetVariableByName("cloudDensity2")->AsShaderResource();
	noiseTexture						=m_pCloudEffect->GetVariableByName("noiseTexture")->AsShaderResource();
	lightningIlluminationTexture		=m_pCloudEffect->GetVariableByName("lightningIlluminationTexture")->AsShaderResource();
	skyLossTexture1						=m_pCloudEffect->GetVariableByName("skyLossTexture1")->AsShaderResource();
	skyInscatterTexture1				=m_pCloudEffect->GetVariableByName("skyInscatterTexture1")->AsShaderResource();
	return (hr==S_OK);
}

void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
{
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(wvp,&tmp2);
}

D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}


bool SimulCloudRendererDX1x::Render(bool cubemap,bool depth_testing,bool default_fog)
{
	HRESULT hr=S_OK;
	PIXBeginNamedEvent(1,"Render Clouds Layers");
	cloudDensity1->SetResource(cloudDensityResource[0]);
	cloudDensity2->SetResource(cloudDensityResource[1]);
	noiseTexture->SetResource(noiseTextureResource);
	skyLossTexture1->SetResource(skyLossTexture1Resource);
	skyInscatterTexture1->SetResource(skyInscatterTexture1Resource);

	// Mess with the proj matrix to extend the far clipping plane:
	// According to the documentation for D3DXMatrixPerspectiveFovLH:
	float zNear=-proj._43/proj._33;
	float zFar=helper->GetMaxCloudDistance()*1.1f;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);
	simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);
	X+=wind_offset;

	worldViewProj->AsMatrix()->SetMatrix(&wvp._11);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	simul::sky::float4 view_km=(const float*)cam_pos;
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
	view_km*=0.001f;
	float base_alt_km=0.001f*(cloudInterface->GetCloudBaseZ());
	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*cloudInterface->GetLightResponse(),
										indirect_light_mult*cloudInterface->GetSecondaryLightResponse(),
										0,
										0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();

	// calculate sun occlusion for any external classes that need it:
	if(y_vertical)
		std::swap(X.y,X.z);
	float len=cloudInterface->GetOpticalPathLength(X.FloatPointer(0),(const float*)sun_dir);
	float vis=min(1.f,max(0.f,exp(-0.001f*cloudInterface->GetOpticalDensity()*len)));
	sun_occlusion=1.f-vis;
	if(y_vertical)
		std::swap(X.y,X.z);

	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 sky_light_colour=skyInterface->GetAmbientLight(base_alt_km)*cloudInterface->GetAmbientLightResponse();
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(base_alt_km);
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);

	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->MakeGeometry(cloudInterface,enable_lightning);
	if(fade_mode==CPU)
		helper->CalcInscatterFactors(skyInterface);

	ID3D1xEffectConstantBuffer* cbUser=m_pCloudEffect->GetConstantBufferByName("cbUser");
	if(cbUser)
	{
		ID3D1xEffectVectorVariable *lr=cbUser->GetMemberByName("lightResponse")->AsVector();
	}

	float cloud_interp=cloudKeyframer->GetInterpolation();
	interp				->AsScalar()->SetFloat			(cloud_interp);
	eyePosition			->AsVector()->SetFloatVector	(cam_pos);
	lightResponse		->AsVector()->SetFloatVector	(light_response);
	lightDir			->AsVector()->SetFloatVector	(sun_dir);
	skylightColour		->AsVector()->SetFloatVector	(sky_light_colour);
	sunlightColour		->AsVector()->SetFloatVector	(sunlight);
	fractalScale		->AsVector()->SetFloatVector	(fractal_scales);
	mieRayleighRatio	->AsVector()->SetFloatVector	(skyInterface->GetMieRayleighRatio());
	cloudEccentricity	->SetFloat						(cloudInterface->GetMieAsymmetry());
	hazeEccentricity	->AsScalar()->SetFloat			(skyInterface->GetMieEccentricity());
	fadeInterp			->AsScalar()->SetFloat			(fade_interp);
	altitudeTexCoord	->AsScalar()->SetFloat			(altitude_tex_coord);

	if(enable_lightning)
	{
		static float bb=.1f;
		simul::sky::float4 lightning_multipliers;
		lightning_colour=lightningRenderInterface->GetLightningColour();
		for(unsigned i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(i);
			else lightning_multipliers[i]=0;
		}
		static float effect_on_cloud=20.f;
		lightning_colour.w=effect_on_cloud;
		lightningMultipliers->SetFloatVector	(lightning_multipliers);
		lightningColour->SetFloatVector	(lightning_colour);

		simul::math::Vector3 light_X1,light_X2,light_DX;
		light_DX.Define(lightningRenderInterface->GetLightningZoneSize(),
			lightningRenderInterface->GetLightningZoneSize(),
			cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());

		light_X1=lightningRenderInterface->GetLightningCentreX();
		light_X1-=0.5f*light_DX;
		light_X1.z=0;
		light_X2=lightningRenderInterface->GetLightningCentreX();
		light_X2+=0.5f*light_DX;
		light_X2.z=light_DX.z;

		illuminationOrigin->SetFloatVector	(light_X1);
		illuminationScales->SetFloatVector	(light_DX);
	}
	int startv=0;
	int v=0;

	startv=v;
	simul::math::Vector3 pos;
#ifdef DX10
	void* mapped_vertices;
#else
	D3D11_MAPPED_SUBRESOURCE mapped_vertices;
#endif
	MapBuffer(vertexBuffer,&mapped_vertices);
	//hr=Map3D(cloud_textures[2],&mapped_cloud_texture);
#ifdef DX10
	vertices=(CloudVertex_t*)mapped_vertices;
#else
	vertices=(CloudVertex_t*)mapped_vertices.pData;
#endif
	for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++)
	{
		helper->MakeLayerGeometry(cloudInterface,*i);
		const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
		size_t qs_vert=0;
		float fade=(*i)->fadeIn;
		bool start=true;
		for(std::vector<const simul::clouds::CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
			j!=(*i)->quad_strips.end();j++)
		{
			bool l=0;
			for(size_t k=0;k<(*j)->num_vertices;k++,qs_vert++,l++,v++)
			{
				const simul::clouds::CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
				
				pos.Define(V.x,V.y,V.z);
				simul::math::Vector3 tex_pos(V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
				if(v>=MAX_VERTICES)
					break;
				if(start)
				{
					CloudVertex_t &startvertex=vertices[v];
					startvertex.position=pos;
					startvertex.texCoords=tex_pos;
					startvertex.texCoordsNoise.x=V.noise_tex_x;
					startvertex.texCoordsNoise.y=V.noise_tex_y;
					startvertex.layerFade=fade;
					v++;
					start=false;
				}
				CloudVertex_t &vertex=vertices[v];
				vertex.position=pos;
				vertex.texCoords=tex_pos;
				vertex.texCoordsNoise.x=V.noise_tex_x;
				vertex.texCoordsNoise.y=V.noise_tex_y;
				vertex.layerFade=fade;
			}
		}
		if(v>=MAX_VERTICES)
			break;
		CloudVertex_t &vertex=vertices[v];
		vertex.position=pos;
		v++;
	}
	UnmapBuffer(vertexBuffer);
	UINT stride = sizeof(CloudVertex_t);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
												1,				// the number of buffers in the array
												&vertexBuffer,	// the array of vertex buffers
												&stride,		// array of stride values, one for each buffer
												&offset);		// array of offset values, one for each buffer
	if(enable_lightning)
		ApplyPass(m_hTechniqueCloudsAndLightning->GetPassByIndex(0));
	else
		ApplyPass(m_hTechniqueCloud->GetPassByIndex(0));
	// Set the input layout
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if((v-startv)>2)
		m_pImmediateContext->Draw((v-startv)-2,0);

	PIXEndNamedEvent();
	skyLossTexture1->SetResource(NULL);
	skyInscatterTexture1->SetResource(NULL);
// To prevent BIZARRE DX11 warning, we re-apply the bass with the rendertextures unbound:
	if(enable_lightning)
		ApplyPass(m_hTechniqueCloudsAndLightning->GetPassByIndex(0));
	else
		ApplyPass(m_hTechniqueCloud->GetPassByIndex(0));
	return (hr==S_OK);
}

bool SimulCloudRendererDX1x::RenderLightning()
{
	if(!enable_lightning)
		return S_OK;
	using namespace simul::clouds;

	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];

	HRESULT hr=S_OK;

	PIXBeginNamedEvent(0, "Render Lightning");
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	m_pImmediateContext->IASetInputLayout( m_pLightningVtxDecl );
	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;
	ApplyPass(m_hTechniqueLightning->GetPassByIndex(0));

	l_worldViewProj->SetMatrix(&wvp._11);
	for(unsigned i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->IsSourceStarted(i))
			continue;
		simul::math::Vector3 x1,x2;
		float bright1=0.f,bright2=0.f;
		simul::math::Vector3 camPos(cam_pos);
		lightningRenderInterface->GetSegmentVertex(i,0,0,bright1,x1.FloatPointer(0));
		float dist=(x1-camPos).Magnitude();
		float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lightningRenderInterface->GetNumBranches(i);jj++)
		{
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			for(unsigned k=0;k<lightningRenderInterface->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lightningRenderInterface->GetSegmentVertex(i,jj,k,bright1,x1.FloatPointer(0));

				static float ww=700.f;
				float width=ww*lightningRenderInterface->GetBranchWidth(i,jj);
				float width1=bright1*width;
				simul::math::Vector3 dx=x2-x1;
				simul::math::Vector3 transverse;
				CrossProduct(transverse,view_dir,dx);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t=transverse;
				if(k)
					t=.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				if(!k)
					bright1=0;
				if(k==lightningRenderInterface->GetNumSegments(i,jj)-1)
					bright2=0;
				PosTexVert_t &v1=lightning_vertices[vert_num++];
				if(y_vertical)
				{
					std::swap(x1a.y,x1a.z);
					std::swap(x1b.y,x1b.z);
				}
				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=0;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				PosTexVert_t &v2=lightning_vertices[vert_num++];
				v2.position.x=x1b.x;
				v2.position.y=x1b.y+vertical_shift;
				v2.position.z=x1b.z;
				v2.texCoords.x=1.f;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
				m_pImmediateContext->Draw(vert_num-vert_start-2,0);//PrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,&(lightning_vertices[vert_start]),sizeof(PosTexVert_t));
		}
	}
//	hr=m_pLightningEffect->EndPass();
//	hr=m_pLightningEffect->End();
	PIXEndNamedEvent();
	return (hr==S_OK);
}

void SimulCloudRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}


simul::clouds::LightningRenderInterface *SimulCloudRendererDX1x::GetLightningRenderInterface()
{
	return lightningRenderInterface;
}

float SimulCloudRendererDX1x::GetSunOcclusion() const
{
	return sun_occlusion;
}

bool SimulCloudRendererDX1x::MakeCubemap()
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

const TCHAR *SimulCloudRendererDX1x::GetDebugText() const
{
	static TCHAR debug_text[256];
	simul::math::Vector3 wo=cloudInterface->GetWindOffset();
	_stprintf_s(debug_text,256,_T("interp %4.4g"),cloudKeyframer->GetInterpolation());
	return debug_text;
}


void SimulCloudRendererDX1x::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

float SimulCloudRendererDX1x::GetTiming() const
{
	return timing;
}

void **SimulCloudRendererDX1x::GetCloudTextures()
{
	return (void **)cloud_textures;
}

void SimulCloudRendererDX1x::SetYVertical(bool y)
{
	y_vertical=y;
	helper->SetYVertical(y);
	CreateCloudEffect();
}

bool SimulCloudRendererDX1x::IsYVertical() const
{
	return y_vertical;
}