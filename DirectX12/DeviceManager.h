#pragma once
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/DirectX12/Export.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include <wrl/client.h>
#include "ThisPlatform/Direct3D12.h"

#include "SimulDirectXHeader.h"

#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable:4251)

#define cp_hwnd HWND

namespace simul
{
	namespace dx12
	{
		struct Window;
		typedef std::map<int, IDXGIOutput*> OutputMap;
		
		//! Manages the rendering device
		class SIMUL_DIRECTX12_EXPORT DeviceManager: public crossplatform::GraphicsDeviceInterface
		{
		public:
									DeviceManager();
									~DeviceManager();
			//! Initializes the manager, finds an adapter, checks feature level, creates a rendering device and a command queue
			void					Initialize(bool use_debug = false, bool instrument = false, bool default_driver = false);
			bool					IsActive() const;
			void					Shutdown();
			void*					GetDevice();
			void*					GetDeviceContext();
			
			void*					GetImmediateContext();
			void					FlushImmediateCommandList();

			int						GetNumOutputs();
			crossplatform::Output	GetOutput(int i);
			void					ReportMessageFilterState();

		protected:
			ImmediateContext		mIContext;
			//! Map of displays
			OutputMap				mOutputs;
			//! The D3D device
			ID3D12DeviceType*		mDevice;
			DWORD					mCallbackCookie;
		};
	}
}
#pragma warning(pop)
