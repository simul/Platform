#ifdef _MSC_VER
#include <stdlib.h>
#endif
#include "DeviceManager.h"

#include "Platform/CrossPlatform/Camera.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/Vulkan/DisplaySurface.h"
#include "Platform/Core/Timer.h"
#include <stdint.h> // for uintptr_t
#include <iomanip>
#include <sstream>
#include <regex>

#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <linux/input.h>
#endif
#include <vulkan/vulkan.hpp>
#ifdef NOMINMAX
#undef NOMINMAX
#endif
#include <vulkan/vk_sdk_platform.h>
#ifndef NOMINMAX

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

#endif
namespace platform
{
	namespace vulkan
	{
		DeviceManager *deviceManager=nullptr;
	}
}

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace platform;
using namespace vulkan;

using namespace std;

#ifdef _MSC_VER
#pragma comment(lib, "vulkan-1")
#endif
static std::vector<std::string> debugMsgGroups;
static void VulkanDebugCallback()
{
	SIMUL_BREAK("");
}

// Allow a maximum of two outstanding presentation operations.

class platform::vulkan::DeviceManagerInternal
{
public:
	vk::Instance instance;
	vk::PhysicalDevice gpu;
	vk::Device device;
	vk::PhysicalDeviceProperties gpu_props;
	vk::PhysicalDeviceMemoryProperties memory_properties;
	vk::PhysicalDeviceFeatures gpu_features;
	
	DeviceManagerInternal()
		: instance(vk::Instance()),
		gpu(vk::PhysicalDevice()),
		device(vk::Device()),
		gpu_props(vk::PhysicalDeviceProperties()),
		memory_properties(vk::PhysicalDeviceMemoryProperties()),
		gpu_features(vk::PhysicalDeviceFeatures()) {};
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
//	platform::crossplatform::Profiler::GetGlobalProfiler().Initialize(NULL);
	deviceManagerInternal = new DeviceManagerInternal;
}

void DeviceManager::InvalidateDeviceObjects()
{
	int err = errno;
	std::cout << "Errno " << err << std::endl;
	errno = 0;
//	delete renderPlatformVulkan;
//	renderPlatformVulkan=nullptr;
//	platform::vulkan::Profiler::GetGlobalProfiler().Uninitialize();
}

DeviceManager::~DeviceManager()
{
	InvalidateDeviceObjects();
	//delete renderPlatformVulkan;
	delete deviceManagerInternal;
}

static bool CheckLayers(uint32_t check_count, char const *const *const check_names, uint32_t layer_count, vk::LayerProperties *layers)
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
			ERRNO_CHECK;
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
	Initialize(use_debug,instrument,default_driver,std::vector<std::string>(),std::vector<std::string>());
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver,std::vector<std::string> required_device_extensions
	,std::vector<std::string> required_instance_extensions)
{
 	ERRNO_BREAK
	uint32_t apiVersion=VK_API_VERSION_1_0;
	//query the api version in order to use the correct vulkan functionality
	uint32_t* instanceVersion = (uint32_t*)malloc(sizeof(uint32_t));
	vk::Result result = vk::enumerateInstanceVersion(instanceVersion);

	//check what is returned
	if (result == vk::Result::eSuccess)
	{
		SIMUL_INTERNAL_COUT << "RESULT(vkEnumerateInstanceVersion) : Intance version enumeration successful\n" << std::endl;

		if (instanceVersion != nullptr)
		{
			SIMUL_INTERNAL_COUT << "API_VERSION : VK_API_VERSION_1_1\n" << std::endl;
			apiVersion = VK_API_VERSION_1_1;
		}
		else
		{
			SIMUL_INTERNAL_COUT << "API_VERSION : VK_API_VERSION_1_0\n" << std::endl;
		}
		SIMUL_INTERNAL_COUT << "Version number returned : " 
			<< VK_VERSION_MAJOR(*instanceVersion) << '.'
			<< VK_VERSION_MINOR(*instanceVersion) << '.'
			<< VK_VERSION_PATCH(*instanceVersion) << '\n';
	}
	else if (result == vk::Result::eErrorOutOfHostMemory)
	{
		SIMUL_CERR << "RESULT(vkEnumerateInstanceVersion) : eErrorOutOfHostMemory\n" << std::endl;
	}
	else
	{
		SIMUL_CERR << "RESULT(vkEnumerateInstanceVersion) : Something else returned while enumerating instance version\n" << std::endl;
	}

	uint32_t instance_extension_count = 0;
	uint32_t instance_layer_count = 0;
	uint32_t validation_layer_count = 0;
	char const *const *instance_validation_layers = nullptr;
	enabled_extension_count = 0;
	enabled_layer_count = 0;

	char const *const instance_validation_layers_main[] = { "VK_LAYER_KHRONOS_validation" };

	char const *const instance_validation_layers_alt1[] = { "VK_LAYER_LUNARG_standard_validation" };

	char const *const instance_validation_layers_alt2[] = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
														   "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
														   "VK_LAYER_GOOGLE_unique_objects" };
	
 	ERRNO_BREAK
	// Look for validation layers
	if (use_debug)
	{
		vk::Result result = vk::enumerateInstanceLayerProperties((uint32_t*)&instance_layer_count, (vk::LayerProperties*)nullptr);
		SIMUL_VK_ASSERT_RETURN(result);

		if (instance_layer_count > 0)
		{
			std::unique_ptr<vk::LayerProperties[]> instance_layers(new vk::LayerProperties[instance_layer_count]);
			result = vk::enumerateInstanceLayerProperties((uint32_t*)&instance_layer_count, instance_layers.get());
			SIMUL_VK_ASSERT_RETURN(result);
			if(instance_layer_count)
			{
				std::cout <<"Vulkan layers:\n";
			}
			for (uint32_t j = 0; j < instance_layer_count; j++)
			{
				std::cout << "\t"<<instance_layers[j].layerName<<std::endl;
			}

			vk::Bool32 validation_set = VK_FALSE;
			vk::Bool32 validation_found = VK_FALSE;
			//Main
			validation_found = CheckLayers(_countof(instance_validation_layers_main), instance_validation_layers_main, instance_layer_count, instance_layers.get());
			if (validation_found && !validation_set)
			{
				instance_validation_layers = instance_validation_layers_main;
				enabled_layer_count = _countof(instance_validation_layers_main);
				enabled_layers[0] = instance_validation_layers_main[0];
				validation_layer_count = 1;
				validation_set = VK_TRUE;
			}

			//Alt1
			validation_found = CheckLayers(_countof(instance_validation_layers_alt1), instance_validation_layers_alt1, instance_layer_count, instance_layers.get());
			if (validation_found && !validation_set)
			{
				instance_validation_layers = instance_validation_layers_alt1;
				enabled_layer_count = _countof(instance_validation_layers_alt1);
				enabled_layers[0] = instance_validation_layers_alt1[0];
				validation_layer_count = 1;
				validation_set = VK_TRUE;
			}

			//Alt2
			validation_found = CheckLayers(_countof(instance_validation_layers_alt2), instance_validation_layers_alt2, instance_layer_count, instance_layers.get());
			if (validation_found && !validation_set)
			{
				instance_validation_layers = instance_validation_layers_alt2;
				enabled_layer_count = _countof(instance_validation_layers_alt2);
				validation_layer_count = _countof(instance_validation_layers_alt2);
				for (uint32_t i = 0; i < validation_layer_count; i++)
				{
					enabled_layers[i] = instance_validation_layers_alt2[i];
				}
				validation_set = VK_TRUE;
			}

			//Error
			if (!validation_found && !validation_set)
			{
				SIMUL_BREAK(
					"vkEnumerateInstanceLayerProperties failed to find required validation layer.\n\n"
					"Please look at the Getting Started guide for additional information.\n"
					"vkCreateInstance Failure");
			}
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

	result = vk::enumerateInstanceExtensionProperties(nullptr, (uint32_t*)&instance_extension_count, (vk::ExtensionProperties*)nullptr);
	extension_names.resize(instance_extension_count);
	SIMUL_VK_ASSERT_RETURN(result);
	if (result != vk::Result::eSuccess)
		return;
	if (instance_extension_count > 0)
	{
		vk::ExtensionProperties * instance_extensions=new vk::ExtensionProperties[instance_extension_count];
		result = vk::enumerateInstanceExtensionProperties(nullptr, (uint32_t*)&instance_extension_count, instance_extensions);
		SIMUL_VK_ASSERT_RETURN(result);

		if(instance_layer_count)
		{
			SIMUL_COUT<<"Vulkan extensions supported on this instance:\n";
		}
		for (uint32_t i = 0; i < instance_extension_count; i++)
		{
			SIMUL_COUT << "\t"<<instance_extensions[i].extensionName<<std::endl;
			if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				debugExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
			}
			else if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				surfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
			}
			else if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				debugUtilsExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
			}
			else if (!strcmp(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				nameExtFound = 1;
				extension_names[enabled_extension_count++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			}
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			else if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
			else if (!strcmp(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			else if (!strcmp(VK_KHR_XCB_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			else if (!strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_MIR_KHR)
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
			else if (!strcmp(VK_KHR_DISPLAY_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_DISPLAY_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			else if (!strcmp(VK_MVK_IOS_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_MVK_IOS_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			else if (!strcmp(VK_MVK_MACOS_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_MVK_MACOS_SURFACE_EXTENSION_NAME;
			}
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			else if (!strcmp(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
			{
				platformSurfaceExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
			}
#endif
			else
			{
				for(size_t j=0;j<required_instance_extensions.size();j++)
				{
					if (!strcmp(required_instance_extensions[j].c_str(), instance_extensions[i].extensionName))
					{
						extension_names[enabled_extension_count++] = required_instance_extensions[j].c_str();
					}
				}
			}
			assert(enabled_extension_count < 64);
		}
		delete[] instance_extensions;
	}
	errno = 0;
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
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
		SIMUL_BREAK("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
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
		.setApiVersion(apiVersion);
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
		std::vector<vk::ExtensionProperties> device_extensions;
		device_extensions.resize(device_extension_count);
		result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, (uint32_t*)&device_extension_count, device_extensions.data());
		SIMUL_VK_ASSERT_RETURN(result);
		
		std::cout<<"Available device extensions: "<<device_extension_count<<std::endl;
		for (uint32_t i = 0; i < device_extension_count; i++)
		{
			std::cout<<device_extensions[i].extensionName<<std::endl;
			if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName))
			{
				swapchainExtFound = 1;
				extension_names[enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			}
			else
			{
				for(size_t j=0;j<required_device_extensions.size();j++)
				{
					if (!strcmp(required_device_extensions[j].c_str(), device_extensions[i].extensionName))
					{
						extension_names[enabled_extension_count++] = required_device_extensions[j].c_str();
					}
				}
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
	deviceManagerInternal->gpu.getFeatures(&deviceManagerInternal->gpu_features);
 	ERRNO_BREAK

	if(use_debug)
		InitDebugging();
 	ERRNO_BREAK
	CreateDevice();
 	ERRNO_BREAK
}

void platform::vulkan::InitQueueProperties(const vk::PhysicalDevice &gpu, std::vector<vk::QueueFamilyProperties>& queue_props)
{
	uint32_t queue_family_count;
	/* Call with nullptr data to get count */
	gpu.getQueueFamilyProperties(&queue_family_count, (vk::QueueFamilyProperties*)nullptr);
	assert(queue_family_count >= 1);

	queue_props.resize(queue_family_count);
	gpu.getQueueFamilyProperties(&queue_family_count, queue_props.data());
}
#ifdef _MSC_VER
#pragma optimize("",off)
#endif
void RewriteVulkanMessage( std::string &str)
{
	// If we have a number followed by a bracket at the start
	std::smatch m;
	std::regex re("0x([0-9a-f]+)");// e.g. 0x1c4e6c00000002c7
	std::string out;
	while(std::regex_search(str,m,re))
	{
		string hex_addr=m[1].str();
		std::stringstream sstr;
		unsigned long long num;
		sstr << std::hex << hex_addr.c_str();
		sstr >> num;

		out += m.prefix();
		out +=m.str();
		auto f=RenderPlatform::ResourceMap.find(num);
		if(f!=RenderPlatform::ResourceMap.end())
		{
			out+="(";
			out+=f->second+")";
		}
		str=m.suffix();
	}
	out+=str;
	str=out;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
	VkDebugReportFlagsEXT	   flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t					object,
	size_t					  location,
	int32_t					 messageCode,
	const char*				 pLayerPrefix,
	const char*				 pMessage,
	void*					   pUserData)
{
	if(pLayerPrefix)
		std::cerr<<pLayerPrefix<<" layer: ";
	if((flags&VK_DEBUG_REPORT_ERROR_BIT_EXT)!=0)
		std::cerr<<" Error: ";
	if ((flags& VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
		std::cerr<<" Warning: ";
	if(pMessage)
	{
		std::string str=pMessage;
		RewriteVulkanMessage(str);
		std::cerr << str.c_str() << std::endl;
	}
	if((flags&VK_DEBUG_REPORT_ERROR_BIT_EXT)!=0)
		SIMUL_BREAK("Vulkan Error");
	return VK_FALSE;
}

void DeviceManager::SetupDebugCallback()
{
	#if 1//def _DEBUG
/* Load VK_EXT_debug_report entry points in debug builds */
		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT	=reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>	(vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkCreateDebugReportCallbackEXT"));
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT					=reinterpret_cast<PFN_vkDebugReportMessageEXT>			(vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkDebugReportMessageEXT"));
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>	(vkGetInstanceProcAddr(deviceManagerInternal->instance, "vkDestroyDebugReportCallbackEXT"));

		/* Setup callback creation information */
		VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
		callbackCreateInfo.sType	   = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		callbackCreateInfo.pNext	   = nullptr;
		callbackCreateInfo.flags	   = VK_DEBUG_REPORT_ERROR_BIT_EXT |
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
	
	//Query Physical Device Features for compatibility
	bool gpu_feature_checks = true;
	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		gpu_feature_checks = false;
	#endif
 	ERRNO_BREAK
	if (gpu_feature_checks)
	{
		if(!deviceManagerInternal->gpu_features.vertexPipelineStoresAndAtomics)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"vertexPipelineStoresAndAtomics\". Unable to proceed.\n");
		if(!deviceManagerInternal->gpu_features.imageCubeArray)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"imageCubeArray\". Unable to proceed.\n");
		if(!deviceManagerInternal->gpu_features.dualSrcBlend)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"dualSrcBlend\". Unable to proceed.\n");
		if(!deviceManagerInternal->gpu_features.samplerAnisotropy)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"samplerAnisotropy\". Unable to proceed.\n");
		if(!deviceManagerInternal->gpu_features.fragmentStoresAndAtomics)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"fragmentStoresAndAtomics\". Unable to proceed.\n");
	}
 	ERRNO_BREAK
	auto deviceInfo = vk::DeviceCreateInfo()
							.setQueueCreateInfoCount(1)
							.setPQueueCreateInfos(queues.data())
							.setEnabledLayerCount(0)
							.setPpEnabledLayerNames(nullptr)
							.setEnabledExtensionCount(enabled_extension_count)
							.setPpEnabledExtensionNames((const char *const *)extension_names.data())
							.setPEnabledFeatures(&deviceManagerInternal->gpu_features)
							.setQueueCreateInfoCount((uint32_t)queues.size());
							
 	ERRNO_BREAK
	auto result = deviceManagerInternal->gpu.createDevice(&deviceInfo, nullptr, &deviceManagerInternal->device);
 	// For unknown reasons, even when successful, Vulkan createDevice sets errno==2: No such file or directory here.
	// So we reset it to prevent spurious error detection.
	errno=0;
	device_initialized=result == vk::Result::eSuccess;
	SIMUL_ASSERT(device_initialized);
 	ERRNO_BREAK
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

void DeviceManager::RenderDepthBuffers(crossplatform::GraphicsDeviceContext &deviceContext, int x0, int y0, int dx, int dy)
{
}