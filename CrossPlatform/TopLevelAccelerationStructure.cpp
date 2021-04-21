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

void TopLevelAccelerationStructure::SetInstanceDescs(const InstanceDescs& instanceDescs)
{
	this->_instanceDescs = instanceDescs;
	initialized = false;
}

InstanceDescs* TopLevelAccelerationStructure::GetInstanceDescs()
{
	return &_instanceDescs;
}