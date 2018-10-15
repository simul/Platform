#include "DisplaySurface.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <linux/input.h>
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vk_sdk_platform.h>

namespace simul
{
	namespace vulkan
	{
		struct SwapchainImageResources
		{
			vk::Image image;
			vk::CommandBuffer cmd;
			vk::CommandBuffer graphics_to_present_cmd;
			vk::ImageView view;
			vk::Buffer uniform_buffer;
			vk::DeviceMemory uniform_memory;
			vk::Framebuffer framebuffer;
			vk::DescriptorSet descriptor_set;
		} ;
	}
}

using namespace simul;
using namespace vulkan;

#ifdef _MSC_VER
static const char *GetErr()
{
	LPVOID lpMsgBuf;
	DWORD err = GetLastError();
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf,
		0,
		NULL
	);
	if (lpMsgBuf)
		return (const char *)lpMsgBuf;
	else
		return "";
}
#endif

DisplaySurface::DisplaySurface()
	:pixelFormat(crossplatform::UNKNOWN)
	, hDC(nullptr)
	, hRC(nullptr)
	, surface(nullptr)
	, swapchain(nullptr)
{
	surface = new vk::SurfaceKHR;
}

DisplaySurface::~DisplaySurface()
{
	InvalidateDeviceObjects();
	delete surface;
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* r, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
	if (mHwnd && mHwnd == handle)
	{
		return;
	}
	renderPlatform = r;
	pixelFormat = outFmt;
	crossplatform::DeviceContext &immediateContext = r->GetImmediateContext();
	mHwnd = handle;
#ifdef _MSC_VER
	if (!(hDC = GetDC(mHwnd)))
	{
		return;
	}
	static  PIXELFORMATDESCRIPTOR pfd =	// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// Size Of This Pixel Format Descriptor
		1,								// Version Number
		PFD_DRAW_TO_WINDOW |			// Format Must Support Window
		PFD_SUPPORT_OPENGL |			// Format Must Support OpenGL...
		PFD_DOUBLEBUFFER,				// Must Support Double Buffering
		PFD_TYPE_RGBA,					// Request An RGBA Format
		32,								// Select Our Color Depth
		0, 0, 0, 0, 0, 0,				// Color Bits Ignored
		0,								// No Alpha Buffer
		0,								// Shift Bit Ignored
		0,								// No Accumulation Buffer
		0, 0, 0, 0,						// Accumulation Bits Ignored
		0,								// 16Bit Z-Buffer (Depth Buffer)
		0,								// No Stencil Buffer
		0,								// No Auxiliary Buffer
		PFD_MAIN_PLANE,					// Main Drawing Layer
		0,								// Reserved
		0, 0, 0							// Layer Masks Ignored
	};
	uint glPixelFormat;

	vk::Win32SurfaceCreateInfoKHR createInfo = vk::Win32SurfaceCreateInfoKHR().setHinstance(GetModuleHandle(nullptr)).setHwnd(handle);
	auto rv = (vulkan::RenderPlatform *)renderPlatform;
	vk::Instance *inst = rv->AsVulkanInstance();
	vk::Result result = inst->createWin32SurfaceKHR(&createInfo, nullptr, surface);
	SIMUL_ASSERT(result == vk::Result::eSuccess);


#endif
}

void DisplaySurface::InvalidateDeviceObjects()
{
	if (hDC && !ReleaseDC(mHwnd, hDC))                    // Are We Able To Release The DC
	{
		SIMUL_CERR << "Release Device Context Failed." << GetErr() << std::endl;
		hDC = NULL;                           // Set DC To NULL
	}
	hDC = nullptr;                           // Set DC To NULL
}

void DisplaySurface::InitSwapChain()
{
	RECT rect;

#if defined(WINVER) 
	GetWindowRect((HWND)mHwnd, &rect);
#endif

	int screenWidth = abs(rect.right - rect.left);
	int screenHeight = abs(rect.bottom - rect.top);

	viewport.h = screenHeight;
	viewport.w = screenWidth;
	viewport.x = 0;
	viewport.y = 0;

	// Initialize the swap chain description.

	std::vector<vk::SurfaceFormatKHR> surfFormats = simul::vulkan::deviceManager->GetSurfaceFormats(surface);

	// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	// the surface has no preferred format.  Otherwise, at least one
	// supported format will be returned.
	size_t formatCount = surfFormats.size();
	if (formatCount == 1 && (surfFormats)[0].format == vk::Format::eUndefined)
	{
		vulkanFormat = vk::Format::eB8G8R8A8Unorm;
	}
	else
	{
		assert(formatCount >= 1);
		vulkanFormat = surfFormats[0].format;
	}
	colour_space = surfFormats[0].colorSpace;
	//if(!swapchain)
//		swapchain.swap(new vk::SwapchainKHR);
	vk::SwapchainKHR oldSwapchain = *swapchain.get();

	// Check the surface capabilities and formats
	vk::SurfaceCapabilitiesKHR surfCapabilities;
	vk::PhysicalDevice *gpu=simul::vulkan::deviceManager->GetGPU();
	auto result = gpu->getSurfaceCapabilitiesKHR(*surface, &surfCapabilities);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	uint32_t presentModeCount;
	result = gpu->getSurfacePresentModesKHR(*surface, &presentModeCount, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::unique_ptr<vk::PresentModeKHR[]> presentModes(new vk::PresentModeKHR[presentModeCount]);
	result = gpu->getSurfacePresentModesKHR(*surface, &presentModeCount, presentModes.get());
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	vk::Extent2D swapchainExtent;
	// width and height are either both -1, or both not -1.
	if (surfCapabilities.currentExtent.width == (uint32_t)-1)
	{
// If the surface size is undefined, the size is set to
// the size of the images requested.
		swapchainExtent.width = viewport.w;
		swapchainExtent.height = viewport.h;
	}
	else
	{
	 // If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCapabilities.currentExtent;
		viewport.w = surfCapabilities.currentExtent.width;
		viewport.h = surfCapabilities.currentExtent.height;
	}

	// The FIFO present mode is guaranteed by the spec to be supported
	// and to have no tearing.  It's a great default present mode to use.
	vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

	// Determine the number of VkImages to use in the swap chain.
	// Application desires to acquire 3 images at a time for triple
	// buffering
	uint32_t desiredNumOfSwapchainImages = 3;
	if (desiredNumOfSwapchainImages < surfCapabilities.minImageCount)
	{
		desiredNumOfSwapchainImages = surfCapabilities.minImageCount;
	}

	// If maxImageCount is 0, we can ask for as many images as we want,
	// otherwise
	// we're limited to maxImageCount
	if ((surfCapabilities.maxImageCount > 0) && (desiredNumOfSwapchainImages > surfCapabilities.maxImageCount))
	{
// Application must settle for fewer images than desired:
		desiredNumOfSwapchainImages = surfCapabilities.maxImageCount;
	}

	vk::SurfaceTransformFlagBitsKHR preTransform;
	if (surfCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
	{
		preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else
	{
		preTransform = surfCapabilities.currentTransform;
	}

	// Find a supported composite alpha mode - one of these is guaranteed to be set
	vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	vk::CompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit,
	};
	for (uint32_t i = 0; i < _countof(compositeAlphaFlags); i++)
	{
		if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i])
		{
			compositeAlpha = compositeAlphaFlags[i];
			break;
		}
	}

	auto const swapchain_ci = vk::SwapchainCreateInfoKHR()
		.setSurface(*surface)
		.setMinImageCount(desiredNumOfSwapchainImages)
		.setImageFormat(vulkanFormat)
		.setImageColorSpace(colour_space)
		.setImageExtent({ swapchainExtent.width, swapchainExtent.height })
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setPreTransform(preTransform)
		.setCompositeAlpha(compositeAlpha)
		.setPresentMode(swapchainPresentMode)
		.setClipped(true)
		.setOldSwapchain(oldSwapchain);
	
	auto *vulkanDevice = renderPlatform->AsVulkanDevice();
	result = vulkanDevice->createSwapchainKHR(&swapchain_ci, nullptr, swapchain.get());
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	// If we just re-created an existing swapchain, we should destroy the
	// old
	// swapchain at this point.
	// Note: destroying the swapchain also cleans up all its associated
	// presentable images once the platform is done with them.
	if (oldSwapchain)
	{
		vulkanDevice->destroySwapchainKHR(oldSwapchain, nullptr);
	}
	
	std::vector<vk::Image> swapchainImages=deviceManager->GetSwapchainImages(swapchain.get());
	swapchain_image_resources.resize(swapchainImages.size());

	for (uint32_t i = 0; i < swapchainImages.size(); ++i)
	{
		auto color_image_view = vk::ImageViewCreateInfo()
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vulkanFormat)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

		swapchain_image_resources[i].image = swapchainImages[i];

		color_image_view.image = swapchain_image_resources[i].image;

		result = vulkanDevice->createImageView(&color_image_view, nullptr, &swapchain_image_resources[i].view);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
}

void DisplaySurface::Render()
{
	Resize();

	auto *vulkanDevice = renderPlatform->AsVulkanDevice();

	crossplatform::DeviceContext &immediateContext = renderPlatform->GetImmediateContext();
	deferredContext.platform_context = immediateContext.platform_context;
	deferredContext.renderPlatform = renderPlatform;


	renderPlatform->StoreRenderState(deferredContext);

	static vec4 clear = { 0.0f,0.0f,1.0f,1.0f };
	//glViewport(0, 0, viewport.w, viewport.h);   
	//glClearColor(clear.x,clear.y,clear.z,clear.w);
	//glClearDepth(1.0f);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (renderer)
		renderer->Render(mViewId, 0, 0, viewport.w, viewport.h);

	renderPlatform->RestoreRenderState(deferredContext);
	SwapBuffers(hDC);
//	wglMakeCurrent(hDC,hglrc);
}

void DisplaySurface::EndFrame()
{
	// We check for resize here, because we must manage the SwapChain from the main thread.
	// we may have to do it after executing the command list, because Resize destroys the CL, and we don't want to lose commands.
	Resize();
}

void DisplaySurface::Resize()
{
	RECT rect;
	if (!GetWindowRect((HWND)mHwnd, &rect))
		return;
	UINT W = abs(rect.right - rect.left);
	UINT H = abs(rect.bottom - rect.top);
	if (viewport.w == W&&viewport.h == H)
		return;
	InitSwapChain();

	viewport.w = W;
	viewport.h = H;
	viewport.x = 0;
	viewport.y = 0;

	renderer->ResizeView(mViewId, W, H);
}