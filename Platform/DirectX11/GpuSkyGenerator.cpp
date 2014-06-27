#include "GpuSkyGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include "Simul/Platform/CrossPlatform/SL/simul_gpu_sky.sl"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"
using namespace simul;
using namespace dx11;

GpuSkyGenerator::GpuSkyGenerator()
	:m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,effect(NULL)
	,constantBuffer(NULL)
	,tables_checksum(0)
	,light_table(NULL)
{
	SetDirectTargets(NULL,NULL,NULL,NULL);
	Enabled=true;
}

GpuSkyGenerator::~GpuSkyGenerator()
{
	InvalidateDeviceObjects();
}

void GpuSkyGenerator::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
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
	dens_tex.InvalidateDeviceObjects();
	optd_tex.InvalidateDeviceObjects();
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
		lightComputeTechnique	=effect->GetTechniqueByName("gpu_light_table_compute");
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
	return m_pd3dDevice!=NULL;
}

void GpuSkyGenerator::MakeLossAndInscatterTextures(
				int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const sky::GpuSkyParameters &p
				,const sky::GpuSkyAtmosphereParameters &a
				,const sky::GpuSkyInfraredParameters &ir
				)
{
	if(p.fill_up_to_texels<0)
		return;
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pImmediateContext;
	deviceContext.renderPlatform=renderPlatform;
	if(keyframe_checksums[cycled_index]!=p.keyframe_checksum)
	{
		keyframe_checksums[cycled_index]	=p.keyframe_checksum;
		fadeTexIndex[cycled_index]=0;
	}
	SIMUL_COMBINED_PROFILE_START(m_pImmediateContext,"GpuSkyGenerator init")
	HRESULT hr=S_OK;
	int gridsize			=(int)p.altitudes_km.size()*p.numElevations*p.numDistances;
	int gridsize_2d			=(int)p.altitudes_km.size()*p.numElevations;
	if(dens_tex.width!=a.table_size||optd_tex.width!=a.table_size)
		tables_checksum=0;
	dens_tex.ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,a.table_size,1,crossplatform::RGBA_32_FLOAT,false);
	optd_tex.ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,a.table_size,a.table_size,crossplatform::RGBA_32_FLOAT,false,false);

	if(a.tables_checksum!=tables_checksum)
	{
		dens_tex.setTexels(m_pImmediateContext,(unsigned*)a.density_table,0,a.table_size);
		optd_tex.setTexels(m_pImmediateContext,(unsigned *)a.optical_table,0,a.table_size*a.table_size);
		tables_checksum=a.tables_checksum;
	}
	SIMUL_COMBINED_PROFILE_END(m_pImmediateContext)
	SIMUL_COMBINED_PROFILE_START(m_pImmediateContext,"GpuSkyGenerator 1")

	//ID3D1xEffectScalarVariable *distKm							=effect->GetVariableByName("distKm")->AsScalar();
	//ID3D1xEffectScalarVariable *prevDistKm						=effect->GetVariableByName("prevDistKm")->AsScalar();
	ID3D1xEffectShaderResourceVariable*	input_texture			=effect->GetVariableByName("input_texture")->AsShaderResource();
	ID3DX11EffectUnorderedAccessViewVariable *targetTexture		=effect->GetVariableByName("targetTexture")->AsUnorderedAccessView();
	ID3DX11EffectShaderResourceVariable* optical_depth_texture	=effect->GetVariableByName("optical_depth_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	density_texture			=effect->GetVariableByName("density_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	loss_texture			=effect->GetVariableByName("loss_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	insc_texture			=effect->GetVariableByName("insc_texture")->AsShaderResource();
	
	float maxOutputAltKm	=p.altitudes_km[p.altitudes_km.size()-1];
	{
		gpuSkyConstants.texSize				=vec2((float)p.altitudes_km.size(),(float)p.numElevations);
		static float tto=0.5f;
		gpuSkyConstants.texelOffset			=tto;
		gpuSkyConstants.tableSize			=vec2((float)a.table_size,(float)a.table_size);
		
		gpuSkyConstants.maxDistanceKm		=p.max_distance_km;
		
		gpuSkyConstants.planetRadiusKm		=skyInterface->GetPlanetRadius();
		gpuSkyConstants.maxOutputAltKm		=maxOutputAltKm;
		gpuSkyConstants.maxDensityAltKm		=a.maxDensityAltKm;
		gpuSkyConstants.hazeBaseHeightKm	=p.physical.hazeStruct.haze_base_height_km;
		gpuSkyConstants.hazeScaleHeightKm	=p.physical.hazeStruct.haze_scale_height_km;

		gpuSkyConstants.rayleigh			=(const float*)skyInterface->GetRayleigh();
		gpuSkyConstants.hazeMie				=(const float*)(p.physical.hazeStruct.haze*p.physical.hazeStruct.mie);
		gpuSkyConstants.ozone				=(const float*)(p.physical.ozone);

		gpuSkyConstants.sunIrradiance		=(const float*)p.physical.sun_irradiance;
		gpuSkyConstants.lightDir			=(const float*)p.physical.dir_to_sun;
		gpuSkyConstants.directionToMoon		=(const float*)p.physical.dir_to_moon;
		gpuSkyConstants.starlight			=(const float*)(p.physical.starlight);
		
		gpuSkyConstants.hazeEccentricity	=1.0;
		gpuSkyConstants.mieRayleighRatio	=(const float*)(skyInterface->GetMieRayleighRatio());
		gpuSkyConstants.emissivity			=ir.emissivity;
		gpuSkyConstants.yRange				=vec2(0.f,1.f);

	}
	//SetGpuSkyConstants(gpuSkyConstants,p,a,ir);
	for(int i=0;i<3;i++)
	{
		finalLoss[i]->ensureTexture3DSizeAndFormat(renderPlatform,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true,1);
		finalInsc[i]->ensureTexture3DSizeAndFormat(renderPlatform,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true,1);
		finalSkyl[i]->ensureTexture3DSizeAndFormat(renderPlatform,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true,1);
	}
	if(light_table)
		light_table->ensureTexture3DSizeAndFormat(renderPlatform,(int)p.altitudes_km.size()*32,3,4,crossplatform::RGBA_32_FLOAT,true);
	density_texture->SetResource(dens_tex.shaderResourceView);
	SIMUL_COMBINED_PROFILE_END(m_pImmediateContext)
	SIMUL_COMBINED_PROFILE_START(m_pImmediateContext,"GpuSkyGenerator 2")

	// divide the grid into blocks:
	static const int BLOCKWIDTH=8;

	int xy_size		=(int)p.altitudes_km.size()*p.numElevations;
	
	int start_step	=(fadeTexIndex[cycled_index]*3)/p.numDistances;
	int end_step	=((p.fill_up_to_texels)*3+p.numDistances-1)/p.numDistances;

	int start_loss	=range(start_step			,0,xy_size);
	int end_loss	=range(end_step				,0,xy_size);
	int num_loss	=range(end_loss-start_loss	,0,xy_size);
	int subgrid=(num_loss+BLOCKWIDTH-1)/BLOCKWIDTH;
	
	if(subgrid>0)
	{
		simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalLoss[cycled_index]->AsD3D11UnorderedAccessView());
		gpuSkyConstants.threadOffset=uint3(start_loss,0,0);
		gpuSkyConstants.Apply(deviceContext);
		V_CHECK(ApplyPass(m_pImmediateContext,lossComputeTechnique->GetPassByIndex(0)));
		m_pImmediateContext->Dispatch(subgrid,1,1);
	}
	int start_insc	=range(start_step-xy_size	,0,xy_size);
	int end_insc	=range(end_step-xy_size		,0,xy_size);
	int num_insc	=range(end_insc-start_insc	,0,xy_size);
	
	loss_texture->SetResource(finalLoss[cycled_index]->AsD3D11ShaderResourceView());
	optical_depth_texture->SetResource(optd_tex.shaderResourceView);
	subgrid=(num_insc+BLOCKWIDTH-1)/BLOCKWIDTH;
	if(subgrid>0)
	{
		simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalInsc[cycled_index]->AsD3D11UnorderedAccessView());
		gpuSkyConstants.threadOffset=uint3(start_insc,0,0);
		gpuSkyConstants.Apply(deviceContext);
		V_CHECK(ApplyPass(m_pImmediateContext,inscComputeTechnique->GetPassByIndex(0)));
		m_pImmediateContext->Dispatch(subgrid,1,1);
	}
	SIMUL_COMBINED_PROFILE_END(m_pImmediateContext)
	SIMUL_COMBINED_PROFILE_START(m_pImmediateContext,"GpuSkyGenerator 3")
	int start_skyl	=range(start_step-2*xy_size	,0	,xy_size);
	int end_skyl	=range(end_step-2*xy_size	,0	,xy_size);
	int num_skyl	=range(end_skyl-start_skyl	,0	,xy_size);
	insc_texture->SetResource(finalInsc[cycled_index]->AsD3D11ShaderResourceView());
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalSkyl[cycled_index]->AsD3D11UnorderedAccessView());
	gpuSkyConstants.threadOffset=uint3(start_skyl,0,0);
	gpuSkyConstants.Apply(deviceContext);
	V_CHECK(ApplyPass(m_pImmediateContext,skylComputeTechnique->GetPassByIndex(0)));
	subgrid=(num_skyl+BLOCKWIDTH-1)/BLOCKWIDTH;
	if(subgrid>0)
	{
		m_pImmediateContext->Dispatch(subgrid,1,1);
	}
	//light_table
	{
		int x_size				=light_table->width;
		int start_light			=range(start_step-2*xy_size	,0,x_size);
		int end_light			=range(end_step-2*xy_size	,0,x_size);
		int num_light			=range(end_light-start_light,0,x_size);
		loss_texture			->SetResource(finalLoss[cycled_index]->AsD3D11ShaderResourceView());
		optical_depth_texture	->SetResource(optd_tex.shaderResourceView);
		insc_texture			->SetResource(finalInsc[cycled_index]->AsD3D11ShaderResourceView());
		if(num_light>0)
		{
			simul::dx11::setUnorderedAccessView(effect,"targetTexture",light_table->AsD3D11UnorderedAccessView());
			gpuSkyConstants.threadOffset	=uint3(start_light,cycled_index,0);
			gpuSkyConstants.Apply(deviceContext);
			V_CHECK(ApplyPass(m_pImmediateContext,lightComputeTechnique->GetPassByIndex(0)));
			m_pImmediateContext->Dispatch(num_light,1,1);
		}
	}
	density_texture->SetResource(NULL);
	input_texture->SetResource(NULL);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",NULL);
	loss_texture->SetResource(NULL);
	insc_texture->SetResource(NULL);
	V_CHECK(ApplyPass(m_pImmediateContext,skylComputeTechnique->GetPassByIndex(0)));
	
	fadeTexIndex[cycled_index]=p.fill_up_to_texels;

	SIMUL_COMBINED_PROFILE_END(m_pImmediateContext)
}

void GpuSkyGenerator::CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl)
{
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pImmediateContext;
	deviceContext.renderPlatform=renderPlatform;
	int size=finalLoss[cycled_index]->depth*finalLoss[cycled_index]->width*finalLoss[cycled_index]->length;
	if(loss)
		finalLoss[cycled_index]->copyToMemory(deviceContext,loss,0,size);
	if(insc)
		finalInsc[cycled_index]->copyToMemory(deviceContext,insc,0,size);	
	if(skyl)
		finalSkyl[cycled_index]->copyToMemory(deviceContext,skyl,0,size);
}