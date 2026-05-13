#include "DeviceManager.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Vulkan/DisplaySurface.h"
#include "Platform/Vulkan/RenderPlatform.h"

#include <iomanip>
#include <regex>
#include <sstream>
#include <stdint.h> // for uintptr_t

#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <linux/input.h>
#endif

#include <vulkan/vulkan.hpp>

namespace platform
{
	namespace vulkan
	{
		DeviceManager *deviceManager = nullptr;
	}
}

#ifndef _MSC_VER
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace platform;
using namespace vulkan;

using namespace std;

#ifdef _MSC_VER
#pragma comment(lib, "vulkan-1")
#endif

#define SIMUL_VK_ASSERT_RETURN(val)  \
	if (val != vk::Result::eSuccess) \
		return;

// VK_EXT_debug_report

void RewriteVulkanMessage(std::string& str)
{
	// If we have a number followed by a bracket at the start
	std::smatch m;
	std::regex re("0x([0-9a-f]+)"); // e.g. 0x1c4e6c00000002c7
	std::string out;
	while (std::regex_search(str, m, re))
	{
		string hex_addr = m[1].str();
		std::stringstream sstr;
		uint64_t num;
		sstr << std::hex << hex_addr.c_str();
		sstr >> num;

		out += m.prefix();
		out += m.str();
		auto f = RenderPlatform::ResourceMap.find(num);
		if (f != RenderPlatform::ResourceMap.end())
		{
			out += "(";
			out += f->second + ")";
		}
		str = m.suffix();
	}
	out += str;
	str = out;
}

vk::Bool32 VKAPI_PTR DebugReportCallback(
	vk::DebugReportFlagsEXT flags,
	vk::DebugReportObjectTypeEXT objectType,
	uint64_t object,
	size_t location,
	int32_t messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	if (pLayerPrefix)
		std::cerr << pLayerPrefix << " layer: ";
	if (flags & vk::DebugReportFlagBitsEXT::eError)
		std::cerr << " Error: ";
	if (flags & vk::DebugReportFlagBitsEXT::eWarning)
		std::cerr << " Warning: ";
	if (pMessage)
	{
		std::string str = pMessage;
		RewriteVulkanMessage(str);
		std::cerr << str.c_str() << std::endl;
	}
	if (flags & vk::DebugReportFlagBitsEXT::eError)
		SIMUL_BREAK("Vulkan Error");
	return VK_FALSE;
}

// VK_EXT_debug_utils

vk::Bool32 VKAPI_PTR DebugUtilsCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT messageType,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	auto GetMessageSeverityString = [](vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity) -> std::string
	{
		bool separator = false;

		std::string msg_flags;
		if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
		{
			msg_flags += "VERBOSE";
			separator = true;
		}
		if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "INFO";
			separator = true;
		}
		if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "WARN";
			separator = true;
		}
		if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "ERROR";
		}
		return msg_flags;
	};
	auto GetMessageTypeString = [](vk::DebugUtilsMessageTypeFlagBitsEXT messageType) -> std::string
	{
		bool separator = false;

		std::string msg_flags;
		if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
		{
			msg_flags += "GEN";
			separator = true;
		}
		if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "SPEC";
			separator = true;
		}
		if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
		{
			if (separator)
				msg_flags += ",";
			msg_flags += "PERF";
		}
		return msg_flags;
	};

	std::string messageSeverityStr = GetMessageSeverityString(messageSeverity);
	std::string messageTypeStr = GetMessageTypeString((vk::DebugUtilsMessageTypeFlagBitsEXT)(VkDebugUtilsMessageTypeFlagsEXT)messageType);

	std::stringstream errorMessage;
	errorMessage << pCallbackData->pMessageIdName << "(" << messageSeverityStr << " / " << messageTypeStr << "): msgNum: " << pCallbackData->messageIdNumber << " - " << pCallbackData->pMessage;
	std::string errorMessageStr = errorMessage.str();

	std::cerr << errorMessageStr.c_str() << std::endl;

	if ((messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError))
		SIMUL_BREAK("Vulkan Error");
	return VK_FALSE;
}

static bool IsInVector(const std::vector<std::string> &container, const std::string &value)
{
	return std::find(container.begin(), container.end(), value) != container.end();
}
static void ExclusivePushBack(std::vector<std::string> &container, const std::string &value)
{
	if (!IsInVector(container, value))
		container.push_back(value);
}

static bool IsInVector(const std::vector<const char *> &container, const char *value)
{
	for (const char *element : container)
	{
		if (strcmp(element, value) == 0)
			return true;
	}
	return false;
}
static void ExclusivePushBack(std::vector<const char *> &container, const char *value)
{
	if (!IsInVector(container, value))
		container.push_back(value);
}

static bool CheckLayers(uint32_t check_count, const char* const* const check_names, uint32_t layer_count, vk::LayerProperties* layers)
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

class platform::vulkan::DeviceManagerInternal
{
public:
	vk::Instance instance;
	vk::PhysicalDevice gpu;
	vk::Device device;
	vk::PhysicalDeviceFeatures gpuFeatures;
	vk::PhysicalDeviceProperties gpuProps;
	vk::PhysicalDeviceMemoryProperties gpuMemoryProperties;
	vk::PhysicalDeviceFeatures2 gpuFeatures2;
	vk::PhysicalDeviceProperties2 gpuProps2;

	DeviceManagerInternal()
		: instance(vk::Instance()),
		  gpu(vk::PhysicalDevice()),
		  device(vk::Device()),
		  gpuFeatures(vk::PhysicalDeviceFeatures()),
		  gpuProps(vk::PhysicalDeviceProperties()),
		  gpuMemoryProperties(vk::PhysicalDeviceMemoryProperties()),
		  gpuFeatures2(vk::PhysicalDeviceFeatures2()),
		  gpuProps2(vk::PhysicalDeviceProperties2()) 
	{};
};

DeviceManager::DeviceManager()
	: deviceInitialized(false)
{
	deviceManager = this;
	deviceManagerInternal = std::make_unique<DeviceManagerInternal>();
}

DeviceManager::~DeviceManager()
{
	if (debugReportCallback)
	{
		DispatchLoaderDynamic d;
		d.vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)deviceManagerInternal->instance.getProcAddr("vkDestroyDebugReportCallbackEXT");
		deviceManagerInternal->instance.destroyDebugReportCallbackEXT(debugReportCallback, nullptr, d);
	}
	if (debugUtilsMessenger)
	{
		DispatchLoaderDynamic d;
		d.vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)deviceManagerInternal->instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
		deviceManagerInternal->instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger, nullptr, d);
	}

	deviceManagerInternal = nullptr;
	deviceManager = nullptr;
}

bool DeviceManager::IsActive() const
{
	return deviceInitialized;
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
	Initialize(use_debug, instrument, default_driver, std::vector<std::string>(), std::vector<std::string>());
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver, std::vector<std::string> required_device_extensions, std::vector<std::string> required_instance_extensions)
{
	if (deviceInitialized)
		return;

	// Instance
	CreateInstance(use_debug, required_instance_extensions);

	// Debug Callbacks
	{
		if (use_debug)
		{
			SetupDebugCallback(
				IsInVector(instanceExtensionNames, VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
				IsInVector(instanceExtensionNames, VK_EXT_DEBUG_REPORT_EXTENSION_NAME),
				IsInVector(instanceExtensionNames, VK_EXT_DEBUG_MARKER_EXTENSION_NAME));
		}

#if PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS
		debugUtilsSupported = IsInVector(instanceExtensionNames, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		debugMarkerSupported = IsInVector(instanceExtensionNames, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif
		ERRNO_BREAK
	}

	// Device and Physical Device
	CreateDevice(use_debug, required_device_extensions);
	ERRNO_BREAK
}

void DeviceManager::CreateInstance(bool use_debug, std::vector<std::string> required_instance_extensions)
{
	ERRNO_BREAK
	apiVersion = VK_API_VERSION_1_0;
	// Query the API version in order to use the correct vulkan functionality
	uint32_t instanceVersion = 0;
	vk::Result result = vk::enumerateInstanceVersion(&instanceVersion);

	// Check what is returned
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

	// Look for instance layers
	char const* const instanceValidationLayersMain[] = {"VK_LAYER_KHRONOS_validation"};
	char const* const instanceValidationLayersAlt1[] = {"VK_LAYER_LUNARG_standard_validation"};
	char const* const instanceValidationLayersAlt2[] = {"VK_LAYER_GOOGLE_threading",
														"VK_LAYER_LUNARG_parameter_validation",
														"VK_LAYER_LUNARG_object_tracker",
														"VK_LAYER_LUNARG_core_validation",
														"VK_LAYER_GOOGLE_unique_objects"};
	if (use_debug)
	{
		uint32_t instanceLayerCount = 0;
		vk::Result result = vk::enumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		SIMUL_VK_ASSERT_RETURN(result);

		if (instanceLayerCount > 0)
		{
			instanceLayers.resize(instanceLayerCount);
			result = vk::enumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());
			SIMUL_VK_ASSERT_RETURN(result);

			if (instanceLayerCount)
			{
				SIMUL_COUT << "Vulkan Instance Layers: " << instanceLayerCount << std::endl;
			}
			for (uint32_t j = 0; j < instanceLayerCount; j++)
			{
				SIMUL_COUT << "\t" << instanceLayers[j].layerName << std::endl;
			}

			vk::Bool32 validationSet = VK_FALSE;
			vk::Bool32 validationFound = VK_FALSE;

			// Main
			validationFound = CheckLayers(std::size(instanceValidationLayersMain), instanceValidationLayersMain, instanceLayerCount, instanceLayers.data());
			if (validationFound && !validationSet)
			{
				for (const char* instance_layer : instanceValidationLayersMain)
					instanceLayerNames.push_back(instance_layer);
				validationSet = VK_TRUE;
			}

			// Alt1
			validationFound = CheckLayers(std::size(instanceValidationLayersAlt1), instanceValidationLayersAlt1, instanceLayerCount, instanceLayers.data());
			if (validationFound && !validationSet)
			{
				for (const char* instance_layer : instanceValidationLayersAlt1)
					instanceLayerNames.push_back(instance_layer);
				validationSet = VK_TRUE;
			}

			// Alt2
			validationFound = CheckLayers(std::size(instanceValidationLayersAlt2), instanceValidationLayersAlt2, instanceLayerCount, instanceLayers.data());
			if (validationFound && !validationSet)
			{
				for (const char* instance_layer : instanceValidationLayersAlt2)
					instanceLayerNames.push_back(instance_layer);
				validationSet = VK_TRUE;
			}

			// Error
			if (!validationFound && !validationSet)
			{
				SIMUL_COUT << "vkEnumerateInstanceLayerProperties failed to find required validation layer.\n";
			}
		}
	}
	ERRNO_BREAK

	// Look for instance extensions

	// Platform Surface Extension
	const char *platformSurfaceExt = nullptr;
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

#ifdef __ANDROID__
#define PLATFORM_VULKAN_USE_RENDERDOC_META_ANDROID (PLATFORM_VULKAN_USE_RENDERDOC_META == 0)
#else
#define PLATFORM_VULKAN_USE_RENDERDOC_META_ANDROID 0
#endif

#if PLATFORM_VULKAN_USE_RENDERDOC_META_ANDROID || PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif
	ExclusivePushBack(required_instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME);
	ExclusivePushBack(required_instance_extensions, platformSurfaceExt);

	uint32_t instanceExtensionCount = 0;
	result = vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	if (result != vk::Result::eSuccess)
		return;

	if (instanceExtensionCount > 0)
	{
		std::vector<bool> foundRequiredInstanceExtension(required_instance_extensions.size(), false);

		instanceExtensions.resize(instanceExtensionCount);
		result = vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());
		SIMUL_VK_ASSERT_RETURN(result);

		SIMUL_COUT << "Vulkan Instance Extensions: " << instanceExtensionCount << std::endl;

		for (uint32_t i = 0; i < instanceExtensionCount; i++)
		{
			SIMUL_COUT << "\t" << instanceExtensions[i].extensionName << std::endl;
			for (size_t j = 0; j < required_instance_extensions.size(); j++)
			{
				if (strcmp(required_instance_extensions[j].c_str(), instanceExtensions[i].extensionName) == 0)
				{
					ExclusivePushBack(instanceExtensionNames, instanceExtensions[i].extensionName);
					foundRequiredInstanceExtension[j] = true;
					break;
				}
			}
		}
		for (size_t j = 0; j < required_instance_extensions.size(); j++)
		{
			if (!foundRequiredInstanceExtension[j])
			{
				SIMUL_CERR << "Missing the required instance extension " << required_instance_extensions[j].c_str() << std::endl;
			}
		}
	}
	errno = 0;
	ERRNO_BREAK

	std::vector<const char*> instanceLayerNamesCstr;
	instanceLayerNamesCstr.reserve(instanceLayerNames.size());
	for (const auto& instanceLayerName : instanceLayerNames)
		instanceLayerNamesCstr.push_back(instanceLayerName.c_str());

	std::vector<const char*> instanceExtensionNamesCstr;
	instanceExtensionNamesCstr.reserve(instanceExtensionNames.size());
	for (const auto& instanceExtensionName : instanceExtensionNames)
		instanceExtensionNamesCstr.push_back(instanceExtensionName.c_str());

	void *instanceCI_pNext = nullptr;
	if (IsInVector(instanceExtensionNames, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
	{
		// Add in vk::DebugUtilsMessengerCreateInfo to vk::InstanceCreateInfo::pNext for error callbacks on vk::createInstance!
		debugUtilsMessengerCI
			.setPNext(nullptr)
			.setFlags(vk::DebugUtilsMessengerCreateFlagBitsEXT(0))
			.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
			.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
			.setPfnUserCallback(DebugUtilsCallback)
			.setPUserData(this);
		instanceCI_pNext = &debugUtilsMessengerCI;
	}

	vk::ApplicationInfo applicationInfo = vk::ApplicationInfo()
		.setPApplicationName("Platform")
		.setApplicationVersion(0)
		.setPEngineName("Platform")
		.setEngineVersion(0)
		.setApiVersion(apiVersion);
	vk::InstanceCreateInfo instanceCI = vk::InstanceCreateInfo()
		.setPNext(instanceCI_pNext)
		.setPApplicationInfo(&applicationInfo)
		.setEnabledLayerCount((uint32_t)instanceLayerNamesCstr.size())
		.setPpEnabledLayerNames(instanceLayerNamesCstr.data())
		.setEnabledExtensionCount((uint32_t)instanceExtensionNamesCstr.size())
		.setPpEnabledExtensionNames(instanceExtensionNamesCstr.data());
	ERRNO_BREAK
	result = vk::createInstance(&instanceCI, nullptr, &deviceManagerInternal->instance);

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	if (result == vk::Result::eErrorExtensionNotPresent)
	{
		// Android Only: This is pure crazy. vk::createInstance can return vk::Result::eErrorExtensionNotPresent,
		// despite querying and loading all the layers and extensions. Second call with the same data return
		// successfully with an vk::Instance.
		// https://developer.android.com/ndk/guides/graphics/validation-layer
		result = vk::createInstance(&instanceCI, nullptr, &deviceManagerInternal->instance);
	}
#endif

	// Vulkan sets errno without warning or error.
	errno = 0;

	ERRNO_BREAK
	if (result == vk::Result::eErrorIncompatibleDriver)
	{
		SIMUL_BREAK("vkCreateInstance failed.\n"
					"Cannot find a compatible Vulkan installable client driver (ICD).\n");
	}
	else if (result == vk::Result::eErrorExtensionNotPresent)
	{
		SIMUL_BREAK("vkCreateInstance failed.\n"
					"Cannot find a specified extension.\n"
					"Make sure your layers path is set appropriately.\n");
		for (uint32_t i = 0; i < (uint32_t)instanceExtensionNames.size() + 1; i++)
		{
			instanceCI.setEnabledExtensionCount(i);
			result = vk::createInstance(&instanceCI, nullptr, &deviceManagerInternal->instance);
			if (result == vk::Result::eErrorExtensionNotPresent)
			{
				SIMUL_CERR << "Fails on extension: " << instanceExtensionNames[i] << std::endl;
			}
		}
	}
	else if (result != vk::Result::eSuccess)
	{
		SIMUL_BREAK("vkCreateInstance failed.\n"
					"Do you have a compatible Vulkan installable client driver (ICD) installed?\n");
	}
	ERRNO_BREAK
}

void DeviceManager::CreateDevice(bool use_debug, std::vector<std::string> required_device_extensions)
{
	vk::Result result;

	// Physical Device
	// Make initial call to query gpu_count, then second call for gpu info
	uint32_t gpuCount;
	result = deviceManagerInternal->instance.enumeratePhysicalDevices(&gpuCount, nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	errno = 0;

	std::vector<vk::PhysicalDevice> physicalDevices(gpuCount);
	result = deviceManagerInternal->instance.enumeratePhysicalDevices(&gpuCount, physicalDevices.data());
	SIMUL_VK_ASSERT_RETURN(result);
	deviceManagerInternal->gpu = physicalDevices[0]; // TODO: Better way of selecting the GPU.

	if (gpuCount == 0)
	{
		SIMUL_BREAK("vkEnumeratePhysicalDevices reported zero accessible devices.\n"
					"Do you have a compatible Vulkan installable client driver (ICD) installed?\n");
	}
	ERRNO_BREAK

	// Look for device layers
	const char* const deviceValidationLayersMain[] = {"VK_LAYER_KHRONOS_validation"};
	if (use_debug)
	{
		uint32_t deviceLayerCount = 0;
		vk::Result result = deviceManagerInternal->gpu.enumerateDeviceLayerProperties(&deviceLayerCount, nullptr);
		SIMUL_VK_ASSERT_RETURN(result);

		if (deviceLayerCount > 0)
		{
			deviceLayers.resize(deviceLayerCount);
			result = deviceManagerInternal->gpu.enumerateDeviceLayerProperties(&deviceLayerCount, deviceLayers.data());
			SIMUL_VK_ASSERT_RETURN(result);
			if (deviceLayerCount)
			{
				SIMUL_COUT << "Vulkan Device Layers: " << deviceLayerCount << std::endl;
			}
			for (uint32_t j = 0; j < deviceLayerCount; j++)
			{
				SIMUL_COUT << "\t" << deviceLayers[j].layerName << std::endl;
			}

			vk::Bool32 validationSet = VK_FALSE;
			vk::Bool32 validationFound = VK_FALSE;
			
			// Main
			validationFound = CheckLayers(std::size(deviceValidationLayersMain), deviceValidationLayersMain, deviceLayerCount, instanceLayers.data());
			if (validationFound && !validationSet)
			{
				for (const char* instance_layer : deviceValidationLayersMain)
					instanceLayerNames.push_back(instance_layer);
				validationSet = VK_TRUE;
			}

			// Error
			if (!validationFound && !validationSet)
			{
				SIMUL_COUT << "vkEnumerateDeviceLayerProperties failed to find required validation layer.\n";
			}
		}
	}

	// Look for device extensions
	ExclusivePushBack(required_device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	ExclusivePushBack(required_device_extensions, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
#endif
#if PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	ExclusivePushBack(required_device_extensions, VK_KHR_MULTIVIEW_EXTENSION_NAME);
#endif
#if PLATFORM_SUPPORT_VULKAN_MESH_SHADER
	ExclusivePushBack(required_device_extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME);
#endif
	ExclusivePushBack(required_device_extensions, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

	uint32_t deviceExtensionCount = 0;
	result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &deviceExtensionCount, nullptr);
	SIMUL_VK_ASSERT_RETURN(result);
	ERRNO_CHECK
	if (deviceExtensionCount > 0)
	{
		std::vector<bool> foundRequiredDeviceExtension(required_device_extensions.size(), false);

		deviceExtensions.resize(deviceExtensionCount);
		result = deviceManagerInternal->gpu.enumerateDeviceExtensionProperties(nullptr, &deviceExtensionCount, deviceExtensions.data());
		SIMUL_VK_ASSERT_RETURN(result);

		SIMUL_COUT << "Vulkan Device Extensions: " << deviceExtensionCount << std::endl;
		for (uint32_t i = 0; i < deviceExtensionCount; i++)
		{
			SIMUL_COUT << "\t" << deviceExtensions[i].extensionName << std::endl;
			for (size_t j = 0; j < required_device_extensions.size(); j++)
			{
				if (strcmp(required_device_extensions[j].c_str(), deviceExtensions[i].extensionName) == 0)
				{
					ExclusivePushBack(deviceExtensionNames, deviceExtensions[i].extensionName);
					foundRequiredDeviceExtension[j] = true;
				}
			}
		}

		for (size_t j = 0; j < required_device_extensions.size(); j++)
		{
			if (!foundRequiredDeviceExtension[j])
			{
				SIMUL_CERR << "Missing the required device extension " << required_device_extensions[j].c_str() << std::endl;
			}
		}
	}
	ERRNO_BREAK

	// Physical Device Feature and Properties
	// Query fine-grained feature support for this device.
	// If app has specific feature requirements it should check supported features based on this query.
	deviceManagerInternal->gpu.getFeatures(&deviceManagerInternal->gpuFeatures);
	deviceManagerInternal->gpu.getProperties(&deviceManagerInternal->gpuProps);
	deviceManagerInternal->gpu.getMemoryProperties(&deviceManagerInternal->gpuMemoryProperties);

	// Taken from UE4 for setting up chains of structures
	void** nextFeatsAddr = nullptr;
	nextFeatsAddr = &deviceManagerInternal->gpuFeatures2.pNext;
	void** nextPropsAddr = nullptr;
	nextPropsAddr = &deviceManagerInternal->gpuProps2.pNext;

	// Query fine-grained feature and properties support for this device with VK_KHR_get_physical_device_properties2.
#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	if (IsInVector(deviceExtensionNames, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceSamplerYcbcrConversionFeatures;
		nextFeatsAddr = &physicalDeviceSamplerYcbcrConversionFeatures.pNext;
	}
#endif
#if PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	if (IsInVector(deviceExtensionNames, VK_KHR_MULTIVIEW_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceMultiviewFeatures;
		nextFeatsAddr = &physicalDeviceMultiviewFeatures.pNext;
		*nextPropsAddr = &physicalDeviceMultiviewProperties;
		nextPropsAddr = &physicalDeviceMultiviewProperties.pNext;
	}
#endif
#if PLATFORM_SUPPORT_VULKAN_MESH_SHADER
	if (IsInVector(deviceExtensionNames, VK_EXT_MESH_SHADER_EXTENSION_NAME))
	{
		*nextFeatsAddr = &physicalDeviceMeshShaderFeatures;
		nextFeatsAddr = &physicalDeviceMeshShaderFeatures.pNext;
		*nextPropsAddr = &physicalDeviceMeshShaderProperties;
		nextPropsAddr = &physicalDeviceMeshShaderProperties.pNext;
	}
#endif
	if (apiVersion >= VK_API_VERSION_1_4)
	{
		*nextFeatsAddr = &physicalDeviceVulkan14Features;
		nextFeatsAddr = &physicalDeviceVulkan14Features.pNext;
		*nextPropsAddr = &physicalDeviceVulkan14Properties;
		nextPropsAddr = &physicalDeviceVulkan14Properties.pNext;
	}
	if (apiVersion >= VK_API_VERSION_1_3)
	{
		*nextFeatsAddr = &physicalDeviceVulkan13Features;
		nextFeatsAddr = &physicalDeviceVulkan13Features.pNext;
		*nextPropsAddr = &physicalDeviceVulkan13Properties;
		nextPropsAddr = &physicalDeviceVulkan13Properties.pNext;
	}
	if (apiVersion >= VK_API_VERSION_1_2)
	{
		*nextFeatsAddr = &physicalDeviceVulkan12Features;
		nextFeatsAddr = &physicalDeviceVulkan12Features.pNext;
		*nextPropsAddr = &physicalDeviceVulkan12Properties;
		nextPropsAddr = &physicalDeviceVulkan12Properties.pNext;
	}
	if (apiVersion >= VK_API_VERSION_1_1)
	{
		*nextFeatsAddr = &physicalDeviceVulkan11Features;
		nextFeatsAddr = &physicalDeviceVulkan11Features.pNext;
		*nextPropsAddr = &physicalDeviceVulkan11Properties;
		nextPropsAddr = &physicalDeviceVulkan11Properties.pNext;
	}
	

	// Execute calls into VK_KHR_get_physical_device_properties2
	if (apiVersion >= VK_API_VERSION_1_1)
	{
		deviceManagerInternal->gpu.getFeatures2(&deviceManagerInternal->gpuFeatures2);
		deviceManagerInternal->gpu.getProperties2(&deviceManagerInternal->gpuProps2);
	}
	else if (IsInVector(instanceExtensionNames, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
	{
		DispatchLoaderDynamic d;
		d.vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)deviceManagerInternal->instance.getProcAddr("vkGetPhysicalDeviceFeatures2");
		d.vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)deviceManagerInternal->instance.getProcAddr("vkGetPhysicalDeviceProperties2");
		deviceManagerInternal->gpu.getFeatures2(&deviceManagerInternal->gpuFeatures2, d);
		deviceManagerInternal->gpu.getProperties2(&deviceManagerInternal->gpuProps2, d);
	}

	SIMUL_ASSERT_WARN((bool)physicalDeviceVulkan12Features.shaderFloat16, "Vulkan: No 16 bit float support in shaders.");
	SIMUL_ASSERT_WARN((bool)deviceManagerInternal->gpuFeatures.shaderInt16, "Vulkan: No 16 bit int/uint support in shaders.");

#if PLATFORM_SUPPORT_VULKAN_MESH_SHADER
#if !PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	physicalDeviceMeshShaderFeatures.multiviewMeshShader = false;
#endif
	physicalDeviceMeshShaderFeatures.primitiveFragmentShadingRateMeshShader = false;
#endif

	ERRNO_BREAK

	float const priorities[1] = {0.0};
	std::vector<vk::DeviceQueueCreateInfo> queues;
	queues.resize(RenderPlatform::GetQueueProperties(GetGPU()).size());
	for (int i = 0; i < queues.size(); i++)
	{
		queues[i].setQueueFamilyIndex(i);
		queues[i].setQueueCount(1);
		queues[i].setPQueuePriorities(priorities);
	}

	// Query Physical Device Features for compatibility
	bool gpuFeatureChecks = true;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	gpuFeatureChecks = false;
#endif
	ERRNO_BREAK
	if (gpuFeatureChecks)
	{
		if (!deviceManagerInternal->gpuFeatures.vertexPipelineStoresAndAtomics)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"vertexPipelineStoresAndAtomics\". Unable to proceed.\n");
		if (!deviceManagerInternal->gpuFeatures.imageCubeArray)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"imageCubeArray\". Unable to proceed.\n");
		if (!deviceManagerInternal->gpuFeatures.dualSrcBlend)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"dualSrcBlend\". Unable to proceed.\n");
		if (!deviceManagerInternal->gpuFeatures.samplerAnisotropy)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"samplerAnisotropy\". Unable to proceed.\n");
		if (!deviceManagerInternal->gpuFeatures.fragmentStoresAndAtomics)
			SIMUL_BREAK("Simul trueSKY requires the VkPhysicalDeviceFeature: \"fragmentStoresAndAtomics\". Unable to proceed.\n");
	}
	ERRNO_BREAK

	void* deviceCI_pNext = nullptr;
	if (IsInVector(instanceExtensionNames, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) || apiVersion >= VK_API_VERSION_1_1) // Promoted to Vulkan 1.1
		deviceCI_pNext = &deviceManagerInternal->gpuFeatures2;

	std::vector<const char *> deviceExtensionNamesCstr;
	deviceExtensionNamesCstr.reserve(instanceExtensionNames.size());
	for (const auto& deviceExtensionName : deviceExtensionNames)
		deviceExtensionNamesCstr.push_back(deviceExtensionName.c_str());

	vk::DeviceCreateInfo deviceCI = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount((uint32_t)queues.size())
		.setPQueueCreateInfos(queues.data())
		//.setEnabledLayerCount(0)				// Depracted and broadly ignore by the Vulkan spec.
		//.setPpEnabledLayerNames(nullptr)		// Depracted and broadly ignore by the Vulkan spec.
		.setEnabledExtensionCount((uint32_t)deviceExtensionNamesCstr.size())
		.setPpEnabledExtensionNames(deviceExtensionNamesCstr.data())
		.setPNext(deviceCI_pNext)
		.setPEnabledFeatures(deviceCI_pNext ? nullptr : &deviceManagerInternal->gpuFeatures);

	ERRNO_BREAK
	result = deviceManagerInternal->gpu.createDevice(&deviceCI, nullptr, &deviceManagerInternal->device);
	// For unknown reasons, even when successful, Vulkan createDevice sets errno==2: No such file or directory here.
	// So we reset it to prevent spurious error detection.
	errno = 0;
	deviceInitialized = result == vk::Result::eSuccess;
	SIMUL_ASSERT(deviceInitialized);
	ERRNO_BREAK
}

void DeviceManager::SetupDebugCallback(bool debugUtils, bool debugReport, bool debugMarker)
{
	if (debugUtils)
	{
		debugUtilsMessengerCI
			.setPNext(nullptr)
			.setFlags(vk::DebugUtilsMessengerCreateFlagBitsEXT(0))
			.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
			.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
			.setPfnUserCallback(DebugUtilsCallback)
			.setPUserData(this);

		DispatchLoaderDynamic d;
		d.vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)deviceManagerInternal->instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
		debugUtilsMessenger = deviceManagerInternal->instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCI, nullptr, d);
		debugUtilsSupported = true;
	}
	else if (debugReport)
	{
		debugReportCallbackCI
			.setPNext(nullptr)
			.setFlags(vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning)
			.setPfnCallback(DebugReportCallback)
			.setPUserData(this);

		DispatchLoaderDynamic d;
		d.vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)deviceManagerInternal->instance.getProcAddr("vkCreateDebugReportCallbackEXT");
		debugReportCallback = deviceManagerInternal->instance.createDebugReportCallbackEXT(debugReportCallbackCI, nullptr, d);
		debugMarkerSupported = debugMarker;
	}
	else
	{
		return;
	}
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

void DeviceManager::Shutdown()
{
}

void *DeviceManager::GetDevice()
{
	static void* ptr[3];
	ptr[0] = (void*)&deviceManagerInternal->device;
	ptr[1] = (void*)&deviceManagerInternal->instance;
	ptr[2] = (void*)&deviceManagerInternal->gpu;
	return (void*)ptr;
}

void *DeviceManager::GetDeviceContext()
{
	return (void*)&deviceManagerInternal->instance;
}

int DeviceManager::GetNumOutputs()
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
	info.name = deviceManagerInternal->gpuProps.deviceName.operator std::string();

	static uint64_t deviceLocalMemorySize = 0;
	if (deviceLocalMemorySize == 0)
	{
		const vk::PhysicalDeviceMemoryProperties &memory_properties = deviceManagerInternal->gpuMemoryProperties;
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