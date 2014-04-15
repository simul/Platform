#include "Mesh.h"
#include "MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
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
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&vertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(inputLayout);

	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
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
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	stride=0;
	numVertices=0;
	numIndices=0;
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectTechnique *tech	=solidEffect->GetTechniqueByName("solid");
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{"POSITION"	,0	,DXGI_FORMAT_R32G32B32_FLOAT,0,0,	D3D11_INPUT_PER_VERTEX_DATA,0},
			{"TEXCOORD"	,0	,DXGI_FORMAT_R32G32_FLOAT	,0,0,	D3D11_INPUT_PER_VERTEX_DATA,0},
			{"TEXCOORD"	,1	,DXGI_FORMAT_R32G32B32_FLOAT,1,8,	D3D11_INPUT_PER_VERTEX_DATA,1},
		};
		SAFE_RELEASE(inputLayout);
		V_CHECK(m_pd3dDevice->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &inputLayout));
	}
	D3D11_BUFFER_DESC desc=
	{
        lPolygonVertexCount*sizeof(vec3),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem		=vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));
}

	return true;
}
void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}