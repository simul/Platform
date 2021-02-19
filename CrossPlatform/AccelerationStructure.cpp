#include "AccelerationStructure.h"
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

////////////////////////////////////
//BottomLevelAccelerationStructure//
////////////////////////////////////

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:BaseAccelerationStructure(r)
{
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
}

void BottomLevelAccelerationStructure::SetMesh(crossplatform::Mesh* mesh)
{
	this->mesh = mesh;
	geometryType = GeometryType::TRIANGLE_MESH;
}

void BottomLevelAccelerationStructure::SetAABB(crossplatform::StructuredBuffer<Raytracing_AABB>* aabbBuffer)
{
	this->aabbBuffer = aabbBuffer;
	geometryType = GeometryType::AABB;
}

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