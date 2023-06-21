#include "DisplaySurface.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <linux/input.h>
#endif
#include <vulkan/vulkan.hpp>
// Careless implementation by Vulkan requires this:
#undef NOMINMAX
//#include <vulkan/vk_sdk_platform.h>
#define NOMINMAX

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

using namespace platform;
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

DisplaySurface::DisplaySurface(int view_id)
	:crossplatform::DisplaySurface( view_id)
	,pixelFormat(crossplatform::UNKNOWN)
#ifdef _MSC_VER
	,hDC(nullptr)
	,hRC(nullptr)
	#endif
	,frame_index(0)
	,current_buffer(0)
{
}

DisplaySurface::~DisplaySurface()
{
	InvalidateDeviceObjects();
}

vk::Instance * DisplaySurface::GetVulkanInstance()
{
	auto rv = (vulkan::RenderPlatform *)renderPlatform;
	vk::Instance *inst = nullptr;
	if (rv)
	{
		inst = rv->AsVulkanInstance();
	}
	return inst;
}

vk::Device *DisplaySurface::GetVulkanDevice()
{
	auto rv = (vulkan::RenderPlatform *)renderPlatform;
	vk::Device *dev = nullptr;
	if (rv)
	{
		dev = rv->AsVulkanDevice();
	}
	return dev;
}

vk::PhysicalDevice* DisplaySurface::GetGPU()
{
	auto rv = (vulkan::RenderPlatform *)renderPlatform;
	vk::PhysicalDevice *gpu = nullptr;
	if (rv)
	{
		gpu = rv->GetVulkanGPU();
	}
	return gpu;
}


void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* r, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
	if (mHwnd && mHwnd == handle)
	{
		return;
	}
	renderPlatform = r;
    pixelFormat = outFmt;
    requestedPixelFormat = outFmt;
	crossplatform::DeviceContext &immediateContext = r->GetImmediateContext();
	mHwnd = handle;
	
	
#ifdef VK_USE_PLATFORM_WIN32_KHR
	hDC = GetDC(mHwnd);
	if (!(hDC))
	{
		return;
	}
	auto hInstance=GetModuleHandle(nullptr);
	vk::Win32SurfaceCreateInfoKHR createInfo = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(hInstance)
		.setHwnd(mHwnd);
	auto rv = (vulkan::RenderPlatform *)renderPlatform;
	vk::Instance *inst = GetVulkanInstance();
	if (inst)
	{
		auto result = inst->createWin32SurfaceKHR(&createInfo, nullptr, &mSurface);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
#endif 
#ifdef VK_USE_PLATFORM_XCB_KHR
#if 0

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        sci.connection = connection;
        sci.window = window->x11.handle;

        err = vkCreateXcbSurfaceKHR(instance, &sci, allocator, surface);

	vk::XcbSurfaceCreateInfoKHR createInfo = vk::XcbSurfaceCreateInfoKHR()
		.setWindow((xcb_window_t)mHwnd)
		.setConnection();
	vk::Instance* inst = GetVulkanInstance();
	if (inst)
	{
		auto result = inst->createXcbSurfaceKHR(&createInfo, nullptr, &mSurface);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
#else
	mSurface=*((VkSurfaceKHR*)handle);
#endif
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vk::AndroidSurfaceCreateInfoKHR createInfo = vk::AndroidSurfaceCreateInfoKHR()
		.setWindow((ANativeWindow*)mHwnd);
	vk::Instance* inst = GetVulkanInstance();
	if (inst)
	{
		auto result = inst->createAndroidSurfaceKHR(&createInfo, nullptr, &mSurface);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

#endif
}

void DisplaySurface::InvalidateDeviceObjects()
{
#ifdef _MSC_VER
	if(!hDC)
		return;
#endif
	vk::Device *vulkanDevice=GetVulkanDevice();
	if(vulkanDevice)
	{
		vulkanDevice->waitIdle();
		for(auto i:swapchain_image_resources)
		{	
			vulkanDevice->destroyFramebuffer(i.framebuffer,nullptr);
			vulkanDevice->freeCommandBuffers(cmd_pool, 1,&i.cmd);
			if(present_cmd_pool)
				vulkanDevice->freeCommandBuffers(present_cmd_pool, 1, &i.graphics_to_present_cmd);
			vulkanDevice->destroyImageView(i.view,nullptr);
			//vulkanDevice->destroyImage(i.image); part of swapchain?
		}
		swapchain_image_resources.clear();
		vulkanDevice->destroyCommandPool(cmd_pool,nullptr);
		vulkanDevice->destroyCommandPool(present_cmd_pool,nullptr);
		vulkanDevice->destroySwapchainKHR(swapchain,nullptr);
		for(int i=0;i<SIMUL_VULKAN_FRAME_LAG+1;i++)
		{
			vulkanDevice->destroySemaphore(image_acquired_semaphores	[i],nullptr);
			vulkanDevice->destroySemaphore(draw_complete_semaphores		[i],nullptr);
			vulkanDevice->destroySemaphore(image_ownership_semaphores	[i],nullptr);
			vulkanDevice->destroyFence(fences[i],nullptr);
		}
		vulkanDevice->destroyRenderPass(render_pass,nullptr);

		vulkanDevice->destroyPipeline(default_pipeline,nullptr);
		vulkanDevice->destroyPipelineCache(pipelineCache,nullptr);
		vulkanDevice->destroyPipelineLayout(pipeline_layout,nullptr);
		vulkanDevice->destroyDescriptorSetLayout(desc_layout,nullptr);
	}
			//vulkanDevice->destroysurface(mSurface,nullptr);

#ifdef _MSC_VER
	if (hDC && !ReleaseDC(mHwnd, hDC))                    // Are We Able To Release The DC
	{
 		SIMUL_CERR << "Release Device Context Failed." << GetErr() << std::endl;
		hDC = NULL;                           // Set DC To NULL
	}
	hDC = nullptr;                           // Set DC To NULL
#endif
}


void DisplaySurface::GetQueues()
{
	auto vkGpu=GetGPU();
	std::vector<vk::QueueFamilyProperties> queue_props;
	
	{
		InitQueueProperties(*vulkanRenderPlatform->GetVulkanGPU(), queue_props);
	}
	uint32_t queue_family_count=(uint32_t)queue_props.size();
	// Iterate over each queue to learn whether it supports presenting:
	std::vector<vk::Bool32> supportsPresent(queue_family_count);
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		vk::Result result=vkGpu->getSurfaceSupportKHR(i, mSurface, &supportsPresent[i]);
		if(result != vk::Result::eSuccess)
		{
			SIMUL_CERR<<"Vulkan operation failed\n";
		}
	}

	uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
	uint32_t presentQueueFamilyIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		if (queue_props[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			if (graphicsQueueFamilyIndex == UINT32_MAX)
			{
				graphicsQueueFamilyIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueFamilyIndex = i;
				presentQueueFamilyIndex = i;
				break;
			}
		}
	}

	if (presentQueueFamilyIndex == UINT32_MAX)
	{
// If didn't find a queue that supports both graphics and present,
// then
// find a separate present queue.
		for (uint32_t i = 0; i < queue_family_count; ++i)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				presentQueueFamilyIndex = i;
				break;
			}
		}
	}

	// Generate error if could not find both a graphics and a present queue
	if (graphicsQueueFamilyIndex == UINT32_MAX || presentQueueFamilyIndex == UINT32_MAX)
	{
		SIMUL_BREAK("Could not find both graphics and present queues\nSwapchain Initialization Failure");
	}

	graphics_queue_family_index = graphicsQueueFamilyIndex;
	present_queue_family_index = presentQueueFamilyIndex;

    bool separate_present_queue = (graphics_queue_family_index != present_queue_family_index);
	
	vk::Device *vulkanDevice=GetVulkanDevice();
	vulkanDevice->getQueue(graphics_queue_family_index, 0, &graphics_queue);
    if (!separate_present_queue) {
        present_queue = graphics_queue;
    } else {
        vulkanDevice->getQueue(present_queue_family_index, 0, &present_queue);
    }

	
    vulkanDevice->getQueue(graphics_queue_family_index, 0, &graphics_queue);
	if (!separate_present_queue)
	{
		present_queue = graphics_queue;
	}
	else
	{
		vulkanDevice->getQueue(present_queue_family_index, 0, &present_queue);
	}

	//vkGpu->getMemoryProperties(&memory_properties);
}



void DisplaySurface::InitSwapChain()
{
#if defined(WINVER) 
	RECT rect;

	GetClientRect((HWND)mHwnd, &rect);

	int screenWidth = abs(rect.right - rect.left);
	int screenHeight = abs(rect.bottom - rect.top);

	viewport.h = screenHeight;
	viewport.w = screenWidth;
#endif
	viewport.x = 0;
	viewport.y = 0;

	// what formats are supported?
    uint32_t surfaceformats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(*vulkanRenderPlatform->GetVulkanGPU(), mSurface, &surfaceformats, nullptr);
    std::vector<VkSurfaceFormatKHR> vkSurfaceFormats;
    vkSurfaceFormats.resize(surfaceformats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(*vulkanRenderPlatform->GetVulkanGPU(), mSurface, &surfaceformats, vkSurfaceFormats.data());
    vulkanFormat = vk::Format::eUndefined;
    colour_space = vk::ColorSpaceKHR::eSrgbNonlinear;
    pixelFormat = requestedPixelFormat;
	// Initialize the swap chain description. Let's try to find a format/colourspace combo that matches our specified pixelFormat.
    {
        for (int i = 0; i < vkSurfaceFormats.size(); i++)
        {
            vk::Format vf=(vk::Format)vkSurfaceFormats[i].format;
            if (RenderPlatform::ToVulkanFormat(pixelFormat) == vf)
			{
				vulkanFormat = vf;
				colour_space = (vk::ColorSpaceKHR)vkSurfaceFormats[i].colorSpace;
				break;
			}
        }
		// failed to find a match? Then change the pixelFormat to match the swapchain format.
        if (vulkanFormat == vk::Format::eUndefined)
        {
            for (int i = 0; i < vkSurfaceFormats.size(); i++)
            {
                vk::Format vf = (vk::Format)vkSurfaceFormats[i].format;
                pixelFormat = RenderPlatform::FromVulkanFormat(vf);
                if (pixelFormat != crossplatform::PixelFormat::UNKNOWN)
                {
                    vulkanFormat = vf;
                    colour_space = (vk::ColorSpaceKHR)vkSurfaceFormats[i].colorSpace;
                    break;
                }
            }
        }
        switch (colour_space)
        {
        case vk::ColorSpaceKHR::eSrgbNonlinear:
        case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT:
        case vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT:
        case vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear:
            swapChainIsGammaCorrected = true;
            break;
            default:
            swapChainIsGammaCorrected = false;
            break;
        };
    }

	//if(!swapchain)
//		swapchain.swap(new vk::SwapchainKHR);
	vk::SwapchainKHR oldSwapchain = swapchain;
	if(!mSurface)
		return;
	// Check the mSurface capabilities and formats
	vk::SurfaceCapabilitiesKHR surfCapabilities;
	vk::PhysicalDevice *gpu=GetGPU();
	auto result = gpu->getSurfaceCapabilitiesKHR(mSurface, &surfCapabilities);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	uint32_t presentModeCount;
	auto result2 = gpu->getSurfacePresentModesKHR(mSurface, &presentModeCount, (vk::PresentModeKHR*)nullptr);
	SIMUL_ASSERT(result2 == vk::Result::eSuccess);

	std::unique_ptr<vk::PresentModeKHR[]> presentModes(new vk::PresentModeKHR[presentModeCount]);
	result = gpu->getSurfacePresentModesKHR(mSurface, &presentModeCount, presentModes.get());
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	vk::Extent2D swapchainExtent;
	// width and height are either both -1, or both not -1.
	if (surfCapabilities.currentExtent.width == (uint32_t)-1)
	{
// If the mSurface size is undefined, the size is set to
// the size of the images requested.
		swapchainExtent.width = viewport.w;
		swapchainExtent.height = viewport.h;
	}
	else
	{
	 // If the mSurface size is defined, the swap chain size must match
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
//	gpu->GetPhysicalDeviceSurfaceSupportKHR(mSurface);
	auto const swapchain_ci = vk::SwapchainCreateInfoKHR()
		.setSurface(mSurface)
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
	int supported=0;
	vkGetPhysicalDeviceSurfaceSupportKHR(*vulkanRenderPlatform->GetVulkanGPU(),0,mSurface, (vk::Bool32*) & supported);
	SIMUL_ASSERT(supported!=0 );
	
	// MUST do GetQueues before creating the swapchain, because getSurfaceSupportKHR is treated as a PREREQUISITE
	// to create the device, even though it's defined as a TEST. This is bad API design.
	GetQueues();
	result = gpu->getSurfaceSupportKHR( 0,  mSurface,(vk::Bool32*)&supported) ;
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SIMUL_ASSERT(supported!=0);
	auto *vulkanDevice = renderPlatform->AsVulkanDevice();
	result = vulkanDevice->createSwapchainKHR(&swapchain_ci, nullptr, &swapchain);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,swapchain,"Swapchain");

	// If we just re-created an existing swapchain, we should destroy the
	// old
	// swapchain at this point.
	// Note: destroying the swapchain also cleans up all its associated
	// presentable images once the platform is done with them.
	if (oldSwapchain)
	{
		vulkanDevice->destroySwapchainKHR(oldSwapchain, nullptr);
	}
	
	std::vector<vk::Image> swapchainImages;

	{
		uint32_t swapchainImageCount;
		auto result = vulkanRenderPlatform->AsVulkanDevice()->getSwapchainImagesKHR(swapchain, (uint32_t*)&swapchainImageCount, (vk::Image*)nullptr);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		swapchainImages.resize(swapchainImageCount);
		result = vulkanRenderPlatform->AsVulkanDevice()->getSwapchainImagesKHR(swapchain, (uint32_t*)&swapchainImageCount, swapchainImages.data());
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		for(int i=0;i<swapchainImages.size();i++)
		{
			SetVulkanName( renderPlatform, swapchainImages[i], platform::core::QuickFormat("Swapchain %d",i));
		}
	}
	swapchain_image_resources.resize(swapchainImages.size());

	if(swapchain_image_resources.size()>SIMUL_VULKAN_FRAME_LAG+1)
	{
		SIMUL_BREAK("swapchain_image_resources.size() is too large");
	}

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
	
	bool separate_present_queue=(graphics_queue_family_index!=present_queue_family_index);

	// Create semaphores to synchronize acquiring presentable buffers before
	// rendering and waiting for drawing to be complete before presenting
	auto const semaphoreCreateInfo = vk::SemaphoreCreateInfo();
	// Create fences that we can use to throttle if we get too far
	// ahead of the image presents
	auto const fence_ci = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);
	for (uint32_t i = 0; i < swapchain_image_resources.size(); i++)
	{
		auto result = vulkanDevice->createFence(&fence_ci, nullptr, &fences[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = vulkanDevice->createSemaphore(&semaphoreCreateInfo, nullptr, &image_acquired_semaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = vulkanDevice->createSemaphore(&semaphoreCreateInfo, nullptr, &draw_complete_semaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		if (separate_present_queue)
		{
			result = vulkanDevice->createSemaphore(&semaphoreCreateInfo, nullptr, &image_ownership_semaphores[i]);
			SIMUL_ASSERT(result == vk::Result::eSuccess);
		}
	}
	
	// Init command buffers / pools:
	{
		auto const cmd_pool_info = vk::CommandPoolCreateInfo().setQueueFamilyIndex(graphics_queue_family_index).setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		auto result = vulkanDevice->createCommandPool(&cmd_pool_info, nullptr, &cmd_pool);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		auto const cmdAllocInfo = vk::CommandBufferAllocateInfo()
											.setCommandPool(cmd_pool)
											.setLevel(vk::CommandBufferLevel::ePrimary)
											.setCommandBufferCount(1);

		result = vulkanDevice->allocateCommandBuffers(&cmdAllocInfo,&cmd);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		auto const cmd_buf_info = vk::CommandBufferBeginInfo().setPInheritanceInfo(nullptr);

		result = cmd.begin(&cmd_buf_info);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	

		for (uint32_t i = 0; i < swapchainImages.size(); ++i)
		{
			result = vulkanDevice->allocateCommandBuffers(&cmdAllocInfo, &swapchain_image_resources[i].cmd);
			SIMUL_ASSERT(result == vk::Result::eSuccess);
		}
		if (present_queue_family_index!=graphics_queue_family_index)
		{
			auto const present_cmd_pool_info = vk::CommandPoolCreateInfo().setQueueFamilyIndex(present_queue_family_index).setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

			result = vulkanDevice->createCommandPool(&present_cmd_pool_info, nullptr, &present_cmd_pool);
			SIMUL_ASSERT(result == vk::Result::eSuccess);

			auto const present_cmd = vk::CommandBufferAllocateInfo()
				.setCommandPool(present_cmd_pool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);

			for (uint32_t i = 0; i < swapchainImages.size(); i++)
			{
				result = vulkanDevice->allocateCommandBuffers(&present_cmd, &swapchain_image_resources[i].graphics_to_present_cmd);
				SIMUL_ASSERT(result == vk::Result::eSuccess);

				{
					auto const cmd_buf_info = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
					auto result = swapchain_image_resources[i].graphics_to_present_cmd.begin(&cmd_buf_info);
					SIMUL_ASSERT(result == vk::Result::eSuccess);

					auto const image_ownership_barrier =
						vk::ImageMemoryBarrier()
						.setSrcAccessMask(vk::AccessFlags())
						.setDstAccessMask(vk::AccessFlags())
						.setOldLayout(vk::ImageLayout::ePresentSrcKHR)
						.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
						.setSrcQueueFamilyIndex(graphics_queue_family_index)
						.setDstQueueFamilyIndex(present_queue_family_index)
						.setImage(swapchain_image_resources[i].image)
						.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

					swapchain_image_resources[i].graphics_to_present_cmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits(), 0, nullptr, 0,
						nullptr, 1, &image_ownership_barrier);
					//result =
					 swapchain_image_resources[i].graphics_to_present_cmd.end();
					SIMUL_ASSERT(result == vk::Result::eSuccess);
				}
			}
		}
	}
	CreateRenderPass();
	CreateDefaultPipeline();
	CreateFramebuffers();
}

void DisplaySurface::CreateRenderPass() 
{
	// The initial layout for the color and depth attachments will be LAYOUT_UNDEFINED
	// because at the start of the renderpass, we don't care about their contents.
	// At the start of the subpass, the color attachment's layout will be transitioned
	// to LAYOUT_COLOR_ATTACHMENT_OPTIMAL and the depth stencil attachment's layout
	// will be transitioned to LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of
	// the renderpass, the color attachment's layout will be transitioned to
	// LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part of
	// the renderpass, no barriers are necessary.

	vulkanRenderPlatform->CreateVulkanRenderpass(deferredContext, render_pass, 1, &pixelFormat);
}

void DisplaySurface::CreateFramebuffers()
{
	vk::ImageView attachments[1];

	auto const fb_info = vk::FramebufferCreateInfo()
		.setRenderPass(render_pass)
		.setAttachmentCount(1)
		.setPAttachments(attachments)
		.setWidth((uint32_t)viewport.w)
		.setHeight((uint32_t)viewport.h)
		.setLayers(1);

	for (uint32_t i = 0; i < swapchain_image_resources.size(); i++)
	{
		attachments[0] = swapchain_image_resources[i].view;
		auto const result = GetVulkanDevice()->createFramebuffer(&fb_info, nullptr, &swapchain_image_resources[i].framebuffer);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
	if (renderer)
	{
		if (mViewId < 0)
			mViewId = renderer->AddView();
		renderer->ResizeView(mViewId, viewport.w, viewport.h);
	}
}

void DisplaySurface::CreateDefaultLayout()
{
	vk::DescriptorSetLayoutBinding const layout_bindings[1] = { vk::DescriptorSetLayoutBinding()
																   .setBinding(0)
																   .setDescriptorType(vk::DescriptorType::eUniformBuffer)
																   .setDescriptorCount(1)
																   .setStageFlags(vk::ShaderStageFlagBits::eVertex)
																   .setPImmutableSamplers(nullptr) };

	auto const descriptor_layout = vk::DescriptorSetLayoutCreateInfo().setBindingCount(1).setPBindings(layout_bindings);

	auto result = GetVulkanDevice()->createDescriptorSetLayout(&descriptor_layout, nullptr, &desc_layout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	auto const pPipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&desc_layout);

	result = GetVulkanDevice()->createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &pipeline_layout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
}


vk::ShaderModule DisplaySurface::prepare_shader_module(const uint32_t *code, size_t size)
{
	const auto moduleCreateInfo = vk::ShaderModuleCreateInfo().setCodeSize(size).setPCode(code);

	vk::ShaderModule module;
	auto result = GetVulkanDevice()->createShaderModule(&moduleCreateInfo, nullptr, &module);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	return module;
}

vk::ShaderModule DisplaySurface::prepare_vs()
{
	const uint32_t vertShaderCode[] = {0x07230203,0x00010000,0x00080007,0x0000002f,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0009000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x00000015,0x0000001e,
	0x0000002b,0x00030003,0x00000002,0x00000190,0x00090004,0x415f4c47,0x735f4252,0x72617065,
	0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00090004,0x415f4c47,0x735f4252,
	0x69646168,0x6c5f676e,0x75676e61,0x5f656761,0x70303234,0x006b6361,0x00040005,0x00000004,
	0x6e69616d,0x00000000,0x00050005,0x00000009,0x63786574,0x64726f6f,0x00000000,0x00030005,
	0x0000000f,0x00667562,0x00040006,0x0000000f,0x00000000,0x0050564d,0x00060006,0x0000000f,
	0x00000001,0x69736f70,0x6e6f6974,0x00000000,0x00050006,0x0000000f,0x00000002,0x72747461,
	0x00000000,0x00040005,0x00000011,0x66756275,0x00000000,0x00060005,0x00000015,0x565f6c67,
	0x65747265,0x646e4978,0x00007865,0x00060005,0x0000001c,0x505f6c67,0x65567265,0x78657472,
	0x00000000,0x00060006,0x0000001c,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,
	0x0000001c,0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x0000001c,
	0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00030005,0x0000001e,0x00000000,
	0x00050005,0x0000002b,0x67617266,0x736f705f,0x00000000,0x00040047,0x00000009,0x0000001e,
	0x00000000,0x00040047,0x0000000d,0x00000006,0x00000010,0x00040047,0x0000000e,0x00000006,
	0x00000010,0x00040048,0x0000000f,0x00000000,0x00000005,0x00050048,0x0000000f,0x00000000,
	0x00000023,0x00000000,0x00050048,0x0000000f,0x00000000,0x00000007,0x00000010,0x00050048,
	0x0000000f,0x00000001,0x00000023,0x00000040,0x00050048,0x0000000f,0x00000002,0x00000023,
	0x00000280,0x00030047,0x0000000f,0x00000002,0x00040047,0x00000011,0x00000022,0x00000000,
	0x00040047,0x00000011,0x00000021,0x00000000,0x00040047,0x00000015,0x0000000b,0x0000002a,
	0x00050048,0x0000001c,0x00000000,0x0000000b,0x00000000,0x00050048,0x0000001c,0x00000001,
	0x0000000b,0x00000001,0x00050048,0x0000001c,0x00000002,0x0000000b,0x00000003,0x00030047,
	0x0000001c,0x00000002,0x00040047,0x0000002b,0x0000001e,0x00000001,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
	0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,
	0x00000009,0x00000003,0x00040018,0x0000000a,0x00000007,0x00000004,0x00040015,0x0000000b,
	0x00000020,0x00000000,0x0004002b,0x0000000b,0x0000000c,0x00000024,0x0004001c,0x0000000d,
	0x00000007,0x0000000c,0x0004001c,0x0000000e,0x00000007,0x0000000c,0x0005001e,0x0000000f,
	0x0000000a,0x0000000d,0x0000000e,0x00040020,0x00000010,0x00000002,0x0000000f,0x0004003b,
	0x00000010,0x00000011,0x00000002,0x00040015,0x00000012,0x00000020,0x00000001,0x0004002b,
	0x00000012,0x00000013,0x00000002,0x00040020,0x00000014,0x00000001,0x00000012,0x0004003b,
	0x00000014,0x00000015,0x00000001,0x00040020,0x00000017,0x00000002,0x00000007,0x0004002b,
	0x0000000b,0x0000001a,0x00000001,0x0004001c,0x0000001b,0x00000006,0x0000001a,0x0005001e,
	0x0000001c,0x00000007,0x00000006,0x0000001b,0x00040020,0x0000001d,0x00000003,0x0000001c,
	0x0004003b,0x0000001d,0x0000001e,0x00000003,0x0004002b,0x00000012,0x0000001f,0x00000000,
	0x00040020,0x00000020,0x00000002,0x0000000a,0x0004002b,0x00000012,0x00000023,0x00000001,
	0x00040017,0x00000029,0x00000006,0x00000003,0x00040020,0x0000002a,0x00000003,0x00000029,
	0x0004003b,0x0000002a,0x0000002b,0x00000003,0x00050036,0x00000002,0x00000004,0x00000000,
	0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000012,0x00000016,0x00000015,0x00060041,
	0x00000017,0x00000018,0x00000011,0x00000013,0x00000016,0x0004003d,0x00000007,0x00000019,
	0x00000018,0x0003003e,0x00000009,0x00000019,0x00050041,0x00000020,0x00000021,0x00000011,
	0x0000001f,0x0004003d,0x0000000a,0x00000022,0x00000021,0x0004003d,0x00000012,0x00000024,
	0x00000015,0x00060041,0x00000017,0x00000025,0x00000011,0x00000023,0x00000024,0x0004003d,
	0x00000007,0x00000026,0x00000025,0x00050091,0x00000007,0x00000027,0x00000022,0x00000026,
	0x00050041,0x00000008,0x00000028,0x0000001e,0x0000001f,0x0003003e,0x00000028,0x00000027,
	0x00050041,0x00000008,0x0000002c,0x0000001e,0x0000001f,0x0004003d,0x00000007,0x0000002d,
	0x0000002c,0x0008004f,0x00000029,0x0000002e,0x0000002d,0x0000002d,0x00000000,0x00000001,
	0x00000002,0x0003003e,0x0000002b,0x0000002e,0x000100fd,0x00010038
	};

	vk::ShaderModule vert_shader_module = prepare_shader_module(vertShaderCode, sizeof(vertShaderCode));

	return vert_shader_module;
}
vk::ShaderModule DisplaySurface::prepare_fs()
{
	const uint32_t fragShaderCode[] = {
	// 7.9.2888
	0x07230203,0x00010000,0x00080007,0x00000030,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x00000022,0x0000002a,
	0x00030010,0x00000004,0x00000007,0x00030003,0x00000002,0x00000190,0x00090004,0x415f4c47,
	0x735f4252,0x72617065,0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00090004,
	0x415f4c47,0x735f4252,0x69646168,0x6c5f676e,0x75676e61,0x5f656761,0x70303234,0x006b6361,
	0x00040005,0x00000004,0x6e69616d,0x00000000,0x00030005,0x00000009,0x00005864,0x00050005,
	0x0000000b,0x67617266,0x736f705f,0x00000000,0x00030005,0x0000000e,0x00005964,0x00040005,
	0x00000011,0x6d726f6e,0x00006c61,0x00040005,0x00000017,0x6867696c,0x00000074,0x00050005,
	0x00000022,0x61724675,0x6c6f4367,0x0000726f,0x00030005,0x00000027,0x00786574,0x00050005,
	0x0000002a,0x63786574,0x64726f6f,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000001,
	0x00040047,0x00000022,0x0000001e,0x00000000,0x00040047,0x00000027,0x00000022,0x00000000,
	0x00040047,0x00000027,0x00000021,0x00000001,0x00040047,0x0000002a,0x0000001e,0x00000000,
	0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040017,0x00000007,0x00000006,0x00000003,0x00040020,0x00000008,0x00000007,0x00000007,
	0x00040020,0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,
	0x00040020,0x00000016,0x00000007,0x00000006,0x0004002b,0x00000006,0x00000018,0x00000000,
	0x0004002b,0x00000006,0x00000019,0x3ed91687,0x0004002b,0x00000006,0x0000001a,0x3f10e560,
	0x0004002b,0x00000006,0x0000001b,0x3f34fdf4,0x0006002c,0x00000007,0x0000001c,0x00000019,
	0x0000001a,0x0000001b,0x00040017,0x00000020,0x00000006,0x00000004,0x00040020,0x00000021,
	0x00000003,0x00000020,0x0004003b,0x00000021,0x00000022,0x00000003,0x00090019,0x00000024,
	0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,
	0x00000025,0x00000024,0x00040020,0x00000026,0x00000000,0x00000025,0x0004003b,0x00000026,
	0x00000027,0x00000000,0x00040020,0x00000029,0x00000001,0x00000020,0x0004003b,0x00000029,
	0x0000002a,0x00000001,0x00040017,0x0000002b,0x00000006,0x00000002,0x00050036,0x00000002,
	0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000008,0x00000009,
	0x00000007,0x0004003b,0x00000008,0x0000000e,0x00000007,0x0004003b,0x00000008,0x00000011,
	0x00000007,0x0004003b,0x00000016,0x00000017,0x00000007,0x0004003d,0x00000007,0x0000000c,
	0x0000000b,0x000400cf,0x00000007,0x0000000d,0x0000000c,0x0003003e,0x00000009,0x0000000d,
	0x0004003d,0x00000007,0x0000000f,0x0000000b,0x000400d0,0x00000007,0x00000010,0x0000000f,
	0x0003003e,0x0000000e,0x00000010,0x0004003d,0x00000007,0x00000012,0x00000009,0x0004003d,
	0x00000007,0x00000013,0x0000000e,0x0007000c,0x00000007,0x00000014,0x00000001,0x00000044,
	0x00000012,0x00000013,0x0006000c,0x00000007,0x00000015,0x00000001,0x00000045,0x00000014,
	0x0003003e,0x00000011,0x00000015,0x0004003d,0x00000007,0x0000001d,0x00000011,0x00050094,
	0x00000006,0x0000001e,0x0000001c,0x0000001d,0x0007000c,0x00000006,0x0000001f,0x00000001,
	0x00000028,0x00000018,0x0000001e,0x0003003e,0x00000017,0x0000001f,0x0004003d,0x00000006,
	0x00000023,0x00000017,0x0004003d,0x00000025,0x00000028,0x00000027,0x0004003d,0x00000020,
	0x0000002c,0x0000002a,0x0007004f,0x0000002b,0x0000002d,0x0000002c,0x0000002c,0x00000000,
	0x00000001,0x00050057,0x00000020,0x0000002e,0x00000028,0x0000002d,0x0005008e,0x00000020,
	0x0000002f,0x0000002e,0x00000023,0x0003003e,0x00000022,0x0000002f,0x000100fd,0x00010038

	};

	vk::ShaderModule frag_shader_module = prepare_shader_module(fragShaderCode, sizeof(fragShaderCode));

	return frag_shader_module;
}

void DisplaySurface::CreateDefaultPipeline()
{
	CreateDefaultLayout();
	vk::PipelineCacheCreateInfo const pipelineCacheInfo;
	auto result = GetVulkanDevice()->createPipelineCache(&pipelineCacheInfo, nullptr, &pipelineCache);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	vk::PipelineShaderStageCreateInfo const shaderStageInfo[2] = {
		vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(prepare_vs()).setPName("main"),
		vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(prepare_fs()).setPName("main") };

	vk::PipelineVertexInputStateCreateInfo const vertexInputInfo;

	auto const inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);

	// TODO: Where are pViewports and pScissors set?
	auto const viewportInfo = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

	auto const rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
		.setDepthClampEnable(VK_FALSE)
		.setRasterizerDiscardEnable(VK_FALSE)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(VK_FALSE)
		.setLineWidth(1.0f);

	auto const multisampleInfo = vk::PipelineMultisampleStateCreateInfo();
	
	auto const stencilOp =
		vk::StencilOpState().setFailOp(vk::StencilOp::eKeep).setPassOp(vk::StencilOp::eKeep).setCompareOp(vk::CompareOp::eAlways);

	auto const depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthTestEnable(VK_TRUE)
		.setDepthWriteEnable(VK_TRUE)
		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
		.setDepthBoundsTestEnable(VK_FALSE)
		.setStencilTestEnable(VK_FALSE)
		.setFront(stencilOp)
		.setBack(stencilOp);

	vk::PipelineColorBlendAttachmentState const colorBlendAttachments[1] = {
		vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
																  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };

	auto const colorBlendInfo =
		vk::PipelineColorBlendStateCreateInfo().setAttachmentCount(1).setPAttachments(colorBlendAttachments);

	vk::DynamicState const dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	auto const dynamicStateInfo = vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);
	
	auto const pipelineInfo = vk::GraphicsPipelineCreateInfo()
		.setStageCount(2)
		.setPStages(shaderStageInfo)
		.setPVertexInputState(&vertexInputInfo)
		.setPInputAssemblyState(&inputAssemblyInfo)
		.setPViewportState(&viewportInfo)
		.setPRasterizationState(&rasterizationInfo)
		.setPMultisampleState(&multisampleInfo)
		.setPDepthStencilState(&depthStencilInfo)
		.setPColorBlendState(&colorBlendInfo)
		.setPDynamicState(&dynamicStateInfo)
		.setLayout(pipeline_layout)
		.setRenderPass(render_pass);

}


void DisplaySurface::Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber)
{
	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->lock_for_write();

	if (!swapchain)
		InitSwapChain();

	auto *vulkanDevice = renderPlatform->AsVulkanDevice();
// Ensure no more than FRAME_LAG renderings are outstanding
	vk::Result result=vulkanDevice->waitForFences(1, &fences[frame_index], VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess)
	{
		SIMUL_CERR << "Vulkan operation failed\n";
	}
	result = vulkanDevice->resetFences(1, &fences[frame_index]);
	if (result != vk::Result::eSuccess)
	{
		SIMUL_CERR << "Vulkan operation failed\n";
	}
	do
	{
		result = vulkanDevice->acquireNextImageKHR(swapchain, UINT64_MAX, image_acquired_semaphores[frame_index], vk::Fence(), &current_buffer);
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
// demo->swapchain is out of date (e.g. the window was resized) and
// must be recreated:
			Resize();
		}
		else if (result == vk::Result::eSuboptimalKHR)
		{
// swapchain is not as optimal as it could be, but the platform's
// presentation engine will still present the image correctly.
			break;
		}
		else
		{
			SIMUL_ASSERT(result == vk::Result::eSuccess);
		}
	} while (result != vk::Result::eSuccess);

	vulkanDevice->waitIdle();
	
	SwapchainImageResources &res=swapchain_image_resources[current_buffer];
	auto& commandBuffer = res.cmd;
	auto& fb = swapchain_image_resources[current_buffer].framebuffer;

	auto const commandInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	result = commandBuffer.begin(&commandInfo);
	SIMUL_VK_CHECK(result);

	deferredContext.platform_context = &commandBuffer;
	deferredContext.renderPlatform = renderPlatform;

	renderPlatform->StoreRenderState(deferredContext);
	EnsureImageLayout();

	ERRNO_BREAK
	if (renderer)
	{
		auto *rp = (vulkan::RenderPlatform*)renderPlatform;
		rp->SetDefaultColourFormat(pixelFormat);
		renderer->Render(mViewId, deferredContext.platform_context, &fb, viewport.w, viewport.h, frameNumber);
	}

	renderPlatform->RestoreRenderState(deferredContext);
	EnsureImagePresentLayout();

	commandBuffer.end();

	Present();
	current_buffer++;
	current_buffer=current_buffer%(swapchain_image_resources.size());
	Resize();
	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->unlock_from_write();
}

void DisplaySurface::EnsureImageLayout()
{
	vk::Image &image=swapchain_image_resources[current_buffer].image;

	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits();
	vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eBottomOfPipe;
	vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eAllCommands;	// very general..

	auto  barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setOldLayout(vk::ImageLayout::eUndefined)
		.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1))
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image);
	swapchain_image_resources[current_buffer].cmd.pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);

}

void DisplaySurface::EnsureImagePresentLayout()
{
	vk::Image& image = swapchain_image_resources[current_buffer].image;

	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	vk::AccessFlags dstAccessMask = vk::AccessFlagBits();
	vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	auto  barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1))
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image);
	swapchain_image_resources[current_buffer].cmd.pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);

}

void DisplaySurface::Present()
{
	auto *vulkanDevice = renderPlatform->AsVulkanDevice();

	//update_data_buffer();

	// Wait for the image acquired semaphore to be signaled to ensure
	// that the image won't be rendered to until the presentation
	// engine has fully released ownership to the application, and it is
	// okay to render to the image.
	vk::PipelineStageFlags const pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto const submit_info = vk::SubmitInfo()
								.setPWaitDstStageMask(&pipe_stage_flags)
								.setWaitSemaphoreCount(1)
								.setPWaitSemaphores(&image_acquired_semaphores[frame_index])
								.setCommandBufferCount(1)
								.setPCommandBuffers(&swapchain_image_resources[current_buffer].cmd)
								.setSignalSemaphoreCount(1)
								.setPSignalSemaphores(&draw_complete_semaphores[frame_index]);

	vk::Result result = graphics_queue.submit(1, &submit_info, fences[frame_index]);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	bool separate_present_queue=(graphics_queue_family_index!=present_queue_family_index);
	if (separate_present_queue)
	{
// If we are using separate queues, change image ownership to the
// present queue before presenting, waiting for the draw complete
// semaphore and signalling the ownership released semaphore when
// finished
		auto const present_submit_info = vk::SubmitInfo()
			.setPWaitDstStageMask(&pipe_stage_flags)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&draw_complete_semaphores[frame_index])
			.setCommandBufferCount(1)
			.setPCommandBuffers(&swapchain_image_resources[current_buffer].graphics_to_present_cmd)
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&image_ownership_semaphores[frame_index]);

		result = present_queue.submit(1, &present_submit_info, vk::Fence());
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

	// If we are using separate queues we have to wait for image ownership,
	// otherwise wait for draw complete
	auto const presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(separate_present_queue ? &image_ownership_semaphores[frame_index]
			: &draw_complete_semaphores[frame_index])
		.setSwapchainCount(1)
		.setPSwapchains(&swapchain)
		.setPImageIndices(&current_buffer);

	result = present_queue.presentKHR(&presentInfo);
	frame_index += 1;
	frame_index %= swapchain_image_resources.size();
	if (result == vk::Result::eErrorOutOfDateKHR)
	{
// swapchain is out of date (e.g. the window was resized) and
// must be recreated:
	//	Resize();
	}
	else if (result == vk::Result::eSuboptimalKHR)
	{
// swapchain is not as optimal as it could be, but the platform's
// presentation engine will still present the image correctly.
	}
	else
	{
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
}


void DisplaySurface::EndFrame()
{
	RestoreDeviceObjects(mHwnd,renderPlatform,false,0,1,pixelFormat);
	// We check for resize here, because we must manage the SwapChain from the main thread.
	// we may have to do it after executing the command list, because Resize destroys the CL, and we don't want to lose commands.
	Resize();
}

void DisplaySurface::Resize()
{
#ifdef _MSC_VER
	RECT rect;
	if (!GetClientRect((HWND)mHwnd, &rect))
		return;
	UINT W = abs(rect.right - rect.left);
	UINT H = abs(rect.bottom - rect.top);
	if (viewport.w == W&&viewport.h == H)
		return;
	if(W*H>0)
		InitSwapChain();

	viewport.w = W;
	viewport.h = H;
	viewport.x = 0;
	viewport.y = 0;
	if(renderer)
		renderer->ResizeView(mViewId, W, H);
#endif
}