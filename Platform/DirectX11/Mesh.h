#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Scene/Mesh.h"
#include <d3d11.h>
#include <vector>

namespace simul
{
	namespace dx11
	{
		class Mesh:public scene::Mesh
		{
		public:
			Mesh();
			~Mesh();
			// Implementing scene::Mesh
			bool Initialize(void *device,int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices);
			void release();
			// Implementing scene::Mesh
			void BeginDraw	(void *context,scene::ShadingMode pShadingMode) const;
			// Draw all the faces with specific material with given shading mode.
			void Draw		(void *context,int pMaterialIndex,scene::ShadingMode pShadingMode) const;
			// Unbind buffers, reset vertex arrays, turn off lighting and texture.
			void EndDraw	(void *context) const;
			// Template function to initialize vertices from an arbitrary vertex structure.
			template<class T,typename U> void init(ID3D11Device *pd3dDevice,const std::vector<T> &vertices,std::vector<U> indices)
			{
				int num_vertices	=(int)vertices.size();
				int num_indices		=(int)indices.size();
				T *v				=new T[num_vertices];
				U *ind				=new U[num_indices];
				for(int i=0;i<num_vertices;i++)
					v[i]=vertices[i];
				for(int i=0;i<num_indices;i++)
					ind[i]=indices[i];
				init(pd3dDevice,num_vertices,num_indices,v,ind);
				delete [] v;
				delete [] ind;
			}
			template<class T,typename U> void init(ID3D11Device *pd3dDevice,int num_vertices,int num_indices,T *vertices,U *indices)
			{
				release();
				stride=sizeof(T);
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
				HRESULT hr=pd3dDevice->CreateBuffer(&vertexBufferDesc,&InitData,&vertexBuffer);
				
				// index buffer
				D3D11_BUFFER_DESC indexBufferDesc=
				{
					num_indices*sizeof(U),
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_INDEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = indices;
				InitData.SysMemPitch = sizeof(U);
				hr=pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
			}
			void apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer);
			ID3D11Buffer *vertexBuffer;
			ID3D11Buffer *indexBuffer;
			ID3D11InputLayout* inputLayout;
			unsigned stride;
			unsigned numVertices;
			unsigned numIndices;
		protected:
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
			mutable ID3D11InputLayout* previousInputLayout;
			mutable D3D11_PRIMITIVE_TOPOLOGY previousTopology;
		};
	}
}