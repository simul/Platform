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
#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
#include "Simul/Platform/Vulkan/DisplaySurface.h"
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
	vk::PhysicalDeviceProperties gpu_props;
	vk::PhysicalDeviceMemoryProperties memory_properties;
};

DeviceManager::DeviceManager()
	:enabled_extension_count(0)
	,enabled_layer_count(0)
	,device_initialized(false)
{
	deviceManager=this;
//	if (!renderPlatformVulkan)
//		renderPlatformVulkan = new vulkan::RenderPlatform;
//	renderPlatformVulkan->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED | crossplatform::TRY_AGAIN_ON_FAIL | crossplatform::BREAK_ON_FAIL);
//	simul::crossplatform::Profiler::GetGlobalProfiler().Initialize(NULL);
	deviceManagerInternal = new DeviceManagerInternal;
}

void DeviceManager::InvalidateDeviceObjects()
{
	int err = errno;
	std::cout << "Errno " << err << std::endl;
	errno = 0;
//	delete renderPlatformVulkan;
//	renderPlatformVulkan=nullptr;
//	simul::vulkan::Profiler::GetGlobalProfiler().Uninitialize();
}

DeviceManager::~DeviceManager()
{
	InvalidateDeviceObjects();
	//delete renderPlatformVulkan;
	delete deviceManagerInternal;
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


bool DeviceManager::IsActive() const
{
	return device_initialized;
}

#define SIMUL_VK_ASSERT_RETURN(val) \
	if(val!=vk::Result::eSuccess)\
		return;

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
 	ERRNO_BREAK
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
	
 	ERRNO_BREAK
	// Look for validation layers
	vk::Bool32 validation_found = VK_FALSE;
	if (use_debug)
	{
		auto result = vk::enumerateInstanceLayerProperties((uint32_t*)&instance_layer_count, (vk::LayerProperties*)nullptr);
		SIMUL_VK_ASSERT_RETURN(result);

		instance_validation_layers = instance_validation_layers_alt1;
		if (instance_layer_count > 0)
		{
			std::unique_ptr<vk::LayerProperties[]> instance_layers(new vk::LayerProperties[instance_layer_count]);
			result = vk::enumerateInstanceLayerProperties((uint32_t*)&instance_layer_count, instance_layers.get());
			SIMUL_VK_ASSERT_RETURN(result);

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
 	ERRNO_BREAK

	/* Look for instance extensions */
	vk::Bool32 surfaceExtFound = VK_FALSE;
	vk::Bool32 platformSurfaceExtFound = VK_FALSE;
	vk::Bool32 debugExtFound = VK_FALSE;
	vk::Bool32 debugUtilsExtFound = VK_FALSE;
	
	// naming objects.
	vk::Bool32 nameExtFound=VK_FALSE;

	auto result = vk::enumerateInstanceExtensionProperties(nullptr, (uint32_t*)&instance_extension_count, (vk::ExtensionProperties*)nullptr);
	extension_names.resize(instance_extension_count);
	SIMUL_VK_ASSERT_RETURN(result);
	if (result != vk::Result::eSuccess)
		return;
	if (instance_extension_count > 0)
	{
		vk::ExtensionProperties * instance_extensions=new vk::ExtensionProperties[instance_extension_count];
		result = vk::enumerateInstanceExtensionProperties(nullptr, (uint32_t*)&instance_extension_count, instance_extensions);
		SIMUL_VK_ASSERT_RETURN(result);

		for (uint32_t i = 0; i < instance_extension_count; i++)
		{
			if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				debugExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
			}
			if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				surfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
			}
			if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				debugUtilsExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
			}
			if (!strcmp(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				nameExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
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
		delete[] instance_extensions;
	}
 	ERRNO_BREAK

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
 	ERRNO_BREAK
	auto const app = vk::ApplicationInfo()
		.setPApplicationName("Simul")
		.setApplicationVersion(0)
		.setPEngineName("Simul")
		.setEngineVersion(0)
		.setApiVersion(VK_API_VERSION_1_0);
	auto inst_info = vk::InstanceCreateInfo()
		.setPApplicationInfo(&app)
		.setEnabledLayerCount(enabled_layer_count)
		.setPpEnabledLayerNames(instance_validation_layers)
		.setEnabledExtensionCount(enabled_extension_count)
		.setPpEnabledExtensionNames(extension_names.data());
 	ERRNO_BREAK
	result = vk::createInstance(&inst_info, (vk::AllocationCallbacks*)nullptr, &deviceManagerInternal->instance);
	// Vulkan sets errno without warning or error.
	errno=0;
 	ERRNO_BREAK
	if (result == vk::Result::eErrorIncompatibleDriver)
	{
		SIMUL_BREAK(
			"Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
	}
	else if (result == vk::Result::eErrorExtensionNotPresent)
	{
		SIMUL_BREAK(
			"Cannot find a specified extension library.\n"
			"Make sure your layers path is set appropriately.\n"
			"vkCreateInstance Failure");
	}
	else if (result != vk::Result::eSuccess)
	{
		SIMUL_BREAK(
			"vkCreateInstance failed.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");
	}
 	ERRNO_BREAK

	/* Make initial call to query gpu_count, then second call for gpu info*/
	uint32_t gpu_count;
	result = deviceManagerInternal->instance.enumeratePhysicalDevices((uint32_t*)&gpu_count, (vk::PhysicalDevice*)nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
 	ERRNO_BREAK

	if (gpu_count > 0)
	{
		std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
		result = deviceManagerInternal->instance.enumeratePhysicalDevices((uint32_t*)&gpu_count, physical_devices.get());
		SIMUL_VK_ASSERT_RETURN(result);
		/* For cube demo we just grab the first physical device */
		deviceManagerInternal->gpu = physical_devices[0];
	}
	else
	{
		SIMUL_BREAK(
			"vkEnumeratePhysicalDevices reported zero accessible devices.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkEnumeratePhysicalDevices Failure");
	}
 	ERRNO_BREAK
	/* Look for device extensions */
	uint32_t device_extension_count = 0;
	vk::Bool32 swapchainExtFound = VK_FALSE;
	enabled_extension_count = 0;
	memset(extension_names.data(), 0, sizeof(extension_names));

	result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, (uint32_t*)&device_extension_count, (vk::ExtensionProperties*)nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
 	ERRNO_BREAK
	if (device_extension_count > 0)
	{
		std::unique_ptr<vk::ExtensionProperties[]> device_extensions(new vk::ExtensionProperties[device_extension_count]);
		result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, (uint32_t*)&device_extension_count, device_extensions.get());
		SIMUL_VK_ASSERT_RETURN(result);

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
 	ERRNO_BREAK

	deviceManagerInternal->gpu.getProperties(&deviceManagerInternal->gpu_props);
	
	InitQueueProperties(deviceManagerInternal->gpu,queue_props);

	// Query fine-grained feature support for this device.
	//  If app has specific feature requirements it should check supported
	//  features based on this query
	vk::PhysicalDeviceFeatures physDevFeatures;
	deviceManagerInternal->gpu.getFeatures(&physDevFeatures);
 	ERRNO_BREAK

	if(use_debug)
		InitDebugging();
	CreateDevice();
 	ERRNO_BREAK
}


void simul::vulkan::InitQueueProperties(const vk::PhysicalDevice &gpu, std::vector<vk::QueueFamilyProperties>& queue_props)
{
	uint32_t queue_family_count;
	/* Call with nullptr data to get count */
	gpu.getQueueFamilyProperties(&queue_family_count, (vk::QueueFamilyProperties*)nullptr);
	assert(queue_family_count >= 1);

	queue_props.resize(queue_family_count);
	gpu.getQueueFamilyProperties(&queue_family_count, queue_props.data());
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData)
{
	if(pLayerPrefix)
		std::cerr<<pLayerPrefix<<" layer: ";
	if((flags&VK_DEBUG_REPORT_ERROR_BIT_EXT)!=0)
		std::cerr<<" Error: ";
	if(pMessage)
		std::cerr << pMessage << std::endl;
	if((flags&VK_DEBUG_REPORT_ERROR_BIT_EXT)!=0)
		SIMUL_BREAK("Missing VK_DEBUG_REPORT_ERROR_BIT_EXT");
    return VK_FALSE;
}

void DeviceManager::SetupDebugCallback()
{
	#if 1//def _DEBUG
/* Load VK_EXT_debug_report entry points in debug builds */
		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT	=reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>     (vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkCreateDebugReportCallbackEXT"));
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT					=reinterpret_cast<PFN_vkDebugReportMessageEXT>            (vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkDebugReportMessageEXT"));
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>    (vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkDestroyDebugReportCallbackEXT"));

		/* Setup callback creation information */
		VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
		callbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		callbackCreateInfo.pNext       = nullptr;
		callbackCreateInfo.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT |
										 VK_DEBUG_REPORT_WARNING_BIT_EXT |
										 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackCreateInfo.pfnCallback = &DebugReportCallback;
		callbackCreateInfo.pUserData   = nullptr;

		/* Register the callback */
		VkDebugReportCallbackEXT callback;
		VkResult result = vkCreateDebugReportCallbackEXT(deviceManagerInternal->instance, &callbackCreateInfo, nullptr, &callback);
	#endif
}

void DeviceManager::CreateDevice()
{
	if(device_initialized)
		return;
    float const priorities[1] = {0.0};
    std::vector<vk::DeviceQueueCreateInfo> queues;
	queues.resize(GetQueueProperties().size());
	for(int i=0;i<queues.size();i++)
	{
		queues[i].setQueueFamilyIndex(i);
		queues[i].setQueueCount(1);
		queues[i].setPQueuePriorities(priorities);
	}
	vk::PhysicalDeviceFeatures features;
	features.setVertexPipelineStoresAndAtomics(1);
	features.setDualSrcBlend(1);		// For compositing shaders.
	features.setImageCubeArray(1);
	features.setSamplerAnisotropy(1);
    auto deviceInfo = vk::DeviceCreateInfo()
                          .setQueueCreateInfoCount(1)
                          .setPQueueCreateInfos(queues.data())
                          .setEnabledLayerCount(0)
                          .setPpEnabledLayerNames(nullptr)
                          .setEnabledExtensionCount(enabled_extension_count)
                          .setPpEnabledExtensionNames((const char *const *)extension_names.data())
                          .setPEnabledFeatures(&features)
						.setQueueCreateInfoCount((uint32_t)queues.size());
	/*
    if (separate_present_queue) {
        queues[1].setQueueFamilyIndex(present_queue_family_index);
        queues[1].setQueueCount(1);
        queues[1].setPQueuePriorities(priorities);
        deviceInfo.setQueueCreateInfoCount(2);
    }*/

    auto result = deviceManagerInternal->gpu.createDevice(&deviceInfo, nullptr, &deviceManagerInternal->device);
	device_initialized=result == vk::Result::eSuccess;
    SIMUL_ASSERT(device_initialized);
}

std::vector<vk::SurfaceFormatKHR> DeviceManager::GetSurfaceFormats(vk::SurfaceKHR *surface)
{
	// Get the list of VkFormat's that are supported:
	uint32_t formatCount;
	auto result = deviceManagerInternal->gpu.getSurfaceFormatsKHR(*surface, (uint32_t*)&formatCount, (vk::SurfaceFormatKHR*)nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::SurfaceFormatKHR> surfFormats(formatCount);
	result = deviceManagerInternal->gpu.getSurfaceFormatsKHR(*surface, (uint32_t*)&formatCount, surfFormats.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	
	return surfFormats;
}

std::vector<vk::Image> DeviceManager::GetSwapchainImages(vk::SwapchainKHR *swapchain)
{
	uint32_t swapchainImageCount;
	auto result = deviceManagerInternal->device.getSwapchainImagesKHR(*swapchain, (uint32_t*)&swapchainImageCount, (vk::Image*)nullptr);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	std::vector<vk::Image> swapchainImages(swapchainImageCount);
	result = deviceManagerInternal->device.getSwapchainImagesKHR(*swapchain, (uint32_t*)&swapchainImageCount, swapchainImages.data());
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	return swapchainImages;
}

vk::PhysicalDevice *DeviceManager::GetGPU()
{
	return &deviceManagerInternal->gpu;
}

vk::Device *DeviceManager::GetVulkanDevice()
{
	return &deviceManagerInternal->device;
}

vk::Instance *DeviceManager::GetVulkanInstance()
{
	return &deviceManagerInternal->instance;
}

void DeviceManager::InitDebugging()
{
	SetupDebugCallback();
}

void	DeviceManager::Shutdown()
{
	InvalidateDeviceObjects();
}

void*	DeviceManager::GetDevice()
{
	static void *ptr[3];
	ptr[0]=(void*)&deviceManagerInternal->device;
	ptr[1]=(void*)&deviceManagerInternal->instance;
	ptr[2]=(void*)&deviceManagerInternal->gpu;
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