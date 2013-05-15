
#include "SimulOpticsRendererDX1x.h"
#include "MacrosDX1x.h"
#include "CreateEffectDX1x.h"
#include "Simul/Math/Decay.h"

#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11/Utilities.h"
using namespace simul::dx11;

SimulOpticsRendererDX1x::SimulOpticsRendererDX1x()
	:m_pd3dDevice(NULL)
	,m_pFlareEffect(NULL)
	,flare_texture(NULL)
{
	FlareTexture=_T("SunFlare.png");
}

SimulOpticsRendererDX1x::~SimulOpticsRendererDX1x()
{
	InvalidateDeviceObjects();
}

void SimulOpticsRendererDX1x::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(flare_texture);
	flare_texture=simul::dx11::LoadTexture(FlareTexture.c_str());
	
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
		tstring tn=simul::base::StringToWString(lensFlare.GetTypeName(i));
		ID3D1xShaderResourceView* &tex=halo_textures[i];
		SAFE_RELEASE(tex);
		tex=simul::dx11::LoadTexture((tn+_T(".png")).c_str());
	}
	RecompileShaders();
}

void SimulOpticsRendererDX1x::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pFlareEffect);
	SAFE_RELEASE(flare_texture);
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
}

void SimulOpticsRendererDX1x::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(m_pFlareEffect);
	std::map<std::string,std::string> defines;
	HRESULT hr;
	hr=CreateEffect(m_pd3dDevice,&m_pFlareEffect,L"simul_sky.fx",defines);
	V_CHECK(hr);
	m_hTechniqueFlare			=m_pFlareEffect->GetTechniqueByName("simul_flare");
	worldViewProj				=m_pFlareEffect->GetVariableByName("worldViewProj")->AsMatrix();
	colour						=m_pFlareEffect->GetVariableByName("colour")->AsVector();
	flareTexture				=m_pFlareEffect->GetVariableByName("flareTexture")->AsShaderResource();
}

void SimulOpticsRendererDX1x::RenderFlare(void *context,float exposure,const float *dir,const float *light)
{
	HRESULT hr=S_OK;
	if(!m_pFlareEffect)
		return;
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	StoreD3D11State(m_pImmediateContext);
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
//m_pFlareEffect->SetTechnique(m_hTechniqueFlare);
	flareTexture->SetResource(flare_texture);
	// NOTE: WE DO NOT swap y and z for the light direction. We don't know where this has
	// come from, so we must assume it's already correct for the co-ordinate system!
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	D3DXVECTOR3 cam_pos,cam_dir;
	//m_pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	//m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
	simul::dx11::GetCameraPosVector(view,false,(float*)&cam_pos,(float*)&cam_dir);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	flare_magnitude*=lensFlare.GetStrength();
	sunlight*=sun_mult*flare_magnitude;
	colour->SetFloatVector((const float*)(&sunlight));
	if(flare_magnitude>0.f)
	{
		UtilityRenderer::RenderAngledQuad(m_pd3dDevice,m_pImmediateContext,sun_dir,false,flare_angular_size*flare_magnitude,m_pFlareEffect,m_hTechniqueFlare,view,proj,sun_dir);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			D3DXVECTOR3 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
			flareTexture->SetResource(halo_textures[t]);
			colour->SetFloatVector((const float*)(&sunlight));
			UtilityRenderer::RenderAngledQuad(m_pd3dDevice,m_pImmediateContext,pos,false,flare_angular_size*sz*flare_magnitude,m_pFlareEffect,m_hTechniqueFlare,view,proj,sun_dir);
		}
	}
	m_pImmediateContext->VSSetShader(NULL, NULL, 0);
	m_pImmediateContext->GSSetShader(NULL, NULL, 0);
	m_pImmediateContext->PSSetShader(NULL, NULL, 0);
	RestoreD3D11State(m_pImmediateContext );
}

void SimulOpticsRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}