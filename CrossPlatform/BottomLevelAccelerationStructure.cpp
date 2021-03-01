#include "BottomLevelAccelerationStructure.h"

using namespace simul;
using namespace crossplatform;

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