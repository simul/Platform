#include "Mesh.h"
#include "MacrosDX1x.h"
using namespace simul;
using namespace dx11;

Mesh::Mesh()
	:vertexBuffer(NULL)
	,indexBuffer(NULL)
	,stride(0)
	,numVertices(0)
	,numIndices(0)
{
}

Mesh::~Mesh()
{
	release();
}

void Mesh::release()
{
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	stride=0;
	numVertices=0;
	numIndices=0;
}

void Mesh::BeginDraw(void *context,scene::ShadingMode pShadingMode,const double* mat) const
{
}
// Draw all the faces with specific material with given shading mode.
void Mesh::Draw(void *context,int pMaterialIndex,scene::ShadingMode pShadingMode) const
{
}
// Unbind buffers, reset vertex arrays, turn off lighting and texture.
void Mesh::EndDraw(void *context) const
{
}

void Mesh::apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer)
{
	UINT strides[]={stride,instanceStride};
	UINT offsets[]={0,0};
	ID3D11Buffer *buffers[]={vertexBuffer,instanceBuffer};

	pImmediateContext->IASetVertexBuffers(	0,			// the first input slot for binding
												2,			// the number of buffers in the array
												buffers,	// the array of vertex buffers
												strides,	// array of stride values, one for each buffer
												offsets);	// array of offset values, one for each buffer

	UINT Strides[1];
	UINT Offsets[1];
	Strides[0] = 0;
	Offsets[0] = 0;
	pImmediateContext->IASetIndexBuffer(	indexBuffer,
											DXGI_FORMAT_R16_UINT,	// unsigned short
											0);						// array of offset values, one for each buffer
	
}

bool Mesh::Initialize(int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices)
{
	return true;
}
void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}