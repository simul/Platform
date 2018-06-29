#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif
struct IDXGISwapChain;
namespace simul
{
	namespace crossplatform
	{
		/// A SwapChain implements the connection between a rendering surface (e.g. rendertarget Texture) and an onscreen window.
		class SIMUL_CROSSPLATFORM_EXPORT SwapChain
		{
		protected:
			uint2 size;
			int bufferCount;
			cp_hwnd hwnd;
			PixelFormat pixelFormat;
			RenderPlatform *renderPlatform;
			bool fullscreen;
		public:
			SwapChain();
			virtual ~SwapChain();
			virtual void InvalidateDeviceObjects();
			/// Set the size and format
			virtual void RestoreDeviceObjects(RenderPlatform *r,DeviceContext &deviceContext,cp_hwnd h,PixelFormat,int count);
			virtual bool IsFullscreen() const
			{
				return fullscreen;
			}
			virtual void SetFullscreen(bool)
			{
			}
			virtual IDXGISwapChain *AsDXGISwapChain()
			{
				return nullptr;
			}
			uint2 GetSize() const
			{
				return size;
			}
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif