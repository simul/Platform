#include "Mesh.h"

using namespace simul;
using namespace dx12;

Mesh::Mesh()
	:vertexBuffer(NULL)
	,indexBuffer(NULL)
	,inputLayout(NULL)
{
}

Mesh::~Mesh()
{
	InvalidateDeviceObjects();
}
void Mesh::InvalidateDeviceObjects()
{
	releaseBuffers();
}


bool Mesh::Initialize(crossplatform::RenderPlatform *r,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount,const unsigned int *lIndices)
{
	return true;
}

void Mesh::releaseBuffers()
{
}

void Mesh::GetVertices(void *target, void *indices)
{
}


void Mesh::BeginDraw(crossplatform::DeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const
{
}

// Draw all the faces with specific material with given shading mode.
void Mesh::Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const
{
}

// Unbind buffers, reset vertex arrays, turn off lighting and texture.
void Mesh::EndDraw(crossplatform::DeviceContext &deviceContext) const
{
}

void Mesh::apply(crossplatform::DeviceContext &deviceContext,unsigned instanceStride,crossplatform::Buffer *instanceBuffer)
{	
}

void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}