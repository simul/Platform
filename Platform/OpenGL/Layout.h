#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
namespace simul
{
	namespace opengl
	{
		class Layout:public crossplatform::Layout
		{
		public:
			Layout();
			virtual ~Layout();
		};

	}
}