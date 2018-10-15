#ifdef _MSC_VER
#include <stdlib.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#endif
#include "DeviceManager.h"

#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Scene/Scene.h"
#include "Simul/Scene/Object.h"
#include "Simul/Scene/BaseObjectRenderer.h"
#include "Simul/Scene/BaseSceneRenderer.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include <stdint.h> // for uintptr_t
#include <iomanip>

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
		DeviceManager *deviceManager=nullptr;
	}
}

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace vulkan;

using namespace std;

#ifdef _DEBUG
#pragma comment(lib, "vulkan-1")
#else
#pragma comment(lib, "vulkan-1")
#endif

static std::vector<std::string> debugMsgGroups;
static void VulkanDebugCallback()
{
	SIMUL_BREAK("");
}

// Allow a maximum of two outstanding presentation operations.

class simul::vulkan::DeviceManagerInternal
{
public:
	vk::Instance instance;
	vk::PhysicalDevice gpu;
	vk::Device device;
	vk::Queue graphics_queue;
	vk::Queue present_queue;
	vk::Semaphore image_acquired_semaphores[SIMUL_VULKAN_FRAME_LAG];
	vk::Semaphore draw_complete_semaphores[SIMUL_VULKAN_FRAME_LAG];
	vk::Semaphore image_ownership_semaphores[SIMUL_VULKAN_FRAME_LAG];
    vk::Fence fences[SIMUL_VULKAN_FRAME_LAG];
	vk::PhysicalDeviceProperties gpu_props;
	std::unique_ptr<vk::QueueFamilyProperties[]> queue_props;
	vk::PhysicalDeviceMemoryProperties memory_properties;
};

DeviceManager::DeviceManager()
	:renderPlatformVulkan(NULL)
	, enabled_extension_count(0)
	, enabled_layer_count(0)
	, graphics_queue_family_index(0)
	, present_queue_family_index(0)
	, current_buffer(0)
	, queue_family_count(0)
	,device_initialized(false)
{
	deviceManager=this;
	if (!renderPlatformVulkan)
		renderPlatformVulkan = new vulkan::RenderPlatform;
	renderPlatformVulkan->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED | crossplatform::TRY_AGAIN_ON_FAIL | crossplatform::BREAK_ON_FAIL);
//	simul::crossplatform::Profiler::GetGlobalProfiler().Initialize(NULL);
	deviceManagerInternal = new DeviceManagerInternal;
}

void DeviceManager::InvalidateDeviceObjects()
{
	int err = errno;
	std::cout << "Errno " << err << std::endl;
	errno = 0;
//	simul::vulkan::Profiler::GetGlobalProfiler().Uninitialize();
}

DeviceManager::~DeviceManager()
{
	InvalidateDeviceObjects();
	delete renderPlatformVulkan;
	delete deviceManagerInternal;
}

static void GlfwErrorCallback(int errcode, const char* info)
{
	SIMUL_CERR << " " << errcode << ": " << info << std::endl;
}

static bool CheckLayers(uint32_t check_count, char const *const *const check_names, uint32_t layer_count,
	vk::LayerProperties *layers)
{
	for (uint32_t i = 0; i < check_count; i++)
	{
		bool found = false;
		for (uint32_t j = 0; j < layer_count; j++)
		{
			if (!strcmp(check_names[i], layers[j].layerName))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
			return false;
		}
	}
	return true;
}



void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
	uint32_t instance_extension_count = 0;
	uint32_t instance_layer_count = 0;
	uint32_t validation_layer_count = 0;
	char const *const *instance_validation_layers = nullptr;
	enabled_extension_count = 0;
	enabled_layer_count = 0;

	char const *const instance_validation_layers_alt1[] = { "VK_LAYER_LUNARG_standard_validation" };

	char const *const instance_validation_layers_alt2[] = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
														   "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
														   "VK_LAYER_GOOGLE_unique_objects" };

	// Look for validation layers
	vk::Bool32 validation_found = VK_FALSE;
	if (use_debug)
	{
		auto result = vk::enumerateInstanceLayerProperties(&instance_layer_count, nullptr);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		instance_validation_layers = instance_validation_layers_alt1;
		if (instance_layer_count > 0)
		{
			std::unique_ptr<vk::LayerProperties[]> instance_layers(new vk::LayerProperties[instance_layer_count]);
			result = vk::enumerateInstanceLayerProperties(&instance_layer_count, instance_layers.get());
			SIMUL_ASSERT(result == vk::Result::eSuccess);

			validation_found = CheckLayers(_countof(instance_validation_layers_alt1), instance_validation_layers,
				instance_layer_count, instance_layers.get());
			if (validation_found)
			{
				enabled_layer_count = _countof(instance_validation_layers_alt1);
				enabled_layers[0] = "VK_LAYER_LUNARG_standard_validation";
				validation_layer_count = 1;
			}
			else
			{
				// use alternative set of validation layers
				instance_validation_layers = instance_validation_layers_alt2;
				enabled_layer_count = _countof(instance_validation_layers_alt2);
				validation_found = CheckLayers(_countof(instance_validation_layers_alt2), instance_validation_layers,
					instance_layer_count, instance_layers.get());
				validation_layer_count = _countof(instance_validation_layers_alt2);
				for (uint32_t i = 0; i < validation_layer_count; i++)
				{
					enabled_layers[i] = instance_validation_layers[i];
				}
			}
		}

		if (!validation_found)
		{
			SIMUL_BREAK(
				"vkEnumerateInstanceLayerProperties failed to find required validation layer.\n\n"
				"Please look at the Getting Started guide for additional information.\n"
				"vkCreateInstance Failure");
		}
	}

	/* Look for instance extensions */
	vk::Bool32 surfaceExtFound = VK_FALSE;
	vk::Bool32 platformSurfaceExtFound = VK_FALSE;
	memset(extension_names, 0, sizeof(extension_names));

	auto result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	if (instance_extension_count > 0)
	{
		std::unique_ptr<vk::ExtensionProperties[]> instance_extensions(new vk::ExtensionProperties[instance_extension_count]);
		result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.get());
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		for (uint32_t i = 0; i < instance_extension_count; i++)
		{
			if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				surfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
			}
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
			if (!strcmp(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			if (!strcmp(VK_KHR_XCB_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			if (!strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_MIR_KHR)
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
			if (!strcmp(VK_KHR_DISPLAY_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_DISPLAY_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			if (!strcmp(VK_MVK_IOS_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_MVK_IOS_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			if (!strcmp(VK_MVK_MACOS_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_MVK_MACOS_SURFACE_EXTENSION_NAME;
			}

#endif
			assert(enabled_extension_count < 64);
		}
	}

	if (!surfaceExtFound)
	{
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_SURFACE_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
	}

	if (!platformSurfaceExtFound)
	{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_WIN32_SURFACE_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_XCB_SURFACE_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_MIR_KHR)
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_XLIB_SURFACE_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_DISPLAY_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_IOS_MVK)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_MVK_IOS_SURFACE_EXTENSION_NAME
			" extension.\n\nDo you have a compatible "
			"Vulkan installable client driver (ICD) installed?\nPlease "
			"look at the Getting Started guide for additional "
			"information.\n"
			"vkCreateInstance Failure");
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_MVK_MACOS_SURFACE_EXTENSION_NAME
			" extension.\n\nDo you have a compatible "
			"Vulkan installable client driver (ICD) installed?\nPlease "
			"look at the Getting Started guide for additional "
			"information.\n"
			"vkCreateInstance Failure");
#endif
	}
	auto const app = vk::ApplicationInfo()
		.setPApplicationName("Simul")
		.setApplicationVersion(0)
		.setPEngineName("Simul")
		.setEngineVersion(0)
		.setApiVersion(VK_API_VERSION_1_0);
	auto const inst_info = vk::InstanceCreateInfo()
		.setPApplicationInfo(&app)
		.setEnabledLayerCount(enabled_layer_count)
		.setPpEnabledLayerNames(instance_validation_layers)
		.setEnabledExtensionCount(enabled_extension_count)
		.setPpEnabledExtensionNames(extension_names);

	result = vk::createInstance(&inst_info, nullptr, &deviceManagerInternal->instance);
	if (result == vk::Result::eErrorIncompatibleDriver)
	{
		SIMUL_BREAK(
			"Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
			"Please look at the Getting Started guide for additional information.\n",
			"vkCreateInstance Failure");
	}
	else if (result == vk::Result::eErrorExtensionNotPresent)
	{
		SIMUL_BREAK(
			"Cannot find a specified extension library.\n"
			"Make sure your layers path is set appropriately.\n",
			"vkCreateInstance Failure");
	}
	else if (result != vk::Result::eSuccess)
	{
		SIMUL_BREAK(
			"vkCreateInstance failed.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n",
			"vkCreateInstance Failure");
	}

	/* Make initial call to query gpu_count, then second call for gpu info*/
	uint32_t gpu_count;
	result = deviceManagerInternal->instance.enumeratePhysicalDevices(&gpu_count, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	if (gpu_count > 0)
	{
		std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
		result = deviceManagerInternal->instance.enumeratePhysicalDevices(&gpu_count, physical_devices.get());
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		/* For cube demo we just grab the first physical device */
		deviceManagerInternal->gpu = physical_devices[0];
	}
	else
	{
		SIMUL_BREAK(
			"vkEnumeratePhysicalDevices reported zero accessible devices.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n",
			"vkEnumeratePhysicalDevices Failure");
	}

	/* Look for device extensions */
	uint32_t device_extension_count = 0;
	vk::Bool32 swapchainExtFound = VK_FALSE;
	enabled_extension_count = 0;
	memset(extension_names, 0, sizeof(extension_names));

	result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	if (device_extension_count > 0)
	{
		std::unique_ptr<vk::ExtensionProperties[]> device_extensions(new vk::ExtensionProperties[device_extension_count]);
		result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, device_extensions.get());
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		for (uint32_t i = 0; i < device_extension_count; i++)
		{
			if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName))
			{
				swapchainExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			}
			assert(enabled_extension_count < 64);
		}
	}

	if (!swapchainExtFound)
	{
		SIMUL_BREAK("vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
			" extension.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
	}

	deviceManagerInternal->gpu.getProperties(&deviceManagerInternal->gpu_props);

	/* Call with nullptr data to get count */
	deviceManagerInternal->gpu.getQueueFamilyProperties(&queue_family_count, nullptr);
	assert(queue_family_count >= 1);

	deviceManagerInternal->queue_props.reset(new vk::QueueFamilyProperties[queue_family_count]);
	deviceManagerInternal->gpu.getQueueFamilyProperties(&queue_family_count, deviceManagerInternal->queue_props.get());


	// Query fine-grained feature support for this device.
	//  If app has specific feature requirements it should check supported
	//  features based on this query
	vk::PhysicalDeviceFeatures physDevFeatures;
	deviceManagerInternal->gpu.getFeatures(&physDevFeatures);

	InitDebugging();
}

void DeviceManager::CreateDevice()
{
    float const priorities[1] = {0.0};

    vk::DeviceQueueCreateInfo queues[2];
    queues[0].setQueueFamilyIndex(graphics_queue_family_index);
    queues[0].setQueueCount(1);
    queues[0].setPQueuePriorities(priorities);

    auto deviceInfo = vk::DeviceCreateInfo()
                          .setQueueCreateInfoCount(1)
                          .setPQueueCreateInfos(queues)
                          .setEnabledLayerCount(0)
                          .setPpEnabledLayerNames(nullptr)
                          .setEnabledExtensionCount(enabled_extension_count)
                          .setPpEnabledExtensionNames((const char *const *)extension_names)
                          .setPEnabledFeatures(nullptr);

    if (separate_present_queue) {
        queues[1].setQueueFamilyIndex(present_queue_family_index);
        queues[1].setQueueCount(1);
        queues[1].setPQueuePriorities(priorities);
        deviceInfo.setQueueCreateInfoCount(2);
    }

    auto result = deviceManagerInternal->gpu.createDevice(&deviceInfo, nullptr, &deviceManagerInternal->device);
    SIMUL_ASSERT(result == vk::Result::eSuccess);
}

void DeviceManager::GetQueues(vk::SurfaceKHR *surface)
{
	// Iterate over each queue to learn whether it supports presenting:
	std::unique_ptr<vk::Bool32[]> supportsPresent(new vk::Bool32[queue_family_count]);
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		deviceManagerInternal->gpu.getSurfaceSupportKHR(i, *surface, &supportsPresent[i]);
	}

	uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
	uint32_t presentQueueFamilyIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		if (deviceManagerInternal->queue_props[i].queueFlags & vk::QueueFlagBits::eGraphics)
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
		SIMUL_BREAK("Could not find both graphics and present queues\n", "Swapchain Initialization Failure");
	}
	

    graphics_queue_family_index = graphicsQueueFamilyIndex;
    present_queue_family_index = presentQueueFamilyIndex;
    separate_present_queue = (graphics_queue_family_index != present_queue_family_index);
	
	if(!device_initialized)
		CreateDevice();

    deviceManagerInternal->device.getQueue(graphics_queue_family_index, 0, &deviceManagerInternal->graphics_queue);
    if (!separate_present_queue) {
        deviceManagerInternal->present_queue = deviceManagerInternal->graphics_queue;
    } else {
        deviceManagerInternal->device.getQueue(present_queue_family_index, 0, &deviceManagerInternal->present_queue);
    }

	

	// Create semaphores to synchronize acquiring presentable buffers before
	// rendering and waiting for drawing to be complete before presenting
	auto const semaphoreCreateInfo = vk::SemaphoreCreateInfo();

	// Create fences that we can use to throttle if we get too far
	// ahead of the image presents
	auto const fence_ci = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);
	for (uint32_t i = 0; i < SIMUL_VULKAN_FRAME_LAG; i++)
	{
		auto result = deviceManagerInternal->device.createFence(&fence_ci, nullptr, &deviceManagerInternal->fences[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = deviceManagerInternal->device.createSemaphore(&semaphoreCreateInfo, nullptr, &deviceManagerInternal->image_acquired_semaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		result = deviceManagerInternal->device.createSemaphore(&semaphoreCreateInfo, nullptr, &deviceManagerInternal->draw_complete_semaphores[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		if (separate_present_queue)
		{
			result = deviceManagerInternal->device.createSemaphore(&semaphoreCreateInfo, nullptr, &deviceManagerInternal->image_ownership_semaphores[i]);
			SIMUL_ASSERT(result == vk::Result::eSuccess);
		}
	}

	// Get Memory information and properties
	deviceManagerInternal->gpu.getMemoryProperties(&deviceManagerInternal->memory_properties);
}

std::vector<vk::SurfaceFormatKHR> DeviceManager::GetSurfaceFormats(vk::SurfaceKHR *surface)
{
	// Get the list of VkFormat's that are supported:
	uint32_t formatCount;
	auto result = deviceManagerInternal->gpu.getSurfaceFormatsKHR(*surface, &formatCount, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::SurfaceFormatKHR> surfFormats(formatCount);
	result = deviceManagerInternal->gpu.getSurfaceFormatsKHR(*surface, &formatCount, surfFormats.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	
	return surfFormats;
}

std::vector<vk::Image> DeviceManager::GetSwapchainImages(vk::SwapchainKHR *swapchain)
{
	uint32_t swapchainImageCount;
	auto result = deviceManagerInternal->device.getSwapchainImagesKHR(*swapchain, &swapchainImageCount, nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::Image> swapchainImages(swapchainImageCount);
	result = deviceManagerInternal->device.getSwapchainImagesKHR(*swapchain, &swapchainImageCount, swapchainImages.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	return swapchainImages;
}

vk::PhysicalDevice *DeviceManager::GetGPU()
{
	return &deviceManagerInternal->gpu;
}

void DeviceManager::InitDebugging()
{

}

void	DeviceManager::Shutdown()
{

}

void*	DeviceManager::GetDevice()
{
	static void *ptr[2];
	ptr[0]=(void*)&deviceManagerInternal->device;
	ptr[1]=(void*)&deviceManagerInternal->instance;
	return (void*)ptr;
}

void*	DeviceManager::GetDeviceContext()
{
	return (void*)&deviceManagerInternal->instance;
}

int		DeviceManager::GetNumOutputs()
{
	return 1;
}

crossplatform::Output DeviceManager::GetOutput(int i)
{
	crossplatform::Output o;
	return o;
}


void DeviceManager::Activate()
{
}

void DeviceManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
}

void DeviceManager::ReloadTextures()
{
}

void DeviceManager::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext, int x0, int y0, int dx, int dy)
{
}