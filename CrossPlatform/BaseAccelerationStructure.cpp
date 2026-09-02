#include "BaseAccelerationStructure.h"
using namespace platform;
using namespace crossplatform;

/////////////////////////////
//BaseAccelerationStructure//
/////////////////////////////

BaseAccelerationStructure::BaseAccelerationStructure(crossplatform::RenderPlatform* r, const std::string& name)
{
	renderPlatform = r;
	this->name = name;
}

BaseAccelerationStructure::~BaseAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void BaseAccelerationStructure::RestoreDeviceObjects()
{
}

void BaseAccelerationStructure::InvalidateDeviceObjects()
{
}

void BaseAccelerationStructure::BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext)
{
}


