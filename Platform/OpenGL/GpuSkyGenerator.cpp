
#include "GpuSkyGenerator.h"
using namespace simul;
using namespace opengl;

GpuSkyGenerator::GpuSkyGenerator()
{
}

GpuSkyGenerator::~GpuSkyGenerator()
{
}

void GpuSkyGenerator::RestoreDeviceObjects(void *)
{
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
}

void GpuSkyGenerator::RecompileShaders()
{
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return false;
}

void GpuSkyGenerator::Make2DLossAndInscatterTextures(simul::sky::SkyInterface *skyInterface
				,int NumElevations,int NumDistances
				,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km,float max_distance_km,int index,int end_index,bool InfraRed)
{
// we will render to 
}