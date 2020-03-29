
#include "Platform/Core/RuntimeError.h"

#if SIMUL_INTERNAL_CHECKS
bool simul::base::SimulInternalChecks = true;
#else
bool simul::base::SimulInternalChecks = false;
#endif

namespace simul
{
	namespace base
	{
		static bool debugBreaksEnabled = true;
		bool DebugBreaksEnabled()
		{
			return debugBreaksEnabled;
		}
		void EnableDebugBreaks(bool b)
		{
			debugBreaksEnabled = b;
		}
#ifndef _MSC_VER
		void DebugBreak()
		{
			if (debugBreaksEnabled)
			{
				std::cout << "Break here.\n";
			}
		}
#endif
	}
}