#include "GpuSkyGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/DirectX11/HLSL/simul_gpu_sky.hlsl"
using namespace simul;
using namespace dx11;

GpuSkyGenerator::GpuSkyGenerator()
{
}

GpuSkyGenerator::~GpuSkyGenerator()
{
}

void GpuSkyGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	RecompileShaders();
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
	CreateEffect(m_pd3dDevice,&effect,L"simul_gpu_sky.fx");
	lossTechnique	=effect->GetTechniqueByName("simul_gpu_loss");
	inscTechnique	=effect->GetTechniqueByName("simul_gpu_insc");//"#define OVERCAST 1\r\n");
	skylTechnique	=effect->GetTechniqueByName("simul_gpu_skyl");
	
	MAKE_CONSTANT_BUFFER(constantBuffer,GpuSkyConstants);
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled;
}

void GpuSkyGenerator::Make2DLossAndInscatterTextures(simul::sky::AtmosphericScatteringInterface *skyInterface,int NumElevations,int NumDistances,
				simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 dir_to_sun,simul::sky::float4 dir_to_moon,float haze
				,float overcast,float overcast_base_km,float overcast_range_km
				,int index,int end_index,const simul::sky::float4 *density_table,const simul::sky::float4 *optical_table,int table_size,float maxDensityAltKm,bool InfraRed)
{
}
