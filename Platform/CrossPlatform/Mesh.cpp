#include "Mesh.h"

using namespace simul;
using namespace crossplatform;

// Three floats for every position.
int crossplatform::Mesh::VERTEX_STRIDE = 3;
// Three floats for every normal.
int crossplatform::Mesh::NORMAL_STRIDE = 3;
// Two floats for every UV.
int crossplatform::Mesh::UV_STRIDE = 2;
int crossplatform::Mesh::TRIANGLE_VERTEX_COUNT = 3;
template <class T> inline void VectorDelete(std::vector<T>& vec)
{
	for( int i = 0, c =(int)vec.size(); i < c; ++i )
	{
		delete vec[i];
	}
	vec.clear();
}

crossplatform::Mesh::Mesh() : mHasNormal(false), mHasUV(false), mAllByControlPoint(true)
{
}

crossplatform::Mesh::~Mesh()
{
	VectorDelete(mSubMeshes);
}

int crossplatform::Mesh::GetSubMeshCount() const
{
	return (int)mSubMeshes.size();
}
			
crossplatform::Mesh::SubMesh *crossplatform::Mesh::GetSubMesh(int index)
{
	return mSubMeshes[index];
}
			
const crossplatform::Mesh::SubMesh *crossplatform::Mesh::GetSubMesh(int index) const
{
	return mSubMeshes[index];
}