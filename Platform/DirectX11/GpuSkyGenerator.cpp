#include "GpuSkyGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include "Simul/Platform/CrossPlatform/simul_gpu_sky.sl"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/DirectX11/Utilities.h"
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
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_gpu_sky.fx");
	if(effect)
	{
		lossTechnique	=effect->GetTechniqueByName("simul_gpu_loss");
		inscTechnique	=effect->GetTechniqueByName("simul_gpu_insc");
		skylTechnique	=effect->GetTechniqueByName("simul_gpu_skyl");
		gpuSkyConstants	=effect->GetConstantBufferByName("GpuSkyConstants");
	}
	
	MAKE_CONSTANT_BUFFER(constantBuffer,GpuSkyConstants);
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled&&m_pd3dDevice!=NULL;
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
				,simul::sky::float4 starlight
				,simul::sky::float4 dir_to_sun
				,simul::sky::float4 dir_to_moon
				,float haze
				,float overcast
				,float overcast_base_km
				,float overcast_range_km
				,int start_texel
				,int texels
				,const simul::sky::float4 *density_table
				,const simul::sky::float4 *optical_table
				,const simul::sky::float4 *blackbody_table
				,int table_size
				,float maxDensityAltKm
				,bool InfraRed
				,float emissivity
				,float seaLevelTemperatureK
				)
{
HRESULT hr=S_OK;
	float maxOutputAltKm=altitudes_km[altitudes_km.size()-1];
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight((int)altitudes_km.size(),numElevations);
	}
	int gridsize_2d=altitudes_km.size()*numElevations;
	simul::dx11::Framebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	ID3D11Texture1D *dens_tex1					=make1DTexture(m_pd3dDevice,table_size,DXGI_FORMAT_R32G32B32A32_FLOAT,(const float *)density_table);
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
	GpuSkyConstants gConstants;
	{
		D3D11_MAPPED_SUBRESOURCE mapped_res;	
		hr=m_pImmediateContext->Map(constantBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped_res);	
	
		GpuSkyConstants &constants	=*((GpuSkyConstants*)mapped_res.pData);
	
		constants.texSize			=vec2((float)altitudes_km.size(),(float)numElevations);
		static float tto=0.5f;
		constants.texelOffset		=tto;
		constants.tableSize			=vec2((float)table_size,(float)table_size);
		
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

		constants.starlight			=(const float*)(starlight);
		
		constants.hazeEccentricity	=1.0;
		constants.mieRayleighRatio	=(const float*)(skyInterface->GetMieRayleighRatio());
		constants.emissivity		=emissivity;
		//float y_start=(float)start_texel/(float)new_density_gridsize;
		//float y_range=(float)(texels)/(float)new_density_gridsize;
		constants.yRange			=vec2(0.f,1.f);

	//UPDATE_CONSTANT_BUFFER(constantBuffer,GpuSkyConstants,gpuSkyConstants);
		gConstants=constants;
		m_pImmediateContext->Unmap(constantBuffer, 0);	
	}
	
	density_texture->SetResource(dens_tex);
	// DX11 needs a whole extra Staging texture just to read the rendertarget output. Progress!

	simul::sky::float4 *target=loss;
	F[0]->Activate(m_pImmediateContext);
		F[0]->Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
	F[0]->Deactivate(m_pImmediateContext);
	F[0]->CopyToMemory(m_pImmediateContext,target);
	target+=altitudes_km.size()*numElevations;
	float prevDist_km=0;
	for(int i=1;i<numDistances;i++)
	{
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		gConstants.distanceKm	=dist_km;
		gConstants.prevDistanceKm=prevDist_km;
		UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,GpuSkyConstants,gConstants);
		gpuSkyConstants->SetConstantBuffer(constantBuffer);
		F[1]->Activate(m_pImmediateContext);
			F[1]->Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,1.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(m_pImmediateContext,lossTechnique->GetPassByIndex(0));
			simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
		F[1]->Deactivate(m_pImmediateContext);
		F[1]->CopyToMemory(m_pImmediateContext,target);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDist_km=dist_km;
	}
	
	
	ID3D11Texture3D *loss_tex1=make3DTexture(m_pd3dDevice,(int)altitudes_km.size(),numElevations,numDistances,DXGI_FORMAT_R32G32B32A32_FLOAT,(const float *)loss);
	ID3D11ShaderResourceView* loss_tex;
	m_pd3dDevice->CreateShaderResourceView(loss_tex1,NULL,&loss_tex);
	m_pImmediateContext->GenerateMips(loss_tex);
	
	ID3D11Texture2D *optd_tex1=make2DTexture(m_pd3dDevice,table_size,table_size,DXGI_FORMAT_R32G32B32A32_FLOAT,(const float *)optical_table);
	ID3D11ShaderResourceView* optd_tex;
	m_pd3dDevice->CreateShaderResourceView(optd_tex1,NULL,&optd_tex);
	m_pImmediateContext->GenerateMips(optd_tex);
	
	loss_texture->SetResource(loss_tex);
	optical_depth_texture->SetResource(optd_tex);
	
	target=insc;
	F[0]->Activate(m_pImmediateContext);
		F[0]->Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,1.f);
	F[0]->Deactivate(m_pImmediateContext);
	F[0]->CopyToMemory(m_pImmediateContext,target);
	target+=altitudes_km.size()*numElevations;
	prevDist_km=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		gConstants.distanceKm	=dist_km;
		gConstants.prevDistanceKm=prevDist_km;
		UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,GpuSkyConstants,gConstants);
		gpuSkyConstants->SetConstantBuffer(constantBuffer);
		F[1]->Activate(m_pImmediateContext);
			F[1]->Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,1.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(m_pImmediateContext,inscTechnique->GetPassByIndex(0));
			F[1]->DrawQuad(m_pImmediateContext);
		F[1]->Deactivate(m_pImmediateContext);
		F[1]->CopyToMemory(m_pImmediateContext,target);
		std::swap(F[0],F[1]);
		target+=altitudes_km.size()*numElevations;
		prevDist_km=dist_km;
	}
	
	ID3D11Texture3D *insc_tex1=make3DTexture(m_pd3dDevice,(int)altitudes_km.size(),numElevations,numDistances,DXGI_FORMAT_R32G32B32A32_FLOAT,(const float *)insc);
	ID3D11ShaderResourceView* insc_tex;
	m_pd3dDevice->CreateShaderResourceView(insc_tex1,NULL,&insc_tex);
	m_pImmediateContext->GenerateMips(insc_tex);
	insc_texture->SetResource(insc_tex);
	target=skyl;
	F[0]->Activate(m_pImmediateContext);
		F[0]->Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,1.f);
	F[0]->Deactivate(m_pImmediateContext);
	F[0]->CopyToMemory(m_pImmediateContext,target);
	target+=altitudes_km.size()*numElevations;
	prevDist_km=0.f;
	for(int i=1;i<numDistances;i++)
	{
	// The midpoint of the step represented by this layer
		float zPosition=pow((float)(i)/((float)numDistances-1.f),2.f);
		float dist_km=zPosition*max_distance_km;
		if(i==numDistances-1)
			dist_km=1000.f;
		gConstants.distanceKm	=dist_km;
		gConstants.prevDistanceKm=prevDist_km;
		UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,GpuSkyConstants,gConstants);
		gpuSkyConstants->SetConstantBuffer(constantBuffer);
		F[1]->Activate(m_pImmediateContext);
			F[1]->Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,1.f);
			input_texture->SetResource((ID3D11ShaderResourceView*)F[0]->GetColorTex());
			ApplyPass(m_pImmediateContext,skylTechnique->GetPassByIndex(0));
			F[1]->DrawQuad(m_pImmediateContext);
		F[1]->Deactivate(m_pImmediateContext);
		F[1]->CopyToMemory(m_pImmediateContext,target);
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
