#pragma once
#include "Platform/CrossPlatform/Export.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		class SIMUL_CROSSPLATFORM_EXPORT RenderDocLoader
		{
		public:
			static void Load();
			static void Unload();
			static void TriggerMultiFrameCapture(unsigned num);
			static void StartCapture(RenderPlatform* renderPlatform, void* windowHandlePtr);
			static void FinishCapture();
		};
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif