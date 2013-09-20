#include "GpuSkyGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include "Simul/Platform/CrossPlatform/simul_gpu_sky.sl"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/DirectX11/Utilities.h"
using namespace simul;
using namespace dx11;

GpuSkyGenerator::GpuSkyGenerator()
	:m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,effect(NULL)
	,constantBuffer(NULL)
	,tables_checksum(0)
{
	SetDirectTargets(NULL,NULL,NULL);
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
	gpuSkyConstants.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
	gpuSkyConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(constantBuffer);
	m_pd3dDevice=NULL;
	dens_tex.release();
	optd_tex.release();
	tables_checksum=0;
}

void GpuSkyGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_gpu_sky.fx");
	if(effect)
	{
		lossComputeTechnique	=effect->GetTechniqueByName("gpu_loss_compute");
		inscComputeTechnique	=effect->GetTechniqueByName("gpu_insc_compute");
		skylComputeTechnique	=effect->GetTechniqueByName("gpu_skyl_compute");
		gpuSkyConstants.LinkToEffect(effect,"GpuSkyConstants");
	}
}

static int range(int x,int start,int end)
{
	if(x<start)
		x=start;
	if(x>end)
		x=end;
	return x;
}


//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled&&m_pd3dDevice!=NULL;
}

void GpuSkyGenerator::Make2DLossAndInscatterTextures(
				int cycled_index
				,simul::sky::AtmosphericScatteringInterface *skyInterface
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
				,unsigned new_tables_checksum
				,float overcast_base_km
				,float overcast_range_km
				,int start_texel
				,int num_texels
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
	SIMUL_PROFILE_START("GpuSkyGenerator init")
	HRESULT hr=S_OK;
	int gridsize			=(int)altitudes_km.size()*numElevations*numDistances;
	int gridsize_2d			=(int)altitudes_km.size()*numElevations;
	if(dens_tex.width!=table_size||optd_tex.width!=table_size)
		tables_checksum=0;
	dens_tex.ensureTexture2DSizeAndFormat(m_pd3dDevice,table_size,1,DXGI_FORMAT_R32G32B32A32_FLOAT,false);
	optd_tex.ensureTexture2DSizeAndFormat(m_pd3dDevice,table_size,table_size,DXGI_FORMAT_R32G32B32A32_FLOAT,false,false);

	if(new_tables_checksum!=tables_checksum)
	{
		dens_tex.setTexels(m_pImmediateContext,(unsigned*)density_table,0,table_size);
		optd_tex.setTexels(m_pImmediateContext,(unsigned *)optical_table,0,table_size*table_size);
		tables_checksum=new_tables_checksum;
	}
	SIMUL_PROFILE_END
	SIMUL_PROFILE_START("GpuSkyGenerator 1")

	ID3D1xEffectScalarVariable *distKm							=effect->GetVariableByName("distKm")->AsScalar();
	ID3D1xEffectScalarVariable *prevDistKm						=effect->GetVariableByName("prevDistKm")->AsScalar();
	ID3D1xEffectShaderResourceVariable*	input_texture			=effect->GetVariableByName("input_texture")->AsShaderResource();
	ID3DX11EffectUnorderedAccessViewVariable *targetTexture		=effect->GetVariableByName("targetTexture")->AsUnorderedAccessView();
	ID3DX11EffectShaderResourceVariable* optical_depth_texture	=effect->GetVariableByName("optical_depth_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	density_texture			=effect->GetVariableByName("density_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	loss_texture			=effect->GetVariableByName("loss_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	insc_texture			=effect->GetVariableByName("insc_texture")->AsShaderResource();
	
	float maxOutputAltKm	=altitudes_km[altitudes_km.size()-1];
	{
		gpuSkyConstants.texSize				=vec2((float)altitudes_km.size(),(float)numElevations);
		static float tto=0.5f;
		gpuSkyConstants.texelOffset			=tto;
		gpuSkyConstants.tableSize			=vec2((float)table_size,(float)table_size);
		
		gpuSkyConstants.maxDistanceKm		=max_distance_km;
		
		gpuSkyConstants.planetRadiusKm		=skyInterface->GetPlanetRadius();
		gpuSkyConstants.maxOutputAltKm		=maxOutputAltKm;
		gpuSkyConstants.maxDensityAltKm		=maxDensityAltKm;
		gpuSkyConstants.hazeBaseHeightKm	=skyInterface->GetHazeBaseHeightKm();
		gpuSkyConstants.hazeScaleHeightKm	=skyInterface->GetHazeScaleHeightKm();

		gpuSkyConstants.overcastBaseKm		=overcast_base_km;
		gpuSkyConstants.overcastRangeKm		=overcast_range_km;
		gpuSkyConstants.overcast			=0.0f;

		gpuSkyConstants.rayleigh			=(const float*)skyInterface->GetRayleigh();
		gpuSkyConstants.hazeMie				=(const float*)(haze*skyInterface->GetMie());
		gpuSkyConstants.ozone				=(const float*)(skyInterface->GetOzoneStrength()*skyInterface->GetBaseOzone());

		gpuSkyConstants.sunIrradiance		=(const float*)sun_irradiance;
		gpuSkyConstants.lightDir			=(const float*)dir_to_sun;

		gpuSkyConstants.starlight			=(const float*)(starlight);
		
		gpuSkyConstants.hazeEccentricity	=1.0;
		gpuSkyConstants.mieRayleighRatio	=(const float*)(skyInterface->GetMieRayleighRatio());
		gpuSkyConstants.emissivity			=emissivity;
		//float y_start=(float)start_texel/(float)new_density_gridsize;
		//float y_range=(float)(texels)/(float)new_density_gridsize;
		gpuSkyConstants.yRange				=vec2(0.f,1.f);

	}
	for(int i=0;i<3;i++)
	{
		finalLoss[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,(int)altitudes_km.size(),numElevations,numDistances,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
		finalInsc[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,(int)altitudes_km.size(),numElevations,numDistances,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
		finalSkyl[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,(int)altitudes_km.size(),numElevations,numDistances,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	}
	density_texture->SetResource(dens_tex.shaderResourceView);
	SIMUL_PROFILE_END
	SIMUL_PROFILE_START("GpuSkyGenerator 2")

	// divide the grid into blocks:
	static const int BLOCKWIDTH=8;

	int xy_size		=(int)altitudes_km.size()*numElevations;
	
	int start_step	=(start_texel*3)/numDistances;
	int end_step	=((start_texel+num_texels)*3+numDistances-1)/numDistances;

	int start_loss	=range(start_step			,0,xy_size);
	int end_loss	=range(end_step				,0,xy_size);
	int num_loss	=range(end_loss-start_loss	,0,xy_size);
	int subgrid=(num_loss+BLOCKWIDTH-1)/BLOCKWIDTH;
	
	if(subgrid>0)
	{
		simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalLoss[cycled_index]->unorderedAccessView);
		gpuSkyConstants.threadOffset=uint3(start_loss,0,0);
		gpuSkyConstants.Apply(m_pImmediateContext);
		V_CHECK(ApplyPass(m_pImmediateContext,lossComputeTechnique->GetPassByIndex(0)));
		m_pImmediateContext->Dispatch(subgrid,1,1);
		if(loss)
			finalLoss[cycled_index]->copyToMemory(m_pd3dDevice,m_pImmediateContext,loss,start_loss*subgrid,num_loss*subgrid);
	}
	int start_insc	=range(start_step-xy_size	,0,xy_size);
	int end_insc	=range(end_step-xy_size		,0,xy_size);
	int num_insc	=range(end_insc-start_insc	,0,xy_size);
	
	loss_texture->SetResource(finalLoss[cycled_index]->shaderResourceView);
	optical_depth_texture->SetResource(optd_tex.shaderResourceView);
	subgrid=(num_insc+BLOCKWIDTH-1)/BLOCKWIDTH;
	if(subgrid>0)
	{
		simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalInsc[cycled_index]->unorderedAccessView);
		gpuSkyConstants.threadOffset=uint3(start_insc,0,0);
		gpuSkyConstants.Apply(m_pImmediateContext);
		V_CHECK(ApplyPass(m_pImmediateContext,inscComputeTechnique->GetPassByIndex(0)));
		m_pImmediateContext->Dispatch(subgrid,1,1);
		if(insc)
			finalInsc[cycled_index]->copyToMemory(m_pd3dDevice,m_pImmediateContext,insc,start_insc*subgrid,num_insc*subgrid);
	}
	SIMUL_PROFILE_END
	SIMUL_PROFILE_START("GpuSkyGenerator 3")
	int start_skyl	=range(start_step-2*xy_size	,0	,xy_size);
	int end_skyl	=range(end_step-2*xy_size	,0	,xy_size);
	int num_skyl	=range(end_skyl-start_skyl	,0	,xy_size);
	insc_texture->SetResource(finalInsc[cycled_index]->shaderResourceView);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalSkyl[cycled_index]->unorderedAccessView);
	gpuSkyConstants.threadOffset=uint3(start_skyl,0,0);
	gpuSkyConstants.Apply(m_pImmediateContext);
	V_CHECK(ApplyPass(m_pImmediateContext,skylComputeTechnique->GetPassByIndex(0)));
	subgrid=(num_skyl+BLOCKWIDTH-1)/BLOCKWIDTH;
	if(subgrid>0)
	{
		m_pImmediateContext->Dispatch(subgrid,1,1);
		if(skyl)
			finalSkyl[cycled_index]->copyToMemory(m_pd3dDevice,m_pImmediateContext,skyl,start_skyl*subgrid,num_skyl*subgrid);
	}
	density_texture->SetResource(NULL);
	input_texture->SetResource(NULL);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",NULL);
	loss_texture->SetResource(NULL);
	insc_texture->SetResource(NULL);
	V_CHECK(ApplyPass(m_pImmediateContext,skylComputeTechnique->GetPassByIndex(0)));
	
	SIMUL_PROFILE_END
}
