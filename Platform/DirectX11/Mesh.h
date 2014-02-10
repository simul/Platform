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
			void release();
			// Implementing scene::Mesh
			void BeginDraw	(void *context,scene::ShadingMode pShadingMode,const double* mat) const;
			// Draw all the faces with specific material with given shading mode.
			void Draw		(void *context,int pMaterialIndex,scene::ShadingMode pShadingMode) const;
			// Unbind buffers, reset vertex arrays, turn off lighting and texture.
			void EndDraw	(void *context) const;
			// Template function to initialize vertices from an arbitrary vertex structure.
			template<class T> void init(ID3D11Device *pd3dDevice,const std::vector<T> &vertices,std::vector<unsigned short> indices)
			{
				int num_vertices	=(int)vertices.size();
				int num_indices		=(int)indices.size();
				T *v				=new T[num_vertices];
				unsigned short *ind	=new unsigned short[num_indices];
				for(int i=0;i<num_vertices;i++)
					v[i]=vertices[i];
				for(int i=0;i<num_indices;i++)
					ind[i]=indices[i];
				init(pd3dDevice,num_vertices,num_indices,v,ind);
				delete [] v;
				delete [] ind;
			}
			template<class T> void init(ID3D11Device *pd3dDevice,int num_vertices,int num_indices,T *vertices,unsigned short *indices)
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
					num_indices*sizeof(unsigned short),
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_INDEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = indices;
				InitData.SysMemPitch = sizeof(unsigned short);
				hr=pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
			}
			void apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer);
			ID3D11Buffer *vertexBuffer;
			ID3D11Buffer *indexBuffer;
			unsigned stride;
			unsigned numVertices;
			unsigned numIndices;
		protected:
			// Implementing scene::Mesh
			bool Initialize(int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices);
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
		};
	}
}