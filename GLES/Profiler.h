#pragma once
#include "Platform/GLES/Export.h"
#include "Platform/Core/Timer.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace gles
	{
		struct ProfilingQuery
		{
			ProfilingQuery() :timestamp(NULL) {}
			~ProfilingQuery() {}
			unsigned long* timestamp;
		};

		class SIMUL_GLES_EXPORT Profiler: public simul::crossplatform::GpuProfiler
		{
		public:
			static Profiler&	GetGlobalProfiler();
								~Profiler();

			void				Begin(crossplatform::DeviceContext& deviceContext,const char* name);
			void				End(crossplatform::DeviceContext& deviceContext);

			void				StartFrame(crossplatform::DeviceContext& deviceContext);
			void				EndFrame(crossplatform::DeviceContext& deviceContext);
		};
	}
}