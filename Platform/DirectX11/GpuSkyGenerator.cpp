#include "GpuSkyGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include "Simul/Platform/CrossPlatform/simul_gpu_sky.sl"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
using namespace simul;
using namespace dx11;

GpuSkyGenerator::GpuSkyGenerator()
	:m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,effect(NULL)
	,constantBuffer(NULL)
{
}

GpuSkyGenerator::~GpuSkyGenerator()
{
	InvalidateDeviceObjects();
}

void GpuSkyGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	RecompileShaders();
	fb[0].RestoreDeviceObjects(m_pd3dDevice);
	fb[1].RestoreDeviceObjects(m_pd3dDevice);
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(constantBuffer);
}

void GpuSkyGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,L"simul_gpu_sky.fx");
	if(effect)
	{
		lossTechnique	=effect->GetTechniqueByName("simul_gpu_loss");
		inscTechnique	=effect->GetTechniqueByName("simul_gpu_insc");//"#define OVERCAST 1\r\n");
		skylTechnique	=effect->GetTechniqueByName("simul_gpu_skyl");
		gpuSkyConstants	=effect->GetConstantBufferByName("GpuSkyConstants");
	}
	
	MAKE_CONSTANT_BUFFER(constantBuffer,GpuSkyConstants);
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled;
}
// make a 1D texture.
static ID3D1xTexture1D* make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w
							,const float *src)
{
	ID3D1xTexture1D*	tex;
	D3D11_TEXTURE1D_DESC textureDesc=
	{
		w,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		w*sizeof(float4),
		w*sizeof(float4)
	};

	m_pd3dDevice->CreateTexture1D(&textureDesc,&init,&tex);
/*	ID3D11ShaderResourceView* r;
	m_pd3dDevice->CreateShaderResourceView(tex,NULL,&r);
	m_pImmediateContext->GenerateMips(r);*/
	return tex;
}

static ID3D11Texture2D* make2DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w,int h
							,const float *src)
{
	ID3D1xTexture2D*	tex;
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		w,h,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		{1,0}
		,D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		4*w*sizeof(float),
		4*w*h*sizeof(float)
	};
	m_pd3dDevice->CreateTexture2D(&textureDesc,&init,&tex);
	return tex;
}


static ID3D1xTexture3D* make3DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w,int l,int d
							,const float *src)
{
	ID3D1xTexture3D*	tex;
	D3D11_TEXTURE3D_DESC textureDesc=
	{
		w,l,d,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT
		,D3D11_USAGE_DYNAMIC
		,D3D11_BIND_SHADER_RESOURCE
		,D3D11_CPU_ACCESS_WRITE
		,0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		4*w*sizeof(float),
		4*w*l*sizeof(float)
	};
	m_pd3dDevice->CreateTexture3D(&textureDesc,&init,&tex);
	return tex;
}
void GpuSkyGenerator::Make2DLossAndInscatterTextures(
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,int numElevations
				,int numDistances
				,simul::sky::float4 *loss
				,simul::sky::float4 *insc
				,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km
				,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 dir_to_sun
				,simul::sky::float4 dir_to_moon
				,float haze
				,float overcast
				,float overcast_base_km
				,float overcast_range_km
				,int index
				,int end_index
				,const simul::sky::float4 *density_table
				,const simul::sky::float4 *optical_table
				,int table_size
				,float maxDensityAltKm
				,bool InfraRed
				)
{
HRESULT hr=S_OK;
	float maxOutputAltKm=altitudes_km[altitudes_km.size()-1];
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(altitudes_km.size(),numElevations);
	}
	FramebufferDX1x *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	ID3D11Texture1D *dens_tex1					=make1DTexture(m_pd3dDevice,m_pImmediateContext,table_size,(const float *)density_table);
	ID3D11ShaderResourceView* dens_tex;
	m_pd3dDevice->CreateShaderResourceView(dens_tex1,NULL,&dens_tex);
	m_pImmediateContext->GenerateMips(dens_tex);

	ID3D1xEffectScalarVariable *distKm							=effect->GetVariableByName("distKm")->AsScalar();
	ID3D1xEffectScalarVariable *prevDistKm						=effect->GetVariableByName("prevDistKm")->AsScalar();
	ID3D1xEffectShaderResourceVariable*	input_texture			=effect->GetVariableByName("input_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	optical_depth_texture	=effect->GetVariableByName("optical_depth_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	density_texture			=effect->GetVariableByName("density_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	loss_texture			=effect->GetVariableByName("loss_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	insc_texture			=effect->GetVariableByName("insc_texture")->AsShaderResource();
	
	{
		D3D11_MAPPED_SUBRESOURCE mapped_res;	
		hr=m_pImmediateContext->Map(constantBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped_res);	
	//*(GpuSkyConstants*)mapped_res.pData = gpuSkyConstants;	
		GpuSkyConstants &constants	=*((GpuSkyConstants*)mapped_res.pData);
	
		constants.texSize			=float2((float)altitudes_km.size(),(float)numElevations);
		static float tto=0.5f;
		constants.texelOffset		=tto;
		constants.tableSize			=float2((float)table_size,(float)table_size);
		
		constants.maxDistanceKm		=max_distance_km;
		
		constants.planetRadiusKm	=skyInterface->GetPlanetRadius();
		constants.maxOutputAltKm	=maxOutputAltKm;
		constants.maxDensityAltKm	=maxDensityAltKm;
		constants.hazeBaseHeightKm	=skyInterface->GetHazeBaseHeightKm();
		constants.hazeScaleHeightKm	=skyInterface->GetHazeScaleHeightKm();

		constants.overcastBaseKm	=overcast_base_km;
		constants.overcastRangeKm	=overcast_range_km;
		constants.overcast			=overcast;

		constants.rayleigh			=(const float*)skyInterface->GetRayleigh();
		constants.hazeMie			=(const float*)(haze*skyInterface->GetMie());
		constants.ozone				=(const float*)(skyInterface->GetOzoneStrength()*skyInterface->GetBaseOzone());

		constants.sunIrradiance		=(const float*)sun_irradiance;
		constants.lightDir			=(const float*)dir_to_sun;

		constants.starlight			=(const float*)(skyInterface->GetStarlight());
		
		constants.hazeEccentricity=1.0;
		constants.mieRayleighRatio=(const float*)(skyInterface->GetMieRayleighRatio());
	//UPDATE_CONSTANT_BUFFER(constantBuffer,GpuSkyConstants,gpuSkyConstants);
	
		m_pImmediateContext->Unmap(constantBuffer, 0);	
	}
	gpuSkyConstants->SetConstantBuffer(constantBuffer);
	
	density_texture->SetResource(dens_tex);
	// DX11 needs a whole extra Staging texture just to read the rendertarget output. Progress!

	simul::sky::float4 *target=loss;
	F[0]->Activate();
		F[0]->Clear(1.f,1.f,1.f,1.f);
	F[0]->Deactivate();
	F[0]->CopyToMemory(target);
	target+=altitudes_km.size()*numElevations;
	float prevDist_km=0;
	for(int i=1;i<numDistances;i++)
	{
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		distKm->SetFloat(dist_km);
		prevDistKm->SetFloat(prevDist_km);
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(lossTechnique->GetPassByIndex(0));
			F[1]->DrawQuad();
		F[1]->Deactivate();
		F[1]->CopyToMemory(target);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDist_km=dist_km;
	}
	
	
	ID3D11Texture3D *loss_tex1=make3DTexture(m_pd3dDevice,m_pImmediateContext,altitudes_km.size(),numElevations,numDistances,(const float *)loss);
	ID3D11ShaderResourceView* loss_tex;
	m_pd3dDevice->CreateShaderResourceView(loss_tex1,NULL,&loss_tex);
	m_pImmediateContext->GenerateMips(loss_tex);
	
	ID3D11Texture2D *optd_tex1=make2DTexture(m_pd3dDevice,m_pImmediateContext,table_size,table_size,(const float *)optical_table);
	ID3D11ShaderResourceView* optd_tex;
	m_pd3dDevice->CreateShaderResourceView(optd_tex1,NULL,&optd_tex);
	m_pImmediateContext->GenerateMips(optd_tex);
	
	loss_texture->SetResource(loss_tex);
	optical_depth_texture->SetResource(optd_tex);
	
	target=insc;
	F[0]->Activate();
		F[0]->Clear(0.f,0.f,0.f,0.f);
	F[0]->Deactivate();
	F[0]->CopyToMemory(target);
	target+=altitudes_km.size()*numElevations;
	prevDist_km=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		distKm->SetFloat(dist_km);
		prevDistKm->SetFloat(prevDist_km);
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(inscTechnique->GetPassByIndex(0));
			F[1]->DrawQuad();
		F[1]->Deactivate();
		F[1]->CopyToMemory(target);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDist_km=dist_km;
	}
	
	ID3D11Texture3D *insc_tex1=make3DTexture(m_pd3dDevice,m_pImmediateContext,altitudes_km.size(),numElevations,numDistances,(const float *)insc);
	ID3D11ShaderResourceView* insc_tex;
	m_pd3dDevice->CreateShaderResourceView(insc_tex1,NULL,&insc_tex);
	m_pImmediateContext->GenerateMips(insc_tex);
	insc_texture->SetResource(insc_tex);
	target=skyl;
	F[0]->Activate();
		F[0]->Clear(0.f,0.f,0.f,0.f);
	F[0]->Deactivate();
	F[0]->CopyToMemory(target);
	target+=altitudes_km.size()*numElevations;
	prevDist_km=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		distKm->SetFloat(dist_km);
		prevDistKm->SetFloat(prevDist_km);
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(skylTechnique->GetPassByIndex(0));
			F[1]->DrawQuad();
		F[1]->Deactivate();
		F[1]->CopyToMemory(target);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDist_km=dist_km;
	}
	
	
	input_texture->SetResource(NULL);
	density_texture->SetResource(NULL);
	SAFE_RELEASE(dens_tex1);
	SAFE_RELEASE(dens_tex);
	SAFE_RELEASE(loss_tex1);
	SAFE_RELEASE(loss_tex);
	SAFE_RELEASE(optd_tex1);
	SAFE_RELEASE(optd_tex);
	SAFE_RELEASE(insc_tex1);
	SAFE_RELEASE(insc_tex);
}
