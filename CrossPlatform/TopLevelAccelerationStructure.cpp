#include "TopLevelAccelerationStructure.h"

#include <atomic>

using namespace simul;
using namespace crossplatform;

/////////////////////////////////
//TopLevelAccelerationStructure//
/////////////////////////////////

static std::atomic<int> IDCount(0);

TopLevelAccelerationStructure::TopLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:BaseAccelerationStructure(r)
{
	ID = IDCount;
	IDCount++;
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
}

void TopLevelAccelerationStructure::SetBottomLevelAccelerationStructuresAndTransforms(const BottomLevelAccelerationStructuresAndTransforms& BLASandTransforms)
{
	this->BLASandTransforms = BLASandTransforms;
	initialized = false;
}

BottomLevelAccelerationStructuresAndTransforms* TopLevelAccelerationStructure::GetBottomLevelAccelerationStructuresAndTransforms()
{
	return &BLASandTransforms;
}

int TopLevelAccelerationStructure::GetID()
{
	return ID;
}