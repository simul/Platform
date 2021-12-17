
#include "Platform/Core/RuntimeError.h"

#if SIMUL_INTERNAL_CHECKS
bool platform::core::SimulInternalChecks = true;
#else
bool platform::core::SimulInternalChecks = false;
#endif

namespace platform
{
	namespace core
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