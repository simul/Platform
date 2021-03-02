#include "BaseAccelerationStructure.h"
using namespace simul;
using namespace crossplatform;

/////////////////////////////
//BaseAccelerationStructure//
/////////////////////////////

BaseAccelerationStructure::BaseAccelerationStructure(crossplatform::RenderPlatform* r)
{
	renderPlatform = r;
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


