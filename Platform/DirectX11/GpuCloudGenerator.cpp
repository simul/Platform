#define NOMINMAX
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "D3dx11effect.h"
#include <math.h>

using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}
