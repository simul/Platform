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

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace vulkan;

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
    // Source:
     SIMUL_BREAK("");
}


DeviceManager::DeviceManager()
	:renderPlatformVulkan(NULL)
{
	if(!renderPlatformVulkan)
		renderPlatformVulkan		=new vulkan::RenderPlatform;
	renderPlatformVulkan->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED|crossplatform::TRY_AGAIN_ON_FAIL|crossplatform::BREAK_ON_FAIL);
//	simul::crossplatform::Profiler::GetGlobalProfiler().Initialize(NULL);

}

void DeviceManager::InvalidateDeviceObjects()
{
	int err=errno;
	std::cout<<"Errno "<<err<<std::endl;
	errno=0;
//	simul::vulkan::Profiler::GetGlobalProfiler().Uninitialize();
}

DeviceManager::~DeviceManager()
{
	InvalidateDeviceObjects();
	delete renderPlatformVulkan;
}

static void GlfwErrorCallback(int errcode, const char* info)
{
    SIMUL_CERR << " "<<errcode<<": " << info << std::endl;
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
	InitDebugging();
}

void DeviceManager::InitDebugging()
{
    
}

void	DeviceManager::Shutdown() 
{

}

void*	DeviceManager::GetDevice() 
{
	return (void*)    0;
}

void*	DeviceManager::GetDeviceContext() 
{
	return (void*)    0;
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

void DeviceManager::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
}