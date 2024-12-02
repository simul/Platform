#ifdef _MSC_VER
#include <stdlib.h>
#endif

#include "DeviceManager.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Vulkan/DisplaySurface.h"

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
//#include <vulkan/vk_sdk_platform.h>
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

//VK_EXT_debug_report

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
	VkDebugReportFlagsEXT	   flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t					object,
	size_t					  location,
	int32_t					 messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData);

//VK_EXT_debug_utils

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData);

static bool IsInVector(const std::vector<std::string>& container, const std::string& value)
{
	return std::find(container.begin(), container.end(), value) != container.end();
}
static void ExclusivePushBack(std::vector<std::string>& container, const std::string& value)
{
	if (!IsInVector(container, value))
		container.push_back(value);
}

static bool IsInVector(const std::vector<const char*>& container, const char* value)
{
	for (const char* element : container)
	{
		if (strcmp(element, value) == 0)
			return true;
	}
	return false;
}
static void ExclusivePushBack(std::vector<const char*>& container, const char* value)
{
	if (!IsInVector(container, value))
		container.push_back(value);
}

// Allow a maximum of two outstanding presentation operations.

class platform::vulkan::DeviceManagerInternal
{
public:
	vk::Instance instance;
	vk::PhysicalDevice gpu;
	vk::Device device;
	vk::PhysicalDeviceFeatures gpu_features;
	vk::PhysicalDeviceProperties gpu_props;
	vk::PhysicalDeviceMemoryProperties memory_properties;
	vk::PhysicalDeviceFeatures2 gpu_features2;
	vk::PhysicalDeviceProperties2 gpu_props2;
	
	DeviceManagerInternal()
		: instance(vk::Instance()),
		gpu(vk::PhysicalDevice()),
		device(vk::Device()),
		gpu_features(vk::PhysicalDeviceFeatures()),
		gpu_props(vk::PhysicalDeviceProperties()),
		memory_properties(vk::PhysicalDeviceMemoryProperties()),
		gpu_features2(vk::PhysicalDeviceFeatures2()),
		gpu_props2(vk::PhysicalDeviceProperties2()){};
};

DeviceManager::DeviceManager()
	:device_initialized(false)
{
	deviceManager=this;

	deviceManagerInternal = new DeviceManagerInternal;
}

void DeviceManager::InvalidateDeviceObjects()
{
	int err = errno;
	std::cout << "Errno " << err << std::endl;
	errno = 0;
}

DeviceManager::~DeviceManager()
{
	if (debugReportCallback)
	{
		vk::DispatchLoaderDynamic d;
		d.vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)deviceManagerInternal->instance.getProcAddr("vkDestroyDebugReportCallbackEXT");
		deviceManagerInternal->instance.destroyDebugReportCallbackEXT(debugReportCallback, nullptr, d);
	}
	if (debugUtilsMessenger)
	{
		vk::DispatchLoaderDynamic d;
		d.vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)deviceManagerInternal->instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
		deviceManagerInternal->instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger, nullptr, d);
	}

	InvalidateDeviceObjects();
	delete deviceManagerInternal;
}

static bool CheckLayers(uint32_t check_count, char const *const *const check_names, uint32_t layer_count, vk::LayerProperties *layers)
{
	for (uint32_t i = 0; i < check_count; i++)
	{
		bool found = false;
		for (uint32_t j = 0; j < layer_count; j++)
		{
			if (strcmp(check_names[i], layers[j].layerName) == 0)
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
	uint32_t instanceVersion = 0;
	vk::Result result = vk::enumerateInstanceVersion(&instanceVersion);

	//check what is returned
	if (result == vk::Result::eSuccess)
	{
		SIMUL_INTERNAL_COUT << "RESULT(vkEnumerateInstanceVersion) : Intance version enumeration successful\n" << std::endl;

		if (instanceVersion != 0)
			apiVersion = instanceVersion;
		
		SIMUL_INTERNAL_COUT << "Version number returned : " 
			<< VK_VERSION_MAJOR(instanceVersion) << '.'
			<< VK_VERSION_MINOR(instanceVersion) << '.'
			<< VK_VERSION_PATCH(instanceVersion) << '\n';
	}
	else if (result == vk::Result::eErrorOutOfHostMemory)
	{
		SIMUL_CERR << "RESULT(vkEnumerateInstanceVersion) : eErrorOutOfHostMemory\n" << std::endl;
	}
	else
	{
		SIMUL_CERR << "RESULT(vkEnumerateInstanceVersion) : Something else returned while enumerating instance version\n" << std::endl;
	}
	ERRNO_BREAK

	// Look for validation layers
	char const *const instance_validation_layers_main[] = { "VK_LAYER_KHRONOS_validation" };
	char const *const instance_validation_layers_alt1[] = { "VK_LAYER_LUNARG_standard_validation" };
	char const *const instance_validation_layers_alt2[] = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
														   "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
														   "VK_LAYER_GOOGLE_unique_objects" };
	if (use_debug)
	{
		uint32_t instance_layer_count = 0;
		vk::Result result = vk::enumerateInstanceLayerProperties(&instance_layer_count, (vk::LayerProperties*)nullptr);
		SIMUL_VK_ASSERT_RETURN(result);

		if (instance_layer_count > 0)
		{
			instance_layers.resize(instance_layer_count);
			result = vk::enumerateInstanceLayerProperties(&instance_layer_count, instance_layers.data());
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
			validation_found = CheckLayers(_countof(instance_validation_layers_main), instance_validation_layers_main, instance_layer_count, instance_layers.data());
			if (validation_found && !validation_set)
			{
				for (const char* instance_layer : instance_validation_layers_main)
					instance_layer_names.push_back(instance_layer);
				validation_set = VK_TRUE;
			}

			//Alt1
			validation_found = CheckLayers(_countof(instance_validation_layers_alt1), instance_validation_layers_alt1, instance_layer_count, instance_layers.data());
			if (validation_found && !validation_set)
			{
				for (const char* instance_layer : instance_validation_layers_alt1)
					instance_layer_names.push_back(instance_layer);
				validation_set = VK_TRUE;
			}

			//Alt2
			validation_found = CheckLayers(_countof(instance_validation_layers_alt2), instance_validation_layers_alt2, instance_layer_count, instance_layers.data());
			if (validation_found && !validation_set)
			{
				for (const char* instance_layer : instance_validation_layers_alt2)
					instance_layer_names.push_back(instance_layer);
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

	// Platform Surface Extension
	const char* platformSurfaceExt = nullptr;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	platformSurfaceExt = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	platformSurfaceExt = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	platformSurfaceExt = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_MIR_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)
	platformSurfaceExt = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
	platformSurfaceExt = VK_KHR_DISPLAY_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	platformSurfaceExt = VK_MVK_IOS_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	platformSurfaceExt = VK_MVK_MACOS_SURFACE_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	platformSurfaceExt = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
#endif 

#define PLATFORM_VULKAN_USE_RENDERDOC_META 0
#define PLATFORM_VULKAN_USE_RENDERDOC_META_ANDROID (PLATFORM_VULKAN_USE_RENDERDOC_META == 0 && defined(__ANDROID__))

#if PLATFORM_VULKAN_USE_RENDERDOC_META_ANDROID || PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif
	ExclusivePushBack(required_instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, platformSurfaceExt);

	uint32_t instance_extension_count = 0;
	result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, (vk::ExtensionProperties*)nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	if (result != vk::Result::eSuccess)
		return;

	if (instance_extension_count > 0)
	{
		std::vector<bool> found_required_instance_extension(required_instance_extensions.size(), false);

		instance_extensions.resize(instance_extension_count);
		result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.data());
		SIMUL_VK_ASSERT_RETURN(result);

		if(instance_extension_count)
		{
			SIMUL_COUT<<"Vulkan extensions supported on this instance:\n";
		}
		for (uint32_t i = 0; i < instance_extension_count; i++)
		{
			SIMUL_COUT << "\t"<<instance_extensions[i].extensionName<<std::endl;
			for (size_t j = 0; j < required_instance_extensions.size(); j++)
			{
				if (strcmp(required_instance_extensions[j].c_str(), instance_extensions[i].extensionName) == 0)
				{
					ExclusivePushBack(instance_extension_names, instance_extensions[i].extensionName);
					found_required_instance_extension[j] = true;
					break;
				}
			}
			
			assert(instance_extension_names.size() < 64);
		}
			
		for (size_t j = 0; j < required_instance_extensions.size(); j++)
		{
			if (!found_required_instance_extension[j])
			{
				SIMUL_CERR << "Missing the required instance extension " << required_instance_extensions[j].c_str() << std::endl;
			}
		}
	}
	errno = 0;
	ERRNO_BREAK

	std::vector<const char*> instance_layer_names_cstr;
	instance_layer_names_cstr.reserve(instance_layer_names.size());
	for (const auto& instance_layer_name : instance_layer_names)
		instance_layer_names_cstr.push_back(instance_layer_name.c_str());

	std::vector<const char*> instance_extension_names_cstr;
	instance_extension_names_cstr.reserve(instance_extension_names.size());
	for (const auto& instance_extension_name : instance_extension_names)
		instance_extension_names_cstr.push_back(instance_extension_name.c_str());

	void* instanceCI_pNext = nullptr;
	if (IsInVector(instance_extension_names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
	{
		//Add in vk::DebugUtilsMessengerCreateInfo to vk::InstanceCreateInfo::pNext for error callbacks on vk::createInstance!
		debugUtilsMessengerCI
			.setPNext(nullptr)
			.setFlags(vk::DebugUtilsMessengerCreateFlagBitsEXT(0))
			.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
			.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
				| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
				| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
			.setPfnUserCallback(DebugUtilsCallback)
			.setPUserData(this);
		instanceCI_pNext = &debugUtilsMessengerCI;
	}

	auto const app = vk::ApplicationInfo()
		.setPApplicationName("Simul")
		.setApplicationVersion(0)
		.setPEngineName("Simul")
		.setEngineVersion(0)
		.setApiVersion(apiVersion);
	auto inst_info = vk::InstanceCreateInfo()
		.setPNext(instanceCI_pNext)
		.setPApplicationInfo(&app)
		.setEnabledLayerCount((uint32_t)instance_layer_names_cstr.size())
		.setPpEnabledLayerNames(instance_layer_names_cstr.data())
		.setEnabledExtensionCount((uint32_t)instance_extension_names_cstr.size())
		.setPpEnabledExtensionNames(instance_extension_names_cstr.data());
	ERRNO_BREAK
	result = vk::createInstance(&inst_info, (vk::AllocationCallbacks*)nullptr, &deviceManagerInternal->instance);

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	if (result == vk::Result::eErrorExtensionNotPresent)
	{
		//Android Only: This is pure crazy. vk::createInstance can return vk::Result::eErrorExtensionNotPresent,
		//despite querying and loading all the layers and extensions. Second call with the same data return
		//successfully with an vk::Instance.
		//https://developer.android.com/ndk/guides/graphics/validation-layer
		result = vk::createInstance(&inst_info, (vk::AllocationCallbacks*)nullptr, &deviceManagerInternal->instance);
	}
#endif

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
			"Cannot find a specified extension.\n"
			"Make sure your layers path is set appropriately.\n"
			"vkCreateInstance Failure");
		for(uint32_t i=0;i<(uint32_t)instance_extension_names.size()+1;i++)
		{
			inst_info.setEnabledExtensionCount(i);
			result = vk::createInstance(&inst_info, (vk::AllocationCallbacks*)nullptr, &deviceManagerInternal->instance);
			if (result == vk::Result::eErrorExtensionNotPresent)
			{
				SIMUL_CERR<<"Fails on extension: "<<instance_extension_names[i]<<std::endl;
			}
		}
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
	result = deviceManagerInternal->instance.enumeratePhysicalDevices(&gpu_count, (vk::PhysicalDevice*)nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	errno=0;

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
	ExclusivePushBack(required_device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	ExclusivePushBack(required_device_extensions, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
#endif
#if PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	ExclusivePushBack(required_device_extensions, VK_KHR_MULTIVIEW_EXTENSION_NAME);
#endif
	ExclusivePushBack(required_device_extensions, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

	uint32_t device_extension_count = 0;
	result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, (vk::ExtensionProperties*)nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	ERRNO_CHECK
	if (device_extension_count > 0)
	{
		std::vector<bool> found_required_device_extension(required_device_extensions.size(), false);

		device_extensions.resize(device_extension_count);
		result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, device_extensions.data());
		SIMUL_VK_ASSERT_RETURN(result);
		#if SIMUL_INTERNAL_CHECKS
		std::cerr<<"Available device extensions: "<<device_extension_count<<std::endl;
		#endif
		for (uint32_t i = 0; i < device_extension_count; i++)
		{
#if SIMUL_INTERNAL_CHECKS
			std::cerr << device_extensions[i].extensionName << std::endl;
#endif
			for(size_t j=0;j<required_device_extensions.size();j++)
			{
				if (strcmp(required_device_extensions[j].c_str(), device_extensions[i].extensionName) == 0)
				{
					ExclusivePushBack(device_extension_names, device_extensions[i].extensionName);
					found_required_device_extension[j] = true;
				}
			}
			
			assert(device_extension_names.size() < 64);
		}

		for (size_t j = 0; j < required_device_extensions.size(); j++)
		{
			if (!found_required_device_extension[j])
			{
				SIMUL_CERR << "Missing the required device extension " << required_device_extensions[j].c_str() << std::endl;
			}
		}
	}
	ERRNO_BREAK

	InitQueueProperties(deviceManagerInternal->gpu,queue_props);

	// Query fine-grained feature support for this device.
	//  If app has specific feature requirements it should check supported features based on this query.
	deviceManagerInternal->gpu.getFeatures(&deviceManagerInternal->gpu_features);
	deviceManagerInternal->gpu.getProperties(&deviceManagerInternal->gpu_props);
	deviceManagerInternal->gpu.getMemoryProperties(&deviceManagerInternal->memory_properties);

	//Taken from UE4 for setting up chains of structures
	void** nextFeatsAddr = nullptr;
	nextFeatsAddr = &deviceManagerInternal->gpu_features2.pNext;
	void** nextPropsAddr = nullptr;
	nextPropsAddr = &deviceManagerInternal->gpu_props2.pNext;

	// Query fine-grained feature and properties support for this device with VK_KHR_get_physical_device_properties2.
#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	if (IsInVector(device_extension_names, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceSamplerYcbcrConversionFeatures;
		nextFeatsAddr = &physicalDeviceSamplerYcbcrConversionFeatures.pNext;
	}
#endif
#if PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	if (IsInVector(device_extension_names, VK_KHR_MULTIVIEW_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceMultiviewFeatures;
		nextFeatsAddr = &physicalDeviceMultiviewFeatures.pNext;
		*nextPropsAddr = &physicalDeviceMultiviewProperties;
		nextPropsAddr = &physicalDeviceMultiviewProperties.pNext;
	}
#endif
	if (IsInVector(device_extension_names, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceShaderFloat16Int8Features;
		nextFeatsAddr = &physicalDeviceShaderFloat16Int8Features.pNext;
	}

	//Execute calls into VK_KHR_get_physical_device_properties2
	if (apiVersion >= VK_API_VERSION_1_1)
	{
		deviceManagerInternal->gpu.getFeatures2(&deviceManagerInternal->gpu_features2);
		deviceManagerInternal->gpu.getProperties2(&deviceManagerInternal->gpu_props2);
	}
	else if (IsInVector(instance_extension_names, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
	{
		vk::DispatchLoaderDynamic d;
		d.vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)deviceManagerInternal->instance.getProcAddr("vkGetPhysicalDeviceFeatures2");
		d.vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)deviceManagerInternal->instance.getProcAddr("vkGetPhysicalDeviceProperties2");
		deviceManagerInternal->gpu.getFeatures2(&deviceManagerInternal->gpu_features2, d);
		deviceManagerInternal->gpu.getProperties2(&deviceManagerInternal->gpu_props2, d);
	}
	
	SIMUL_ASSERT_WARN((bool)physicalDeviceShaderFloat16Int8Features.shaderFloat16, "Vulkan: No 16 bit float support in shaders.");
	SIMUL_ASSERT_WARN((bool)deviceManagerInternal->gpu_features.shaderInt16, "Vulkan: No 16 bit int/uint support in shaders.");

	ERRNO_BREAK

	if(use_debug)
		SetupDebugCallback(
			IsInVector(instance_extension_names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
			IsInVector(instance_extension_names, VK_EXT_DEBUG_REPORT_EXTENSION_NAME),
			IsInVector(instance_extension_names, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)
		);

	#if PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS
	debugUtilsSupported = IsInVector(instance_extension_names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	debugMarkerSupported = IsInVector(instance_extension_names, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	#endif

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

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	auto GetMessageSeverityString = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity)->std::string
	{
		bool separator = false;

		std::string msg_flags;
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		{
			msg_flags += "VERBOSE";
			separator = true;
		}
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "INFO";
			separator = true;
		}
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "WARN";
			separator = true;
		}
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "ERROR";
		}
		return msg_flags;
	};
	auto GetMessageTypeString = [](VkDebugUtilsMessageTypeFlagBitsEXT messageType)->std::string
	{
		bool separator = false;

		std::string msg_flags;
		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		{
			msg_flags += "GEN";
			separator = true;
		}
		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "SPEC";
			separator = true;
		}
		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "PERF";
		}
		return msg_flags;
	};

	std::string messageSeverityStr = GetMessageSeverityString(messageSeverity);
	std::string messageTypeStr = GetMessageTypeString(VkDebugUtilsMessageTypeFlagBitsEXT(messageType));

	std::stringstream errorMessage;
	errorMessage << pCallbackData->pMessageIdName << "(" << messageSeverityStr << " / " << messageTypeStr << "): msgNum: " << pCallbackData->messageIdNumber << " - " << pCallbackData->pMessage;
	std::string errorMessageStr = errorMessage.str();

	std::cerr << errorMessageStr.c_str() << std::endl;

	if((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
		SIMUL_BREAK("Vulkan Error");
	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	objectType,
	uint64_t					object,
	size_t						location,
	int32_t						messageCode,
	const char*					pLayerPrefix,
	const char*					pMessage,
	void*						pUserData)
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
void DeviceManager::SetupDebugCallback(bool debugUtils, bool debugReport, bool debugMarker)
{
	if (debugUtils)
	{
		debugUtilsMessengerCI
			.setPNext(nullptr)
			.setFlags(vk::DebugUtilsMessengerCreateFlagBitsEXT(0))
			.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
								| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
			.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
							| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
							| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
			.setPfnUserCallback(DebugUtilsCallback)
			.setPUserData(this);

		vk::DispatchLoaderDynamic d;
		d.vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)deviceManagerInternal->instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
		debugUtilsMessenger = deviceManagerInternal->instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCI, nullptr, d);
		debugUtilsSupported = true;
	}
	else if(debugReport)
	{
		debugReportCallbackCI
			.setPNext(nullptr)
			.setFlags(vk::DebugReportFlagBitsEXT::eError
						| vk::DebugReportFlagBitsEXT::eWarning
						| vk::DebugReportFlagBitsEXT::ePerformanceWarning)
			.setPfnCallback(DebugReportCallback)
			.setPUserData(this);

		vk::DispatchLoaderDynamic d;
		d.vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)deviceManagerInternal->instance.getProcAddr("vkCreateDebugReportCallbackEXT");
		debugReportCallback = deviceManagerInternal->instance.createDebugReportCallbackEXT(debugReportCallbackCI, nullptr, d);
		debugMarkerSupported = debugMarker;
	}
	else
	{
		return;
	}
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
	
	uint32_t apiVersion = VK_API_VERSION_1_0;
	uint32_t instanceVersion = 0;
	vk::Result result = vk::enumerateInstanceVersion(&instanceVersion);
	if (result == vk::Result::eSuccess && instanceVersion != 0)
		apiVersion = instanceVersion;

	void* deviceCI_pNext = nullptr;
	if (IsInVector(instance_extension_names, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) || apiVersion >= VK_API_VERSION_1_1) //Promoted to Vulkan 1.1
		deviceCI_pNext = &deviceManagerInternal->gpu_features2;

	std::vector<const char*> device_extension_names_cstr;
	device_extension_names_cstr.reserve(device_extension_names.size());
	for (const auto& device_extension_name : device_extension_names)
		device_extension_names_cstr.push_back(device_extension_name.c_str());

	auto deviceInfo = vk::DeviceCreateInfo()
							.setQueueCreateInfoCount((uint32_t)queues.size())
							.setPQueueCreateInfos(queues.data())
							.setEnabledLayerCount(0)
							.setPpEnabledLayerNames(nullptr)
							.setEnabledExtensionCount((uint32_t)device_extension_names_cstr.size())
							.setPpEnabledExtensionNames(device_extension_names_cstr.data())
							.setPNext(deviceCI_pNext)
							.setPEnabledFeatures(deviceCI_pNext ? nullptr : &deviceManagerInternal->gpu_features);
							
	ERRNO_BREAK
	result = deviceManagerInternal->gpu.createDevice(&deviceInfo, nullptr, &deviceManagerInternal->device);
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

crossplatform::GPUInfo DeviceManager::GetGPUInfo()
{
	crossplatform::GPUInfo info;
	info.name = deviceManagerInternal->gpu_props.deviceName.operator std::string();

	static uint64_t deviceLocalMemorySize = 0;
	if (deviceLocalMemorySize == 0)
	{
		const vk::PhysicalDeviceMemoryProperties &memory_properties = deviceManagerInternal->memory_properties;
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
		{
			const vk::MemoryType &memoryType = memory_properties.memoryTypes[i];
			const vk::MemoryPropertyFlags &propertyFlags = memoryType.propertyFlags;

			if ((propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal)
			{
				const uint32_t &heapIndex = memoryType.heapIndex;
				if (heapIndex < memory_properties.memoryHeapCount)
				{
					const vk::MemoryHeap &memoryHeap = memory_properties.memoryHeaps[heapIndex];
					deviceLocalMemorySize = (memoryHeap.size / 1024 / 1024);
					break;
				}
			}
		}
	}
	info.memorySize = deviceLocalMemorySize;

	return info;
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