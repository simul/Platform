#include "Mesh.h"
#include "DeviceContext.h"

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

crossplatform::Mesh::Mesh() 
	:done_begin(false)
	,mHasNormal(false)
	,mHasUV(false)
	,mAllByControlPoint(true)
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
	if(index<0||index>=mSubMeshes.size())
		return NULL;
	return mSubMeshes[index];
}
			
const crossplatform::Mesh::SubMesh *crossplatform::Mesh::GetSubMesh(int index) const
{
	return mSubMeshes[index];
}

static const float unity=1000.0f;
struct Vec3
{
	float x,y,z;
};
static const Vec3 box_vertices[8] =
{
	{-unity,		-unity,	-unity},
	{-unity,		-unity,	unity},
	{-unity,		unity,	-unity},
	{-unity,		unity,	unity},
	{unity,			-unity,	-unity},
	{unity,			-unity,	unity},
	{unity,			unity,	-unity},
	{unity,			unity,	unity},
};
static const unsigned int MMM=0,MMP=1,MPM=2,MPP=3,PMM=4,PMP=5,PPM=6,PPP=7;
static const unsigned int box_indices[36] =
{
	MMP,
	PPP,
	PMP,
	PPP,
	MMP,
	MPP,
	
	MMM,
	PMM,
	PPM,
	PPM,
	MPM,
	MMM,
	
	MPM,
	PPM,
	PPP,
	PPP,
	MPP,
	MPM,
				
	MMM,
	PMP,
	PMM,
	PMP,
	MMM,
	MMP,
	
	PMM,
	PPP,
	PPM,
	PPP,
	PMM,
	PMP,
				
	MMM,
	MPM,
	MPP,
	MPP,
	MMP,
	MMM,
};
void Mesh::Initialize(crossplatform::RenderPlatform *r,crossplatform::MeshType m)
{
	renderPlatform = r;
	Initialize(renderPlatform,8,(const float*)box_vertices,(const float*)box_vertices,(const float*)box_vertices,12,(const unsigned *)box_indices);
}
