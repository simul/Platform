#include "GpuProfiler.h"
using namespace simul;
using namespace crossplatform;
static std::map<void*,simul::crossplatform::GpuProfilingInterface*> gpuProfilingInterface;

namespace simul
{
	namespace crossplatform
	{
		void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p)
		{
			gpuProfilingInterface[&context]=p;
		}
		GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context)
		{
			if(gpuProfilingInterface.find(&context)==gpuProfilingInterface.end())
				return NULL;
			return gpuProfilingInterface[&context];
		}
	}
}

GpuProfiler::GpuProfiler()
{
}


GpuProfiler::~GpuProfiler()
{
}

void GpuProfiler::Begin(crossplatform::DeviceContext &deviceContext,const char *)
{
}

void GpuProfiler::End()
{
}

void GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
}

void GpuProfiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
}

const char *GpuProfiler::GetDebugText(simul::base::TextStyle st) const
{
	return NULL;
}