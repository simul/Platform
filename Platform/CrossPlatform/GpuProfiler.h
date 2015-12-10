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
		};
		/// Set the GPU profilerFuture use of SIMUL_GPU_PROFILE_START or SIMUL_COMBINED_PROFILE_START will use that profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p);
		/// Returns a pointer to the current GPU profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context);
		
		struct ProfileData;
		typedef std::map<std::string,base::ProfileData*> ProfileMap;
		typedef std::map<int,base::ProfileData*> ChildMap;
		struct SIMUL_CROSSPLATFORM_EXPORT ProfileData:public base::ProfileData
		{
				simul::crossplatform::Query *DisjointQuery;
				simul::crossplatform::Query *TimestampStartQuery;
				simul::crossplatform::Query *TimestampEndQuery;
				ProfileData();
				~ProfileData();
			ChildMap children;
		};
		/*!
		The Simul GPU profiler. Usage is as follows:

		* On initialization, when you have a device pointer:
		
				simul::crossplatform::GpuProfiler::GetGlobalProfiler().Initialize(pd3dDevice);

		* On shutdown, or whenever the device is changed or lost:

				simul::crossplatform::GpuProfiler::GetGlobalProfiler().Uninitialize();

		* Per-frame, at the start of the frame:

				simul::base::SetGpuProfilingInterface(deviceContext.platform_context,&simul::crossplatform::GpuProfiler::GetGlobalProfiler());
				SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext.platform_context)

		*  Wrap these around anything you want to measure:

				SIMUL_COMBINED_PROFILE_START(deviceContext,"Element name")
				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* At frame-end:

				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* To obtain the profiling results - pass true if you want HTML output:

				const char *text=simul::dx11::Profiler::GetGlobalProfiler().GetDebugText(as_html);
		*/
		SIMUL_CROSSPLATFORM_EXPORT_CLASS GpuProfiler:public GpuProfilingInterface
		{
		public:
			GpuProfiler();
			virtual ~GpuProfiler();
		public:
			virtual ProfileData *CreateProfileData() const
			{
				return new ProfileData;
			}
			/// Call this when the profiler is to be initialized with a device pointer - must be done before use.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			/// Call this when the profiler is to be shut-down, or the device pointer has been lost or changed.
			virtual void InvalidateDeviceObjects();
			virtual void Begin(crossplatform::DeviceContext &deviceContext,const char *) override;
			virtual void End() override;
			/// Call this before any timeable events in a frame.
			virtual void StartFrame(crossplatform::DeviceContext &deviceContext) override;
			/// Call this after all timeable events in a frame have completed. It is acceptable
			/// to call EndFrame() without having first called StartFrame() - this has no effect.
			virtual void EndFrame(crossplatform::DeviceContext &deviceContext) override;
			virtual const char *GetDebugText(simul::base::TextStyle st = simul::base::PLAINTEXT) const override;
		
			const base::ProfileData *GetEvent(const base::ProfileData *parent,int i) const;
			std::string GetChildText(const char *name,std::string tab) const;

			float GetTime(const std::string &name) const;

		protected:
			std::string Walk(base::ProfileData *p, int tab, float parent_time, base::TextStyle style) const;
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
		class ProfileBlock
		{
		public:

			ProfileBlock(crossplatform::DeviceContext &c,GpuProfiler *prof,const std::string& name);
			~ProfileBlock();

			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			GpuProfiler *profiler;
			crossplatform::DeviceContext* context;
			std::string name;
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
			SIMUL_PROFILE_END \
			SIMUL_GPU_PROFILE_END(ctx)
		
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