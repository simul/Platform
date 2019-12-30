#pragma once
#include <functional>

namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;

		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void(crossplatform::DeviceContext&)> RenderDelegate;
		typedef std::function<void(void*)> StartupDeviceDelegate;
		typedef std::function<void()> ShutdownDeviceDelegate;
	}
}