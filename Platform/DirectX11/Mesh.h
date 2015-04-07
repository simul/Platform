#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "SimulDirectXHeader.h"
#include <vector>

namespace simul
{
	namespace dx11
	{
		class Mesh:public crossplatform::Mesh
		{
		public:
			Mesh();
			~Mesh();
			void InvalidateDeviceObjects();
			// Implementing crossplatform::Mesh
			bool Initialize(crossplatform::RenderPlatform *renderPlatform, int lPolygonVertexCount, const float *lVertices, const float *lNormals, const float *lUVs, int lPolygonCount, const unsigned int *lIndices);
			void GetVertices(void *target);
			void releaseBuffers();
			// Implementing crossplatform::Mesh
			void BeginDraw	(crossplatform::DeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const;
			// Draw all the faces with specific material with given shading mode.
			void Draw		(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const;
			// Unbind buffers, reset vertex arrays, turn off lighting and texture.
			void EndDraw	(crossplatform::DeviceContext &deviceContext) const;
			// Template function to initialize vertices from an arbitrary vertex structure.
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,const std::vector<T> &vertices,std::vector<U> indices)
			{
				stride = sizeof(T);
				indexSize = sizeof(U);
				numVertices = (int)vertices.size();
				numIndices = (int)indices.size();
				T *v				=new T[num_vertices];
				U *ind				=new U[num_indices];
				for(int i=0;i<num_vertices;i++)
					v[i]=vertices[i];
				for(int i=0;i<num_indices;i++)
					ind[i]=indices[i];
				init(renderPlatform,num_vertices,num_indices,v,ind);
				delete [] v;
				delete [] ind;
			}
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int num_indices,T *vertices,U *indices)
			{
				releaseBuffers();
				stride=sizeof(T);
				indexSize = sizeof(U);
				numVertices=num_vertices;
				numIndices=num_indices;
				D3D11_BUFFER_DESC vertexBufferDesc=
				{
					num_vertices*sizeof(T),
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_VERTEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				D3D11_SUBRESOURCE_DATA InitData;
				ZeroMemory(&InitData,sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = vertices;
				InitData.SysMemPitch = sizeof(T);
				HRESULT hr=renderPlatform->AsD3D11Device()->CreateBuffer(&vertexBufferDesc,&InitData,&vertexBuffer);
				
				// index buffer
				D3D11_BUFFER_DESC indexBufferDesc=
				{
					num_indices*indexSize,
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_INDEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = indices;
				InitData.SysMemPitch = sizeof(U);
				hr=renderPlatform->AsD3D11Device()->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
			}
			void apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer);
			ID3D11Buffer		*vertexBuffer;
			ID3D11Buffer		*indexBuffer;
			ID3D11InputLayout	*inputLayout;
			unsigned stride;		// number of bytes per vertex.
			unsigned indexSize;
			unsigned numVertices;
			unsigned numIndices;
		protected:
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
			mutable ID3D11InputLayout* previousInputLayout;
			mutable D3D11_PRIMITIVE_TOPOLOGY previousTopology;
		};
	}
}