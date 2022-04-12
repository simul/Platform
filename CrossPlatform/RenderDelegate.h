#pragma once
#include <functional>

namespace platform
{
	namespace crossplatform
	{
		struct GraphicsDeviceContext;

		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void(crossplatform::GraphicsDeviceContext&)> RenderDelegate;
		typedef std::function<void(void*)> StartupDeviceDelegate;
		typedef std::function<void()> ShutdownDeviceDelegate;
	}
}