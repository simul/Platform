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
			GLuint vao;
			Layout();
			virtual ~Layout();
			void InvalidateDeviceObjects();
			void Apply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);
		};

	}
}