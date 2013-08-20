#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		SIMUL_OPENGL_EXPORT_CLASS Profiler
		{
		public:
			static Profiler &GetGlobalProfiler();
			void Initialize(void*);
			void Uninitialize();
			void StartProfile(const std::string& name);
			void EndProfile(const std::string& name);

			void EndFrame();
			
			float GetTime(const std::string &name) const;
		protected:
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

			simul::base::Timer timer;
			std::string output;
		};
		class ProfileBlock
		{
		public:

			ProfileBlock(const std::string& name);
			~ProfileBlock();
			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			std::string name;
		};

	}
}