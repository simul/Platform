#include "GpuProfiler.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <stdint.h>

using namespace simul;
using namespace dx12;

using std::string;
using std::map;

GpuProfiler::~GpuProfiler()
{

}

void GpuProfiler::Begin(crossplatform::DeviceContext &deviceContext,const char *name)
{
	crossplatform::GpuProfiler::Begin(deviceContext, name);
}

void GpuProfiler::End(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::GpuProfiler::End(deviceContext);
}

void GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::GpuProfiler::StartFrame(deviceContext);
}

void GpuProfiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::GpuProfiler::EndFrame(deviceContext);
}

