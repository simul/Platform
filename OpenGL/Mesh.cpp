#include "Mesh.h"

using namespace simul;
using namespace opengl;

Mesh::Mesh(crossplatform::RenderPlatform*r) :crossplatform::Mesh(r)
{
}

Mesh::~Mesh()
{
}

bool Mesh::Initialize(crossplatform::RenderPlatform *,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount,const unsigned int *lIndices)
{
    return true;
}

bool Mesh::Initialize(const std::vector<vec3> &vertices,const std::vector<unsigned int> &indices)
{
	return true;
}

void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}

void Mesh::BeginDraw(crossplatform::DeviceContext &,crossplatform::ShadingMode pShadingMode) const
{
}

void Mesh::Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const
{
}

void Mesh::EndDraw(crossplatform::DeviceContext &) const
{
}