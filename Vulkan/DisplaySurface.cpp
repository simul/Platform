#include "DisplaySurface.h"
#include "DeviceManager.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/RenderDelegator.h"
#include "RenderPlatform.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <linux/input.h>
#endif
#include <vulkan/vulkan.hpp>

using namespace platform;
using namespace vulkan;

#ifdef _MSC_VER
static const char* GetErr()
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
				  NULL);
	if (lpMsgBuf)
		return (const char*)lpMsgBuf;
	else
		return "";
}
#endif

DisplaySurface::DisplaySurface(int view_id)
	: crossplatform::DisplaySurface(view_id), pixelFormat(crossplatform::UNKNOWN), imageIndex(0)
{
}

DisplaySurface::~DisplaySurface()
{
	InvalidateDeviceObjects();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* r, bool vsync, crossplatform::PixelFormat outFmt)
{
	if (mHwnd && mHwnd == handle)
	{
		return;
	}
	renderPlatform = r;
	pixelFormat = outFmt;
	requestedPixelFormat = outFmt;
	mHwnd = handle;
	mIsVSYNC = vsync;

#ifdef VK_USE_PLATFORM_WIN32_KHR
	auto hInstance = GetModuleHandle(nullptr);
	vk::Win32SurfaceCreateInfoKHR createInfo = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(hInstance)
		.setHwnd(mHwnd);
	auto rv = (vulkan::RenderPlatform*)renderPlatform;
	vk::Instance* instance = GetVulkanInstance();
	if (instance)
	{
		vk::Result result = instance->createWin32SurfaceKHR(&createInfo, nullptr, &surface);
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
		auto result = inst->createXcbSurfaceKHR(&createInfo, nullptr, &surface);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}
#else
	surface = *((VkSurfaceKHR*)handle);
#endif
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vk::AndroidSurfaceCreateInfoKHR createInfo = vk::AndroidSurfaceCreateInfoKHR()
													 .setWindow((ANativeWindow*)mHwnd);
	vk::Instance* inst = GetVulkanInstance();
	if (inst)
	{
		auto result = inst->createAndroidSurfaceKHR(&createInfo, nullptr, &surface);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

#endif
}

void DisplaySurface::InvalidateDeviceObjects()
{
	vk::Device* vulkanDevice = GetVulkanDevice();
	vk::Instance* vulkanInstanace = GetVulkanInstance();
	if (vulkanDevice && vulkanInstanace)
	{
		vulkanDevice->waitIdle();
		
		for (CommandBufferResources& cmdBufferResource : cmdBufferResources)
		{
			renderPlatform->DestroyCommandList((void*&)(VkCommandBuffer&)cmdBufferResource.cmdBuffer, cmdPool);
			if (presentCmdPool)
			{
				renderPlatform->DestroyCommandList((void*&)(VkCommandBuffer&)cmdBufferResource.graphicsToPresentCmdBuffer, presentCmdPool);
			}
		}
		cmdBufferResources.clear();

		renderPlatform->DestroyCommandAllocator((void*&)(VkCommandPool&)cmdPool);
		if (presentCmdPool)
		{
			renderPlatform->DestroyCommandAllocator((void*&)(VkCommandPool&)presentCmdPool);
		}

		for (SwapchainImageResources& swapchainImageResource : swapchainImageResources)
		{
			vulkanDevice->destroyFramebuffer(swapchainImageResource.framebuffer, nullptr);
			vulkanDevice->destroyImageView(swapchainImageResource.view, nullptr);
			// swapchainImageResource.image is owned by the swapchain
		}
		swapchainImageResources.clear();

		vulkanDevice->destroyRenderPass(renderPass, nullptr);

		vulkanDevice->destroyCommandPool(cmdPool, nullptr);
		vulkanDevice->destroyCommandPool(presentCmdPool, nullptr);

		vulkanDevice->destroySwapchainKHR(swapchain, nullptr);
		vulkanInstanace->destroySurfaceKHR(surface, nullptr);

		for (size_t i = 0; i < FrameCount; i++)
		{
			vulkanDevice->destroySemaphore(imageAcquiredSemaphores[i], nullptr);
			vulkanDevice->destroySemaphore(drawCompleteSemaphores[i], nullptr);
			vulkanDevice->destroySemaphore(imageOwnershipSemaphores[i], nullptr);
			vulkanDevice->destroyFence(fences[i], nullptr);
			
			imageAcquiredSemaphores[i] = nullptr;
			drawCompleteSemaphores[i] = nullptr;
			imageOwnershipSemaphores[i] = nullptr;
			fences[i] = nullptr;
		}
	}
}

void DisplaySurface::Render(platform::core::ReadWriteMutex* delegatorReadWriteMutex, long long frameNumber)
{
	if (delegatorReadWriteMutex)
	{
		if (delegatorReadWriteMutex->locked())
			return;
		delegatorReadWriteMutex->lock_for_write();
	}

	if (!swapchain)
		InitSwapChain();

	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	vk::Device* vulkanDevice = vulkanRenderPlatform->AsVulkanDevice();

	vk::Result result = vulkanDevice->waitForFences(1, &fences[frameIndex], VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess)
	{
		SIMUL_CERR << "Vulkan operation failed\n";
	}
	result = vulkanDevice->resetFences(1, &fences[frameIndex]);
	if (result != vk::Result::eSuccess)
	{
		SIMUL_CERR << "Vulkan operation failed\n";
	}
	result = vulkanDevice->acquireNextImageKHR(swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], vk::Fence(), &imageIndex);
	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		// Swapchain is out of date (e.g. the window was resized) and must be recreated:
		vulkanDevice->waitIdle();
		Resize();
		if (delegatorReadWriteMutex)
			delegatorReadWriteMutex->unlock_from_write();
		return;
	}
	// swapchain is not as optimal as it could be, but the platform's presentation engine will still present the image correctly.
	SIMUL_ASSERT((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));

	vk::CommandBuffer& commandBuffer = cmdBufferResources[frameIndex].cmdBuffer;
	vk::Framebuffer& framebuffer = swapchainImageResources[imageIndex].framebuffer;

	const vk::CommandBufferBeginInfo commandInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	result = commandBuffer.begin(&commandInfo);
	SIMUL_VK_CHECK(result);

	EnsureImageLayout();

	ERRNO_BREAK
	if (renderer)
	{
		vulkanRenderPlatform->SetDefaultColourFormat(pixelFormat);
		renderer->Render(mViewId, &commandBuffer, &framebuffer, viewport.w, viewport.h, frameNumber);
	}

	EnsureImagePresentLayout();
	commandBuffer.end();

	Present();

	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->unlock_from_write();
}

void DisplaySurface::EndFrame()
{
	RestoreDeviceObjects(mHwnd, renderPlatform, false, pixelFormat);
	// We check for resize here, because we must manage the SwapChain from the main thread.
	// we may have to do it after executing the command list, because Resize destroys the CL, and we don't want to lose commands.
	Resize();
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

	if (!surface)
		return;

	vk::Result result;
	vk::Device* device = GetVulkanDevice();
	vk::PhysicalDevice* gpu = GetGPU();

	// If we just re-created an existing swapchain, we should destroy the old swapchain at this point.
	// Note: destroying the swapchain also cleans up all its associated presentable images once the platform is done with them.
	if (swapchain)
	{
		device->waitIdle(); // Ensure not work on the GPU, before destroying the swapchain.
		device->destroySwapchainKHR(swapchain, nullptr);
	}

	// what formats are supported?
	uint32_t surfaceformats = 0;
	result = gpu->getSurfaceFormatsKHR(surface, &surfaceformats, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::SurfaceFormatKHR> vkSurfaceFormats(surfaceformats);
	result = gpu->getSurfaceFormatsKHR(surface, &surfaceformats, vkSurfaceFormats.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	
	vulkanFormat = vk::Format::eUndefined;
	colourSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	pixelFormat = requestedPixelFormat;

	// Initialize the swap chain description. Let's try to find a format/colourspace combo that matches our specified pixelFormat.
	{
		for (int i = 0; i < vkSurfaceFormats.size(); i++)
		{
			vk::Format vf = (vk::Format)vkSurfaceFormats[i].format;
			if (RenderPlatform::ToVulkanFormat(pixelFormat) == vf)
			{
				vulkanFormat = vf;
				colourSpace = (vk::ColorSpaceKHR)vkSurfaceFormats[i].colorSpace;
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
					colourSpace = (vk::ColorSpaceKHR)vkSurfaceFormats[i].colorSpace;
					break;
				}
			}
		}
		switch (colourSpace)
		{
		case vk::ColorSpaceKHR::eSrgbNonlinear:
		case vk::ColorSpaceKHR::eDisplayP3NonlinearEXT:
		case vk::ColorSpaceKHR::eDciP3NonlinearEXT:
		case vk::ColorSpaceKHR::eBt709NonlinearEXT:
		case vk::ColorSpaceKHR::eAdobergbNonlinearEXT:
		case vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT:
		case vk::ColorSpaceKHR::eHdr10St2084EXT:
		// case vk::ColorSpaceKHR::eDolbyvisionEXT:
		case vk::ColorSpaceKHR::eHdr10HlgEXT:
			swapChainIsGammaEncoded = true;
			break;
		case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT:
		case vk::ColorSpaceKHR::eDisplayP3LinearEXT:
		case vk::ColorSpaceKHR::eBt709LinearEXT:
		case vk::ColorSpaceKHR::eBt2020LinearEXT:
		case vk::ColorSpaceKHR::eAdobergbLinearEXT:
		case vk::ColorSpaceKHR::ePassThroughEXT:
		case vk::ColorSpaceKHR::eDisplayNativeAMD:
		default:
			swapChainIsGammaEncoded = false;
			break;
		};
	}

	// Check the surface capabilities and formats
	vk::SurfaceCapabilitiesKHR surfCapabilities;
	result = gpu->getSurfaceCapabilitiesKHR(surface, &surfCapabilities);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	uint32_t presentModeCount;
	result = gpu->getSurfacePresentModesKHR(surface, &presentModeCount, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::PresentModeKHR> presentModes(presentModeCount);
	result = gpu->getSurfacePresentModesKHR(surface, &presentModeCount, presentModes.data());
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
	for (uint32_t i = 0; i < std::size(compositeAlphaFlags); i++)
	{
		if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i])
		{
			compositeAlpha = compositeAlphaFlags[i];
			break;
		}
	}

	// Create Swapchain
	const vk::SwapchainCreateInfoKHR swapchainCI = vk::SwapchainCreateInfoKHR()
		.setMinImageCount(desiredNumOfSwapchainImages)
		.setSurface(surface)
		.setImageFormat(vulkanFormat)
		.setImageColorSpace(colourSpace)
		.setImageExtent({swapchainExtent.width, swapchainExtent.height})
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setPreTransform(preTransform)
		.setCompositeAlpha(compositeAlpha)
		.setPresentMode(mIsVSYNC ? swapchainPresentMode : vk::PresentModeKHR::eImmediate) // Use vk::PresentModeKHR::eImmediate for no v-sync.
		.setClipped(true)
		.setOldSwapchain(VK_NULL_HANDLE);
	result = device->createSwapchainKHR(&swapchainCI, nullptr, &swapchain);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform, swapchain, "Swapchain");

	// Get Swapchain Images
	uint32_t swapchainImageCount;
	result = device->getSwapchainImagesKHR(swapchain, (uint32_t*)&swapchainImageCount, (vk::Image*)nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::Image> swapchainImages(swapchainImageCount);
	result = device->getSwapchainImagesKHR(swapchain, (uint32_t*)&swapchainImageCount, swapchainImages.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		SetVulkanName(renderPlatform, swapchainImages[i], std::format("Swapchain Image {}", i));
	}

	cmdBufferResources.resize(swapchainImages.size());
	swapchainImageResources.resize(swapchainImages.size());

	if (swapchainImageResources.size() > FrameCount)
	{
		SIMUL_BREAK("swapchainImageResources is larger than FrameCount");
	}

	for (uint32_t i = 0; i < swapchainImages.size(); i++)
	{
		vk::ImageViewCreateInfo colourImageView = vk::ImageViewCreateInfo()
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vulkanFormat)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

		swapchainImageResources[i].image = swapchainImages[i];
		colourImageView.image = swapchainImageResources[i].image;

		result = device->createImageView(&colourImageView, nullptr, &swapchainImageResources[i].view);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform, swapchainImages[i], std::format("Swapchain ImageView {}", i));
	}

	GetQueues();

	CreateSyncObjects();
	CreateCommandPoolsAndBuffers();

	CreateRenderPass();
	CreateFramebuffers();
}

void DisplaySurface::GetQueues()
{
	vk::Device* vulkanDevice = GetVulkanDevice();
	vk::PhysicalDevice* gpu = GetGPU();

	graphicsQueueFamilyIndex = UINT32_MAX;
	presentQueueFamilyIndex = UINT32_MAX;

	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	graphicsQueueFamilyIndex = vulkanRenderPlatform->GetQueueFamilyIndex(crossplatform::DeviceContextType::GRAPHICS);

	// Iterate over each queue to learn whether it supports presenting:
	for (uint32_t i = 0; i < (uint32_t)vulkanRenderPlatform->GetQueueProperties().size(); i++)
	{
		vk::Bool32 supportsPresent = VK_FALSE;
		vk::Result result = gpu->getSurfaceSupportKHR(i, surface, &supportsPresent);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		if (supportsPresent)
		{
			presentQueueFamilyIndex = i;
			break;
		}
	}

	// Generate error if could not find both a graphics and a present queue
	if (graphicsQueueFamilyIndex == UINT32_MAX || presentQueueFamilyIndex == UINT32_MAX)
	{
		SIMUL_BREAK("Could not find both graphics and present queues.");
	}

	vulkanDevice->getQueue(graphicsQueueFamilyIndex, 0, &graphicsQueue);
	bool separatePresentQueue = (graphicsQueueFamilyIndex != presentQueueFamilyIndex);
	if (separatePresentQueue)
	{
		vulkanDevice->getQueue(presentQueueFamilyIndex, 0, &presentQueue);
	}
	else
	{
		presentQueue = graphicsQueue;
	}
}

void DisplaySurface::CreateSyncObjects()
{
	vk::Device* device = GetVulkanDevice();

	// Create semaphores to synchronize acquiring presentable buffers before rendering and waiting for drawing to be complete before presenting
	const auto semaphoreCI = vk::SemaphoreCreateInfo();
	// Create fences that we can use to throttle if we get too far ahead of the image presents
	const auto fenceCI = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

	for (uint32_t i = 0; i < swapchainImageResources.size(); i++)
	{
		auto result = device->createFence(&fenceCI, nullptr, &fences[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = device->createSemaphore(&semaphoreCI, nullptr, &imageAcquiredSemaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = device->createSemaphore(&semaphoreCI, nullptr, &drawCompleteSemaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		bool separatePresentQueue = (graphicsQueueFamilyIndex != presentQueueFamilyIndex);
		if (separatePresentQueue)
		{
			result = device->createSemaphore(&semaphoreCI, nullptr, &imageOwnershipSemaphores[i]);
			SIMUL_ASSERT(result == vk::Result::eSuccess);
		}
	}
}

void DisplaySurface::CreateCommandPoolsAndBuffers()
{
	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	vk::Device* device = GetVulkanDevice();

	bool separatePresentQueue = (graphicsQueueFamilyIndex != presentQueueFamilyIndex);

	// Before Creating/Recreating Command Buffers/Pool, we must ensure that they are not being used on the GPU.
	// When a pool is destroyed, all command buffers allocated from the pool are freed.
	auto CreateCommandPoolAndBuffers = [&device, &vulkanRenderPlatform, this](vk::CommandPool& cmdPool, uint32_t queueFamilyIndex)
	{
		if (cmdPool)
		{
			device->waitIdle();
			device->destroyCommandPool(cmdPool, nullptr);
		}

		const vk::CommandPoolCreateInfo cmdPoolCI = vk::CommandPoolCreateInfo()
			.setQueueFamilyIndex(queueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		cmdPool = (VkCommandPool)vulkanRenderPlatform->CreateCommandAllocator(crossplatform::DeviceContextType::GRAPHICS);
		SIMUL_ASSERT(cmdPool != nullptr);
		SetVulkanName(renderPlatform, cmdPool, std::format("Command Pool ({})", queueFamilyIndex));

		const vk::CommandBufferAllocateInfo cmdBufferAI = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmdPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		for (size_t i = 0; i < cmdBufferResources.size(); ++i)
		{
			vk::CommandBuffer& cmdBuffer = cmdBufferResources[i].cmdBuffer;
			if (presentQueueFamilyIndex == queueFamilyIndex)
			{
				cmdBuffer = cmdBufferResources[i].graphicsToPresentCmdBuffer;
			}

			cmdBuffer = (VkCommandBuffer)vulkanRenderPlatform->CreateCommandList(crossplatform::DeviceContextType::GRAPHICS, cmdPool);
			SIMUL_ASSERT(cmdBuffer != nullptr);
			SetVulkanName(renderPlatform, cmdBufferResources[i].cmdBuffer, std::format("Command Buffer {}", i));
		}
	};

	CreateCommandPoolAndBuffers(cmdPool, graphicsQueueFamilyIndex);

	if (separatePresentQueue)
	{
		CreateCommandPoolAndBuffers(presentCmdPool, presentQueueFamilyIndex);

		// Create Image Ownership transfer command buffers for later execution.
		for (uint32_t i = 0; i < cmdBufferResources.size(); i++)
		{
			const vk::CommandBufferBeginInfo cmdBufferBI = vk::CommandBufferBeginInfo()
				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			
			vk::Result result = cmdBufferResources[i].graphicsToPresentCmdBuffer.begin(&cmdBufferBI);
			SIMUL_ASSERT(result == vk::Result::eSuccess);

			const vk::ImageMemoryBarrier imageOwnershipBarrier = vk::ImageMemoryBarrier()
					.setSrcAccessMask(vk::AccessFlags())
					.setDstAccessMask(vk::AccessFlags())
					.setOldLayout(vk::ImageLayout::ePresentSrcKHR)
					.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
					.setSrcQueueFamilyIndex(graphicsQueueFamilyIndex)
					.setDstQueueFamilyIndex(presentQueueFamilyIndex)
					.setImage(swapchainImageResources[i].image)
					.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

			cmdBufferResources[i].graphicsToPresentCmdBuffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlagBits(),
				0, nullptr, 0, nullptr, 1, &imageOwnershipBarrier);

			cmdBufferResources[i].graphicsToPresentCmdBuffer.end();
		}
	}
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

	crossplatform::DeviceContext deviceContext;
	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext, renderPass, 1, &pixelFormat);
}

void DisplaySurface::CreateFramebuffers()
{
	vk::Device* device = GetVulkanDevice();

	vk::ImageView attachment;
	const vk::FramebufferCreateInfo framebufferCI = vk::FramebufferCreateInfo()
		.setRenderPass(renderPass)
		.setAttachmentCount(1)
		.setPAttachments(&attachment)
		.setWidth((uint32_t)viewport.w)
		.setHeight((uint32_t)viewport.h)
		.setLayers(1);

	for (size_t i = 0; i < swapchainImageResources.size(); i++)
	{
		attachment = swapchainImageResources[i].view;
		vk::Result result = device->createFramebuffer(&framebufferCI, nullptr, &swapchainImageResources[i].framebuffer);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform, swapchainImageResources[i].framebuffer, std::format("Framebuffer {}", i));
	}

	if (renderer)
	{
		if (mViewId < 0)
			mViewId = renderer->AddView();
		renderer->ResizeView(mViewId, viewport.w, viewport.h);
	}
}

void DisplaySurface::Present()
{
	vk::Device* device = GetVulkanDevice();
	vk::Result result;

	bool separatePresentQueue = (graphicsQueueFamilyIndex != presentQueueFamilyIndex);

	// https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html#_discussion_of_solution
	// Wait for the image acquired semaphore to be signaled to ensure that the image won't be rendered to until the presentation
	// engine has fully released ownership to the application, and it is  okay to render to the image.
	const vk::PipelineStageFlags pipelineStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	const vk::SubmitInfo submitInfo = vk::SubmitInfo()
		.setPWaitDstStageMask(&pipelineStageFlags)
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&imageAcquiredSemaphores[frameIndex])
		.setCommandBufferCount(1)
		.setPCommandBuffers(&cmdBufferResources[frameIndex].cmdBuffer)
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&drawCompleteSemaphores[imageIndex]); 
	result = graphicsQueue.submit(1, &submitInfo, fences[frameIndex]);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	if (separatePresentQueue)
	{
		// If we are using separate queues, change image ownership to the present queue before presenting,
		// waiting for the draw complete semaphore and signalling the ownership released semaphore when finished.
		const vk::SubmitInfo presentSubmitInfo = vk::SubmitInfo()
			.setPWaitDstStageMask(&pipelineStageFlags)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&drawCompleteSemaphores[imageIndex])
			.setCommandBufferCount(1)
			.setPCommandBuffers(&cmdBufferResources[frameIndex].graphicsToPresentCmdBuffer)
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&imageOwnershipSemaphores[imageIndex]);
		result = presentQueue.submit(1, &presentSubmitInfo, vk::Fence());
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

	// If we are using separate queues we have to wait for image ownership, otherwise wait for draw complete
	const auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(separatePresentQueue ? &imageOwnershipSemaphores[imageIndex] : &drawCompleteSemaphores[imageIndex])
		.setSwapchainCount(1)
		.setPSwapchains(&swapchain)
		.setPImageIndices(&imageIndex);
	result = presentQueue.presentKHR(&presentInfo);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		// swapchain is out of date (e.g. the window was resized) and must be recreated:
		//	Resize();
	}
	else if (result == vk::Result::eSuboptimalKHR)
	{
		// swapchain is not as optimal as it could be, but the platform's presentation engine will still present the image correctly.
	}
	else
	{
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

	frameIndex += 1;
	frameIndex %= swapchainImageResources.size();
}

void DisplaySurface::Resize()
{
	auto* vulkanDevice = ((vulkan::RenderPlatform*)renderPlatform)->AsVulkanDevice();

	bool recreate = false;
	uint32_t W = 0;
	uint32_t H = 0;

#ifdef _MSC_VER
	RECT rect = {};
	if (!GetClientRect((HWND)mHwnd, &rect))
	{
		return;
	}
	RECT wrect = {};
	if (GetWindowRect((HWND)mHwnd, &wrect))
	{
		if (wrect.left != lastWindow.x || wrect.top != lastWindow.y)
			recreate = true;
		lastWindow.x = wrect.left;
		lastWindow.y = wrect.top;
		lastWindow.z = wrect.right - wrect.left;
		lastWindow.w = wrect.bottom - wrect.top;
	}
	W = abs(rect.right - rect.left);
	H = abs(rect.bottom - rect.top);
#else
	// On Linux the cp_hwnd is a VkSurfaceKHR* (see RestoreDeviceObjects, XCB path),
	// not a GLFWwindow*, so use the Vulkan surface capabilities to drive resize.
	vk::SurfaceCapabilitiesKHR surfCapabilities;
	vk::PhysicalDevice* gpu = GetGPU();
	if (gpu && surface)
	{
		auto result = gpu->getSurfaceCapabilitiesKHR(surface, &surfCapabilities);
		if (result == vk::Result::eSuccess && surfCapabilities.currentExtent.width != (uint32_t)-1)
		{
			W = surfCapabilities.currentExtent.width;
			H = surfCapabilities.currentExtent.height;
		}
	}
#endif

	// If we couldn't determine a new size, leave the existing swapchain alone.
	if (W == 0 || H == 0)
		return;
	if (viewport.w != W || viewport.h != H)
		recreate = true;
	if (!recreate)
		return;

	// InitSwapChain re-queries the surface capabilities, recreates the swapchain and framebuffers
	// at whatever size the surface reports at that moment, sets viewport to match, and calls
	// renderer->ResizeView via CreateFramebuffers. Do not overwrite viewport here with the W/H
	// captured above: a window manager can change the surface size between the two queries, and
	// using a stale W/H would produce a mismatch between the swapchain framebuffer dimensions
	// and the HDR framebuffer / renderArea, triggering VUID-VkRenderPassBeginInfo-pNext-02852/02853.
	InitSwapChain();
}

void DisplaySurface::EnsureImageLayout()
{
	vk::Image& image = swapchainImageResources[imageIndex].image;

	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits();
	vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	vk::PipelineStageFlags srcStages = vk::PipelineStageFlagBits::eBottomOfPipe;
	vk::PipelineStageFlags dstStages = vk::PipelineStageFlagBits::eAllCommands; // very general..

	vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setOldLayout(vk::ImageLayout::eUndefined)
		.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1))
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image);

	cmdBufferResources[frameIndex].cmdBuffer.pipelineBarrier(
		srcStages, dstStages,
		vk::DependencyFlagBits::eDeviceGroup,
		0, nullptr, 0, nullptr, 1, &barrier);
}

void DisplaySurface::EnsureImagePresentLayout()
{
	vk::Image& image = swapchainImageResources[imageIndex].image;

	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	vk::AccessFlags dstAccessMask = vk::AccessFlagBits();
	vk::PipelineStageFlags srcStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	vk::PipelineStageFlags dstStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1))
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image);

	cmdBufferResources[frameIndex].cmdBuffer.pipelineBarrier(
		srcStages, dstStages,
		vk::DependencyFlagBits::eDeviceGroup,
		0, nullptr, 0, nullptr, 1, &barrier);
}

vk::Instance* DisplaySurface::GetVulkanInstance()
{
	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	if (vulkanRenderPlatform)
	{
		return vulkanRenderPlatform->AsVulkanInstance();
	}
	return nullptr;
}

vk::Device* DisplaySurface::GetVulkanDevice()
{
	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	if (vulkanRenderPlatform)
	{
		return vulkanRenderPlatform->AsVulkanDevice();
	}
	return nullptr;
}

vk::PhysicalDevice* DisplaySurface::GetGPU()
{
	vulkan::RenderPlatform* vulkanRenderPlatform = (vulkan::RenderPlatform*)renderPlatform;
	if (vulkanRenderPlatform)
	{
		return vulkanRenderPlatform->GetVulkanGPU();
	}
	return nullptr;
}
