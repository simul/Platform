#include "Profiler.h"

using namespace platform;
using namespace opengl;

Profiler::~Profiler()
{

}

void Profiler::Begin(crossplatform::DeviceContext& deviceContext, const char* name)
{
	crossplatform::GpuProfiler::Begin(deviceContext, name);
}

void Profiler::End(crossplatform::DeviceContext& deviceContext)
{
	crossplatform::GpuProfiler::End(deviceContext);
}

void Profiler::StartFrame(crossplatform::DeviceContext& deviceContext)
{
	crossplatform::GpuProfiler::StartFrame(deviceContext);
}

void Profiler::EndFrame(crossplatform::DeviceContext& deviceContext)
{
	crossplatform::GpuProfiler::EndFrame(deviceContext);
}