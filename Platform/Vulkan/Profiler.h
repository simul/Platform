#pragma once
#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include <string>
#include <map>

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
typedef unsigned int GLuint;
namespace simul
{
	namespace vulkan
	{
		SIMUL_VULKAN_EXPORT_CLASS Profiler:public simul::crossplatform::GpuProfiler
		{
		public:
			static Profiler &GetGlobalProfiler();
			void Initialize(void*);
			void Uninitialize();
			void Begin(crossplatform::DeviceContext &deviceContext,const char *name) override;
			virtual void End(crossplatform::DeviceContext &deviceContext) override;
			
			void StartFrame(crossplatform::DeviceContext &deviceContext) override;
			void EndFrame(crossplatform::DeviceContext &deviceContext) override;
			
		protected:
			std::vector<unsigned> query_stack;
			static Profiler GlobalProfiler;
			// Constants
			static const unsigned int QueryLatency = 5;
			struct ProfileData
			{
				~ProfileData();
				GLuint queryID[2];
				GLuint TimestampQuery[QueryLatency];
				bool QueryStarted;
				bool QueryFinished;
				float time;
				ProfileData()
					:QueryStarted(false)
					,QueryFinished(false)
					, time(0.f)
				{
					for(int i=0;i<QueryLatency;i++)
					{
						TimestampQuery[i]	=0;
					}
				}
			};
			typedef std::map<std::string, ProfileData> ProfileMap;

			ProfileMap profiles;
			unsigned int currFrame;
			std::string last_name;
			simul::base::Timer timer;
			std::string output;
		};
		class ProfileBlock
		{
		public:

			ProfileBlock(crossplatform::DeviceContext &deviceContext,const char *name);
			~ProfileBlock();
			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			std::string name;
			crossplatform::DeviceContext &deviceContext;
		};

	}
}