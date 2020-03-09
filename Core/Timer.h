#pragma once
#ifndef _MSC_VER	// Using Visual Studio? Then this pragma works:
#include <inttypes.h>
#define __int64 int64_t
#endif
#include "Platform/Core/Export.h"

namespace simul
{
	namespace core
	{
		struct Int64
		{
			unsigned int LowPart;
			int HighPart;
		};
		//!  \brief A microsecond timer.
		//!  Provides timing to microsecond accuracy; results are reported in milliseconds.
		class PLATFORM_CORE_EXPORT Timer
		{
		private:
			static __int64 OverheadTicks;			// overhead  in calling timer
		#ifdef __ORBIS__
			uint64_t iStart,iStop;
		#endif
		#ifdef _MSC_VER     
			Int64 iStart,iStop;
			int PerfFreqAdjust;			// in case Freq is too big
			float dPerfFreq;			// ticks per second
			__int64 Oht;
			int ReduceMag;
		#endif
			void EmptyFunction();
		public:
			float Time;
			float TimeSum;
			Timer();
			~Timer();
			//! Start timing: record the start of an event.
			void StartTime();
			//! Stop timing: record the end of an event. The value Time can now be read.
			float FinishTime();
			//! Update the value of Time and continue timing.
			float UpdateTime()
			{
				float t=FinishTime();
				StartTime();
				return t;
			}
			float AbsoluteTimeMS();
			//! Update the value of Time and continue timing.
			float UpdateTimeSum()
			{
				FinishTime();
				StartTime();
				return TimeSum;
			}
		};
	}
}

