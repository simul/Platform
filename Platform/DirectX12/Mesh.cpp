#include "Mesh.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#endif

#include "MacrosDX1x.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "D3dx11effect.h"
using namespace simul;
using namespace dx11on12;

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
	renderPlatform = r;
	stride=0;
	numVertices=0;
	numIndices=0;
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		crossplatform::Effect *effect=NULL;
		std::map<std::string,std::string> defines;
		effect=renderPlatform->CreateEffect("solid",defines);
		if(!effect)
			return false;
		crossplatform::EffectTechnique *tech	=effect->GetTechniqueByName("solid");
		if(!tech)
			return false;
		tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
		const crossplatform::LayoutDesc layoutDesc[]=
		{
			{"POSITION"	,0	,crossplatform::PixelFormat::RGBA_32_FLOAT,0,0,	false,0},
			{"TEXCOORD"	,0	,crossplatform::PixelFormat::RG_32_FLOAT,0,12, false,0},
			{"TEXCOORD"	,1	,crossplatform::PixelFormat::RGB_32_FLOAT,0,20, false,0},
		};
		delete inputLayout;
		inputLayout= renderPlatform->CreateLayout(3, layoutDesc);
		SAFE_DELETE(effect);
	}
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
	init(renderPlatform,numVertices,numIndices,vertices,lIndices);
	delete [] vertices;
	return true;
}

void Mesh::releaseBuffers()
{
	delete (vertexBuffer);
	delete (indexBuffer);
	numVertices=0;
	numIndices=0;
}

void Mesh::GetVertices(void *target, void *indices)
{
}


void Mesh::BeginDraw(crossplatform::DeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	// Set the input layout
	renderPlatform->SetLayout(deviceContext,inputLayout);
	renderPlatform->SetTopology(deviceContext,crossplatform::TRIANGLELIST);
	done_begin=true;
}

// Draw all the faces with specific material with given shading mode.
void Mesh::Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const
{
	bool init=done_begin;
	if(!init)
		BeginDraw(deviceContext,crossplatform::SHADING_MODE_SHADED);
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	UINT offset = 0;
	renderPlatform->SetVertexBuffers(deviceContext
									,0					// the first input slot for binding
									,1					// the number of buffers in the array
									,&vertexBuffer		// the array of vertex buffers
									,inputLayout);			// array of offset values, one for each buffer
	renderPlatform->SetIndexBuffer(deviceContext,indexBuffer);					

	pContext->DrawIndexed(numIndices,0,0);
	if(!init)
		EndDraw(deviceContext);
}

// Unbind buffers, reset vertex arrays, turn off lighting and texture.
void Mesh::EndDraw(crossplatform::DeviceContext &deviceContext) const
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	done_begin=false;
}

void Mesh::apply(crossplatform::DeviceContext &deviceContext,unsigned instanceStride,crossplatform::Buffer *instanceBuffer)
{
	UINT strides[]={stride,instanceStride};
	UINT offsets[]={0,0};
	crossplatform::Buffer *buffers[]={vertexBuffer,instanceBuffer};
	renderPlatform->SetVertexBuffers(deviceContext, 0, 2, buffers, inputLayout);
	renderPlatform->SetIndexBuffer(deviceContext, indexBuffer);
	
}

void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}