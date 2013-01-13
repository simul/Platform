
#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include <math.h>
using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
}

void GpuCloudGenerator::CreateVolumeNoiseTexture(int size,const float *src_ptr)
{
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
}

void GpuCloudGenerator::RecompileShaders()
{
}
int GpuCloudGenerator::GetDensityGridsize(const int *grid){return 0;}
void *GpuCloudGenerator::FillDensityGrid(const int *grid
									,int start_texel
									,int texels
									,float humidity
									,float time
									,int noise_size,int octaves,float persistence
									,const float  *noise_src_ptr)
{
return NULL;
}