#pragma once
#include "Export.h"
#include "Platform/CrossPlatform/Layout.h"

namespace platform
{
	namespace vulkan
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