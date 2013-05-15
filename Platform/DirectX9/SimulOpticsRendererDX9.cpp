
#include "SimulOpticsRendererDX9.h"
#include "Macros.h"
#include "CreateDX9Effect.h"
#include "Simul/Math/Decay.h"

SimulOpticsRendererDX9::SimulOpticsRendererDX9()
	:m_pd3dDevice(NULL)
	,m_pFlareEffect(NULL)
	,external_flare_texture(false)
	,flare_texture(NULL)
{
	FlareTexture="SunFlare.png";
}

SimulOpticsRendererDX9::~SimulOpticsRendererDX9()
{
	InvalidateDeviceObjects();
}

void SimulOpticsRendererDX9::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
		CreateDX9Texture(m_pd3dDevice,flare_texture,FlareTexture.c_str());
	}
	RecompileShaders();
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	int num_halo_textures=lensFlare.GetNumArtifactTypes();
	halo_textures.resize(num_halo_textures);
	for(int i=0;i<num_halo_textures;i++)
	{
		std::string tn=lensFlare.GetTypeName(i);
		LPDIRECT3DTEXTURE9 &tex=halo_textures[i];
		CreateDX9Texture(m_pd3dDevice,tex,(tn+".png").c_str());
	}
	RecompileShaders();
}

void SimulOpticsRendererDX9::InvalidateDeviceObjects()
{
	if(m_pFlareEffect)
        m_pFlareEffect->OnLostDevice();
	SAFE_RELEASE(m_pFlareEffect);
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
	}
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
}

void SimulOpticsRendererDX9::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(m_pFlareEffect);
	std::map<std::string,std::string> defines;
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pFlareEffect,"simul_sky.fx",defines));
	m_hTechniqueFlare			=m_pFlareEffect->GetTechniqueByName("simul_flare");
	worldViewProj				=m_pFlareEffect->GetParameterByName(NULL,"worldViewProj");
	colour						=m_pFlareEffect->GetParameterByName(NULL,"colour");
	flareTexture				=m_pFlareEffect->GetParameterByName(NULL,"flareTexture");
}

void SimulOpticsRendererDX9::SetFlare(LPDIRECT3DTEXTURE9 tex,float rad)
{
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
	}
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	flare_angular_size=rad;
	flare_texture=tex;
	external_flare_texture=true;
}

void SimulOpticsRendererDX9::RenderFlare(void *context,float exposure,const float *dir,const float *light)
{
	HRESULT hr=S_OK;
	if(!m_pFlareEffect)
		return ;
	D3DXVECTOR3 sun_dir(dir);//skyKeyframer->GetDirectionToLight());
	float magnitude=exposure;//*(1.f-sun_occlusion);
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
	//float alt_km=0.001f*cam_pos.y;
	simul::sky::float4 sunlight(light);//=skyKeyframer->GetLocalIrradiance(alt_km)*lensFlare.GetStrength();
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.05f;
    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);
	m_pFlareEffect->SetTechnique(m_hTechniqueFlare);
	m_pFlareEffect->SetTexture(flareTexture,flare_texture);
	// NOTE: WE DO NOT swap y and z for the light direction. We don't know where this has
	// come from, so we must assume it's already correct for the co-ordinate system!
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	D3DXVECTOR3 cam_pos,cam_dir;
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
	GetCameraPosVector(view,false,(float*)&cam_pos,(float*)&cam_dir);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	flare_magnitude*=lensFlare.GetStrength();
	sunlight*=sun_mult*flare_magnitude;
	m_pFlareEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
	if(flare_magnitude>0.f)
	{
		hr=RenderAngledQuad(m_pd3dDevice,cam_pos,sun_dir,false,flare_angular_size*flare_magnitude,m_pFlareEffect);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			D3DXVECTOR3 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
			m_pFlareEffect->SetTexture(flareTexture,halo_textures[t]);
			m_pFlareEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
			hr=RenderAngledQuad(m_pd3dDevice,cam_pos,pos,false,flare_angular_size*sz*flare_magnitude,m_pFlareEffect);
		}
	}
}

void SimulOpticsRendererDX9::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}