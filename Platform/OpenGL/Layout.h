#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
namespace simul
{
	namespace opengl
	{
        //! Holds current vertex buffer layout information:
		class Layout:public crossplatform::Layout
		{
		public:
			        Layout();
			virtual ~Layout();
			void    InvalidateDeviceObjects();
			void    Apply(crossplatform::DeviceContext& deviceContext);
			void    Unapply(crossplatform::DeviceContext& deviceContext);
		};

	}
}