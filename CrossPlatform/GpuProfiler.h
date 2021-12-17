#ifndef SIMUL_PLATFORM_CROSSPLATFORM_GPUPROFILER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_GPUPROFILER_H
#include "Platform/Core/ProfilingInterface.h"
#include "Platform/CrossPlatform/Export.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace core
	{
		struct ProfileData;
	}
}

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct Query;
		struct DeviceContext;
		//! A virtual interface base class for GPU performance measurement.
		class SIMUL_CROSSPLATFORM_EXPORT GpuProfilingInterface:public platform::core::BaseProfilingInterface
		{
		public:
			//! Platform-dependent function called when uninitializing the profiler.
			virtual void InvalidateDeviceObjects() =0;
			//! Mark the start of a render profiling block. Some API's require a context pointer.
			virtual void Begin(crossplatform::DeviceContext &deviceContext,const char *)=0;
			//! Mark the end of the current render profiling block.
			virtual void End(crossplatform::DeviceContext &deviceContext)=0;
			//! Call this at the start of the frame to reset values.
			using BaseProfilingInterface::StartFrame; //Warning suppressor
			virtual void StartFrame(crossplatform::DeviceContext &deviceContext)=0;
			//! Call this at the end of the frame to prepare the data to be read.
			virtual void EndFrame(crossplatform::DeviceContext &deviceContext)=0;
			//! Get the timing text per-frame.
			virtual const char *GetDebugText(platform::core::TextStyle st = platform::core::PLAINTEXT) const=0;
		};
		/// Set the GPU profilerFuture use of SIMUL_GPU_PROFILE_START or SIMUL_COMBINED_PROFILE_START will use that profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p);
		/// Returns a pointer to the current GPU profiler.
		extern SIMUL_CROSSPLATFORM_EXPORT GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context);
		
		struct SIMUL_CROSSPLATFORM_EXPORT ProfileData:public platform::core::ProfileData
		{
			simul::crossplatform::Query *DisjointQuery;
			simul::crossplatform::Query *TimestampStartQuery;
			simul::crossplatform::Query *TimestampEndQuery;
			ProfileData();
			~ProfileData();
		};
		/*!
		The Simul GPU profiler. Usage is as follows:

		* On initialization, when you have a device pointer:
		
				simul::crossplatform::GpuProfiler::GetGlobalProfiler().Initialize(pd3dDevice);

		* On shutdown, or whenever the device is changed or lost:

				simul::crossplatform::GpuProfiler::GetGlobalProfiler().Uninitialize();

		* Per-frame, at the start of the frame:

				platform::core::SetGpuProfilingInterface(deviceContext.platform_context,&simul::crossplatform::GpuProfiler::GetGlobalProfiler());
				SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext.platform_context)

		*  Wrap these around anything you want to measure:

				SIMUL_COMBINED_PROFILE_START(deviceContext,"Element name")
				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* At frame-end:

				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* To obtain the profiling results - pass true if you want HTML output:

				const char *text=simul::dx11::Profiler::GetGlobalProfiler().GetDebugText(as_html);
		*/
		class SIMUL_CROSSPLATFORM_EXPORT GpuProfiler:public GpuProfilingInterface
		{
		public:
			GpuProfiler();
			virtual ~GpuProfiler();
		public:
			virtual ProfileData *CreateProfileData() const override
			{
				return new ProfileData;
			}
			//! Platform-dependent function called when initializing the profiler.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Platform-dependent function called when uninitializing the profiler.
			virtual void InvalidateDeviceObjects() override;
			virtual void Begin(crossplatform::DeviceContext &deviceContext,const char *) override;
			virtual void End(crossplatform::DeviceContext &deviceContext) override;
			/// Call this before any timeable events in a frame.
			virtual void StartFrame(crossplatform::DeviceContext &deviceContext) override;
			/// Call this after all timeable events in a frame have completed.
			virtual void EndFrame(crossplatform::DeviceContext &deviceContext) override;
			virtual const char *GetDebugText(platform::core::TextStyle st = platform::core::PLAINTEXT) const override;
		
			const platform::core::ProfileData *GetEvent(const platform::core::ProfileData *parent,int i) const override;
			std::string GetChildText(const char *name,std::string tab) const;

			float GetTime(const std::string &name) const;

		protected:
			virtual void InitQuery(Query *);
			void WalkEndFrame(crossplatform::DeviceContext &deviceContext,crossplatform::ProfileData *p);
			std::string Walk(platform::core::ProfileData *p, int tab, float parent_time, platform::core::TextStyle style) const;
			__int64 currFrame;
			platform::core::Timer timer;
			float queryTime;
			crossplatform::RenderPlatform *renderPlatform;
			bool enabled;
			long long current_framenumber=-1;
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
		extern SIMUL_CROSSPLATFORM_EXPORT void ClearGpuProfilers();
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
				gpuProfilingInterface->End(ctx);}
		
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
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif