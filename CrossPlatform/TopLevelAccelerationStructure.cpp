#include "TopLevelAccelerationStructure.h"

using namespace simul;
using namespace crossplatform;

/////////////////////////////////
//TopLevelAccelerationStructure//
/////////////////////////////////

TopLevelAccelerationStructure::TopLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:BaseAccelerationStructure(r)
{
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
}

void TopLevelAccelerationStructure::SetBottomLevelAccelerationStructuresAndTransforms(const BottomLevelAccelerationStructuresAndTransforms& BLASandTransforms)
{
	this->BLASandTransforms = BLASandTransforms;
}