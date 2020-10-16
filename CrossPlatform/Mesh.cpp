#include "Mesh.h"
#include "DeviceContext.h"
#include "Macros.h"
#include "Platform/Math/Pi.h"

using namespace simul;
using namespace crossplatform;

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
	, mAllByControlPoint(true)
	, stride(0)
	, indexSize(0)
	, numVertices(0)
	, numIndices(0)
	,vertexBuffer(NULL)
	, indexBuffer(NULL)
	, layout(NULL)
{
}

crossplatform::Mesh::~Mesh()
{
	InvalidateDeviceObjects();
}
void Mesh::InvalidateDeviceObjects()
{
	releaseBuffers();
	VectorDelete(mSubMeshes);
	VectorDelete(children);
	SAFE_DELETE(layout);
}


bool Mesh::Initialize(crossplatform::RenderPlatform *r,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount
	,const unsigned int *lIndices
	,const unsigned short *sIndices)
{
	renderPlatform = r;
	SAFE_DELETE(vertexBuffer);
	SAFE_DELETE(indexBuffer);
	stride=0;
	numVertices=0;
	numIndices=0;
	// Vertex declaration
	crossplatform::LayoutDesc layoutDesc[] =
	{
		{ "POSITION", 0, crossplatform::RGB_32_FLOAT, 0, 0, false, 0 },
		{ "TEXCOORD", 0, crossplatform::RG_32_FLOAT, 0, 12, false, 0 },
		{ "TEXCOORD", 1, crossplatform::RGB_32_FLOAT, 0, 20, false, 0 },
	};
	SAFE_DELETE(layout);
	layout = renderPlatform->CreateLayout(3, layoutDesc,true);
	
	
	// Put positions, texcoords and normals in an array of structs:
	numVertices=lPolygonVertexCount;
	numIndices=lPolygonCount*3;
	struct Vertex
	{
		vec3 pos;
		vec2 texc;
		vec3 normal;
	};
	stride = sizeof(Vertex);
	Vertex *vertices=new Vertex[lPolygonVertexCount];
	for(int i=0;i<lPolygonVertexCount;i++)
	{
		Vertex &v		=vertices[i];
		v.pos			=&(lVertices[i*3]);
		if(lUVs)
			v.texc		=&(lUVs[i*2]);
		if(lNormals)
			v.normal	=&(lNormals[i*3]);
	}
	if(sIndices)
		init(renderPlatform, numVertices, numIndices, vertices, sIndices);
	else
		init(renderPlatform,numVertices,numIndices,vertices,lIndices);
	SetSubMesh(0, 0, numIndices, nullptr);
	delete [] vertices;
	return true;
}

void Mesh::BeginDraw(GraphicsDeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const
{
	if(layout)
		layout->Apply(deviceContext);
	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);
	done_begin=true;
}

// Draw all the faces with specific material with given shading mode.
void Mesh::Draw(GraphicsDeviceContext &deviceContext,int meshIndex) const
{
	bool init = done_begin;
	if (!init)
		BeginDraw(deviceContext, crossplatform::SHADING_MODE_SHADED);
	{
		SubMesh *m = mSubMeshes[meshIndex];
		if (numIndices)
		{
			renderPlatform->SetVertexBuffers(deviceContext, 0, 1, &vertexBuffer, layout);
			renderPlatform->SetIndexBuffer(deviceContext, indexBuffer);
			renderPlatform->DrawIndexed(deviceContext, m->TriangleCount*3, m->IndexOffset,0);
		}
	}
	if(!init)
		EndDraw(deviceContext);
}

// Unbind buffers, reset vertex arrays, turn off lighting and texture.
void Mesh::EndDraw(GraphicsDeviceContext &deviceContext) const
{
	if (layout)
		layout->Unapply(deviceContext);
	done_begin=false;
}


void Mesh::UpdateVertexPositions(int , float *) const
{
}
void Mesh::apply(GraphicsDeviceContext &deviceContext,unsigned instanceStride,Buffer *instanceBuffer)
{
	renderPlatform->SetVertexBuffers(deviceContext, 0, 1, &vertexBuffer, layout);
	renderPlatform->SetIndexBuffer(deviceContext, indexBuffer);
}
void Mesh::releaseBuffers()
{
	SAFE_DELETE(vertexBuffer);
	SAFE_DELETE(indexBuffer);
	numVertices = 0;
	numIndices = 0;
}

void Mesh::SetSubMesh(int submesh, int index_start, int num_indices,Material *m)
{
	while (submesh >= mSubMeshes.size())
	{
		mSubMeshes.push_back(new SubMesh);
	}
	SubMesh *s=mSubMeshes[submesh];
	s->IndexOffset = index_start;
	s->TriangleCount = num_indices / 3;
	s->material = m;
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

static const float unity=1.0f;
static const vec3 box_vertices[8] =
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
	if(m==MeshType::CUBE_MESH)
	{
		vec3 box_vertices2[36];
		vec3 box_normals[36];
		unsigned box_indices2[36];
		for(int i=0;i<36;i++)
		{
			box_indices2[i]=i;
			box_vertices2[i]= box_vertices[box_indices[i]];
			int face=i/6;
			float nx=0.f,ny=0.f,nz=0.f;
			if(face <2)
				nz= face ==0?1.f:-1.f;
			else if (face < 4)
				ny = face == 2 ? 1.f : -1.f;
			else
				nx = face == 4 ? 1.f : -1.f;
			vec3 n(nx,ny,nz);
			box_normals[i]=n;
		}
		Initialize(renderPlatform,36,(const float*)box_vertices2,(const float*)box_normals,(const float*)box_vertices2,12,(const unsigned *)box_indices2,nullptr);
	}
	else
	{
		static int lat=16,lon=32;
		int vertex_count=(lat+1)*lon;
		vec3 *sphere_vertices =new vec3[vertex_count];
		vec3 *sphere_normals =new vec3[vertex_count];
		vec2 *sphere_uvs =new vec2[vertex_count];
		int index_count=2*lat*(lon+1);
		unsigned int *sphere_indices=new unsigned int[index_count];
		vec3 *v=sphere_vertices;
		vec3 *n=sphere_normals;
		vec2 *u=sphere_uvs;
		for(int i=0;i<lat+1;i++)
		{
			float elev=float(i)/float(lat+1)*SIMUL_PI_F/2.0f;
			float z=sin(elev);
			float ce=cos(elev);
			for(int j=0;j<lon;j++)
			{
				float az=float(j)*2.0f*SIMUL_PI_F;
				n->x=v->x=sin(az)*ce;
				n->y=v->y=cos(az)*ce;
				n->z=v->z=z;
				v->x*=unity;
				v->y*=unity;
				v->z*=unity;
				v++;
				n++;
				u->x=0;
				u->y=0;
				u++;
			}
		}

		Initialize(renderPlatform,8,(const float*)sphere_vertices,(const float*)sphere_normals,(const float*)sphere_uvs,12,(const unsigned *)sphere_indices, nullptr);
		delete [] sphere_vertices;
		delete [] sphere_normals;
		delete [] sphere_uvs;
	}
}
