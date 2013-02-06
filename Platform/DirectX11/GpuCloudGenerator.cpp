
#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include <math.h>
using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
			:m_pd3dDevice(NULL)
			,m_pImmediateContext(NULL)
			,effect(NULL)
			,densityTechnique(NULL)
			,lightingTechnique(NULL)
			,transformTechnique(NULL)
			,iformat(DXGI_FORMAT_R32_FLOAT)
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::CreateVolumeNoiseTexture(int size,const float *src_ptr)
{
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	RecompileShaders();
	fb[0].RestoreDeviceObjects(m_pd3dDevice);
	fb[1].RestoreDeviceObjects(m_pd3dDevice);
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,L"simul_gpu_clouds.fx");
	if(effect)
	{
		densityTechnique	=effect->GetTechniqueByName("simul_gpu_density");
		lightingTechnique	=effect->GetTechniqueByName("simul_gpu_lighting");
		transformTechnique	=effect->GetTechniqueByName("simul_gpu_transform");
	}
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	int size=1;
	if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
		size=4;
	return grid[0]*grid[1]*grid[2]*size;
}

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