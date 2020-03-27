#ifndef SIMUL_BASE_PROFILING_INTERFACE_H
#define SIMUL_BASE_PROFILING_INTERFACE_H
#include "Platform/Core/Export.h"
#include <stdio.h>
#include <cstdlib>

#include <map>
#include <string>
#include <stack>
#include <vector>
#include "Platform/Core/Timer.h"
#include "ThisPlatform/Threads.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#elif defined(NN_NINTENDO_SDK)
	#include <nn/os/os_ThreadApi.h>
	typedef nn::os::ThreadId THREAD_TYPE;
#elif defined(__ORBIS__)
	#include <kernel.h>
	typedef  ScePthread THREAD_TYPE;
#elif defined(__APPLE__)
	#include <pthread.h>
	#define THREAD_TYPE pthread_t
#endif
namespace simul
{
	namespace base
	{
		/// Get the id of the current thread.
		extern PLATFORM_CORE_EXPORT THREAD_TYPE GetThreadId();
	}
}
#define GET_THREAD_ID simul::base::GetThreadId 

//! Define this to enable internal calls to the profiler, if it has been set with simul::base::SetProfilingInterface().
#define SIMUL_INTERNAL_PROFILING

#ifdef DOXYGEN
#undef SIMUL_INTERNAL_PROFILING
#endif
namespace simul
{
	namespace base
	{
		struct ProfileData;
		typedef std::map<const char *,ProfileData*> ChildMap;
		struct PLATFORM_CORE_EXPORT ProfileData
		{
			ProfileData()
				:time(0.0f)
				,maxTime(0.0f)
				,frameTime(0.0f)
				,start(0.0f)
				,overhead(0.0f)
				,gotResults(false)
				,QueryStarted(false)
				,QueryFinished(false)
				,updatedThisFrame(false)
				,parent(NULL)
				,last_child_updated(0)
				//,child_index(0)
				,age(0)
			{
			}
			virtual ~ProfileData()
			{
			}
			std::string name;
			float time;			///< Time in ms taken by this event, including its children, in this instance.
			float maxTime;		///< The longest this event has taken in the present process.
			float frameTime;	///< Time in ms taken by this event, including all the times it occurs in a frame.

			float start;		// The last time it was started
			float overhead;		// Overhead, including its children.
			bool gotResults;

			bool QueryStarted;
			bool QueryFinished;
			bool updatedThisFrame;
			ProfileData *parent;
			std::string full_name;
			std::string unqualifiedName;
			std::wstring wUnqualifiedName;
			int last_child_updated;
			//..int child_index;
			int age;
			ChildMap children;
		};
		//! Style for text output.
		enum TextStyle
		{
			PLAINTEXT=0,HTML=1,RICHTEXT=2
		};
		//! A virtual interface base class for classes that can profile for performance.
		class PLATFORM_CORE_EXPORT BaseProfilingInterface
		{
		protected:
			virtual ProfileData *CreateProfileData() const=0;
			// The name stack is of pointers to const strings. So don't use temporary variables for profiling strings...
			std::vector<ProfileData *> profileStack;
			std::string Walk(ProfileData *profileData,int tab,float parent_time,TextStyle style) const;
			void WalkReset(ProfileData *p=nullptr);
		public:
			BaseProfilingInterface():max_level(0)
									,max_level_this_frame(0)
									,level(0)
									,level_in_use(0)
									,root(NULL)
			{
			}
			virtual ~BaseProfilingInterface()
			{
			}
			//! Get profile data for the event at index i. Returns NULL for i<0 or i>= number of events.
			//! Null parent means the top-level events.
			virtual const ProfileData *GetEvent(const ProfileData *,int ) const
			{
				return NULL;
			}
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
			/// Gets the profiling report as text.
			///
			/// \param	Determines if the text should be returned as HTML, including colour formatting.
			///
			/// \return	null if it fails, else the debug text.
			const char *GetDebugText(TextStyle st = PLAINTEXT) const;

			void Clear(base::ProfileData *p=nullptr);
			//! Call this at the start of the frame to reset values.
			virtual void StartFrame();
		protected:
			int max_level;				// Maximum level of nesting.
			int max_level_this_frame;
			int level;
			int level_in_use;
			bool frame_active=false;
			ProfileData *root;
		};
		//! simul::base::DefaultProfiler inherits from ProfilingInterface to measure CPU performance.
		class PLATFORM_CORE_EXPORT ProfilingInterface:public BaseProfilingInterface
		{
		public:
			//! Constructor
			ProfilingInterface(){}
			//! Mark the start of a profiling block.
			virtual void Begin(const char *)=0;
			//! Mark the end of a profiling block.
			virtual void End()=0;
			//! Call this at the start of the frame to reset values.
			virtual void EndFrame()=0;
		};
		/// Set the CPU profiler. Future use of SIMUL_PROFILE_START or SIMUL_COMBINED_PROFILE_START will use that profiler.
		extern PLATFORM_CORE_EXPORT void SetProfilingInterface(THREAD_TYPE thread_id,ProfilingInterface *p);
		/// Returns a pointer to the current CPU profiler.
		extern PLATFORM_CORE_EXPORT ProfilingInterface *GetProfilingInterface(THREAD_TYPE thread_id);

		//! A simple profiler using simul::core::Timer
		/*!  Usage is as follows:

		* Declare and initialize:

				simul::base::DefaultProfiler cpuProfiler;
				...
				simul::base::SetProfilingInterface(&cpuProfiler);

		* Per-frame, at the start of the frame:

				SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext.platform_context)

		*  Wrap these around anything you want to measure:

				SIMUL_COMBINED_PROFILE_START(deviceContext,"Element name")
				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* At frame-end:

				SIMUL_COMBINED_PROFILE_ENDFRAME(deviceContext)

		* To obtain the profiling results
		
				cpuProfiler.SetMaxLevel(any number from 0 up);
				const char *text=cpuProfiler.GetDebugText(false);

		and print this text to screen.
		*/
		class PLATFORM_CORE_EXPORT DefaultProfiler:public simul::base::ProfilingInterface
		{
		public:
			struct Timing:public ProfileData
			{
			};
			virtual ProfileData *CreateProfileData() const
			{
				return new Timing;
			}
			void ResetMaximums();
			float overhead;
			simul::core::Timer timer;
			DefaultProfiler();
			~DefaultProfiler();
			//! Call this at the start of the frame to set-up the profiler for data-gathering.
			void EndFrame();
			//! Mark the start of a profiling block.
			virtual void Begin(const char *txt);
			//! Allocate \a nbytes bytes of memory, aligned to \a align and return a pointer to them.
			virtual void End();
			float WalkOverhead(simul::base::DefaultProfiler::Timing *p,int level);
			bool GetCounter(int i,std::string &str,float &t);

			const ProfileData *GetEvent(const ProfileData *parent,int i) const;
		};
	}
}

#ifdef SIMUL_INTERNAL_PROFILING
		#define SIMUL_PROFILE_STARTFRAME \
			{\
				THREAD_TYPE thread_id=GET_THREAD_ID();\
				simul::base::ProfilingInterface *profilingInterface=simul::base::GetProfilingInterface(thread_id); \
				if(profilingInterface) \
					profilingInterface->StartFrame();\
			}

		#define SIMUL_PROFILE_ENDFRAME \
			{\
				THREAD_TYPE thread_id=GET_THREAD_ID();\
				simul::base::ProfilingInterface *profilingInterface=simul::base::GetProfilingInterface(thread_id); \
				if(profilingInterface) \
					profilingInterface->EndFrame();}

		#define SIMUL_PROFILE_START(name) \
			{\
				THREAD_TYPE thread_id=GET_THREAD_ID();\
				simul::base::ProfilingInterface *profilingInterface=simul::base::GetProfilingInterface(thread_id); \
				if(profilingInterface) \
					profilingInterface->Begin(name);}
		#define SIMUL_PROFILE_END \
			{\
				THREAD_TYPE thread_id=GET_THREAD_ID();\
				simul::base::ProfilingInterface *profilingInterface=simul::base::GetProfilingInterface(thread_id); \
				if(profilingInterface) \
					profilingInterface->End();}
#else
		#define SIMUL_PROFILE_STARTFRAME
		#define SIMUL_PROFILE_ENDFRAME
		/// Call this at the start of a block to be profiled.
		#define SIMUL_PROFILE_START(name)
		/// Call this at the end of a block to be profiled - must match uses of SIMUL_PROFILE_START.
		#define SIMUL_PROFILE_END
		#define SIMUL_DISABLE_PROFILING
#endif
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif