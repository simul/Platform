#ifdef _MSC_VER
#include <Windows.h>// for DebugBreak etc Include this at the cpp-level, too much to add into a header.
#endif
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
		bool IsDebuggerPresent()
		{
#ifdef _MSC_VER
			return ::IsDebuggerPresent();
#else
		return true;
#endif
		}
		void DebugBreak()
		{
#ifdef _MSC_VER
			::DebugBreak();
#else
			if (debugBreaksEnabled)
			{
				std::cout << "Break here.\n";
			}
#endif
		}
	}
}