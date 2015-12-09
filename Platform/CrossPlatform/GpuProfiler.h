#ifndef SIMUL_PLATFORM_CROSSPLATFORM_GPUPROFILER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_GPUPROFILER_H
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/CrossPlatform/Export.h"

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct Query;
		struct DeviceContext;
		//! A virtual interface base class for GPU performance measurement.
		class SIMUL_CROSSPLATFORM_EXPORT GpuProfilingInterface:public base::BaseProfilingInterface
		{
		public:
			//! Mark the start of a render profiling block. Some API's require a context pointer.
			virtual void Begin(crossplatform::DeviceContext &deviceContext,const char *)=0;
			//! Mark the end of the current render profiling block.
			virtual void End()=0;
			//! Call this at the start of the frame to reset values.
			virtual void StartFrame(crossplatform::DeviceContext &deviceContext)=0;
			//! Call this at the end of the frame to prepare the data to be read.
			virtual void EndFrame(crossplatform::DeviceContext &deviceContext)=0;
			//! Get the timing text per-frame.
			virtual const char *GetDebugText(base::TextStyle st = base::PLAINTEXT) const=0;
			//! Call this to set the maximum level of the profiling tree.
			virtual void SetMaxLevel(int m)
			{
				max_level=m;
			}
			//! Call this to get the maximum level of the profiling tree.
			virtual int GetMaxLevel() const
			{
				return max_level;
			}
		protected:
			int max_level;				// Maximum level of nesting.
		};
		/// Set the GPU profilerFuture use of SIMUL_GPU_PROFILE_START or SIMUL_COMBINED_PROFILE_START will use that profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p);
		/// Returns a pointer to the current GPU profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context);
		
		struct ProfileData;
		typedef std::map<std::string,crossplatform::ProfileData*> ProfileMap;
		typedef std::map<int,crossplatform::ProfileData*> ChildMap;
		struct SIMUL_CROSSPLATFORM_EXPORT ProfileData:public base::ProfileData
		{
				simul::crossplatform::Query *DisjointQuery;
				simul::crossplatform::Query *TimestampStartQuery;
				simul::crossplatform::Query *TimestampEndQuery;
				ProfileData();
				~ProfileData();
			ChildMap children;
		};
		SIMUL_CROSSPLATFORM_EXPORT_CLASS GpuProfiler:public GpuProfilingInterface
		{
		public:
			GpuProfiler();
			virtual ~GpuProfiler();
		public:
			/// Call this when the profiler is to be initialized with a device pointer - must be done before use.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			/// Call this when the profiler is to be shut-down, or the device pointer has been lost or changed.
			virtual void InvalidateDeviceObjects();
			virtual void Begin(crossplatform::DeviceContext &deviceContext,const char *) override;
			virtual void End() override;
			virtual void StartFrame(crossplatform::DeviceContext &deviceContext) override;
			virtual void EndFrame(crossplatform::DeviceContext &deviceContext) override;
			virtual const char *GetDebugText(simul::base::TextStyle st = simul::base::PLAINTEXT) const override;
		
			const base::ProfileData *GetEvent(const base::ProfileData *parent,int i) const;
			std::string GetChildText(const char *name,std::string tab) const;
		protected:
			std::string Walk(crossplatform::ProfileData *p, int tab, float parent_time, base::TextStyle style) const;
			int level;
			__int64 currFrame;
			simul::base::Timer timer;
			float queryTime;
			crossplatform::RenderPlatform *renderPlatform;
			ProfileMap profileMap;
bool enabled;
			std::vector<std::string> last_name;
			std::vector<crossplatform::DeviceContext *> last_context;
		};
	}
}

#ifdef SIMUL_INTERNAL_PROFILING
		
		#define SIMUL_GPU_PROFILE_STARTFRAME(ctx) \
			{simul::crossplatform::GpuProfilingInterface *gpuProfilingInterface=simul::crossplatform::GetGpuProfilingInterface(ctx); \
			if(gpuProfilingInterface) \
				gpuProfilingInterface->StartFrame(ctx);}
		#define SIMUL_GPU_PROFILE_ENDFRAME(ctx) \
			{simul::crossplatform::GpuProfilingInterface *gpuProfilingInterface=simul::crossplatform::GetGpuProfilingInterface(ctx); \
			if(gpuProfilingInterface) \
				gpuProfilingInterface->EndFrame(ctx);}
		
		#define SIMUL_GPU_PROFILE_START(ctx,name) \
			{simul::crossplatform::GpuProfilingInterface *gpuProfilingInterface=simul::crossplatform::GetGpuProfilingInterface(ctx); \
			if(gpuProfilingInterface) \
				gpuProfilingInterface->Begin(ctx,name);}
		#define SIMUL_GPU_PROFILE_END(ctx) \
			{simul::crossplatform::GpuProfilingInterface *gpuProfilingInterface=simul::crossplatform::GetGpuProfilingInterface(ctx); \
			if(gpuProfilingInterface) \
				gpuProfilingInterface->End();}
		
		#define SIMUL_COMBINED_PROFILE_START(ctx,name) \
			SIMUL_GPU_PROFILE_START(ctx,name) \
			SIMUL_PROFILE_START(name)
		#define SIMUL_COMBINED_PROFILE_END(ctx) \
			SIMUL_GPU_PROFILE_END(ctx) \
			SIMUL_PROFILE_END
		
		#define SIMUL_COMBINED_PROFILE_STARTFRAME(ctx) \
			SIMUL_GPU_PROFILE_STARTFRAME(ctx) \
			SIMUL_PROFILE_STARTFRAME
		#define SIMUL_COMBINED_PROFILE_ENDFRAME(ctx) \
			SIMUL_GPU_PROFILE_ENDFRAME(ctx) \
			SIMUL_PROFILE_ENDFRAME

		#define SIMUL_DISABLE_PROFILING \
			#undef SIMUL_PROFILE_START \
			#undef SIMUL_PROFILE_END \
			#undef SIMUL_GPU_PROFILE_START \
			#undef SIMUL_GPU_PROFILE_END \
			#define SIMUL_PROFILE_START(name) \
			#define SIMUL_PROFILE_END \
			#define SIMUL_GPU_PROFILE_START(ctx,name) \
			#define SIMUL_GPU_PROFILE_END
#else
		#define SIMUL_PROFILE_STARTFRAME
		#define SIMUL_PROFILE_ENDFRAME
		/// Call this at the start of a block to be profiled.
		#define SIMUL_PROFILE_START(name)
		/// Call this at the end of a block to be profiled - must match uses of SIMUL_PROFILE_START.
		#define SIMUL_PROFILE_END
		#define SIMUL_GPU_PROFILE_STARTFRAME(ctx)
		#define SIMUL_GPU_PROFILE_ENDFRAME(ctx)
		/// Call this at the start of a block to be profiled.
		#define SIMUL_GPU_PROFILE_START(ctx,name)
		/// Call this at the end of a block to be profiled - must match uses of SIMUL_GPU_PROFILE_START.
		#define SIMUL_GPU_PROFILE_END(ctx)
		/// Call this at the start of a block to be profiled.
		#define SIMUL_COMBINED_PROFILE_START(ctx,name)
		/// Call this at the end of a block to be profiled - must match uses of SIMUL_COMBINED_PROFILE_START.
		#define SIMUL_COMBINED_PROFILE_END(ctx)
		/// Call this at the start of the frame when using Simul profiling.
		#define SIMUL_COMBINED_PROFILE_STARTFRAME(ctx)
		/// Call this at the end of the frame when using Simul profiling.
		#define SIMUL_COMBINED_PROFILE_ENDFRAME(ctx)
		#define SIMUL_DISABLE_PROFILING
#endif
#endif