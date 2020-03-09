#ifndef VBOMESH_H
#define VBOMESH_H

#include <vector>
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/ShaderMode.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/Topology.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/solid_constants.sl"
#include "Platform/Math/Orientation.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
#include <vector>

namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class RenderPlatform;
		// Save mesh vertices, normals, UVs and indices in GPU with OpenGL Vertex Buffer Objects
		class SIMUL_CROSSPLATFORM_EXPORT Mesh
		{
		protected:
			mutable bool done_begin;
			crossplatform::RenderPlatform *renderPlatform;
		public:
			Mesh();
			virtual ~Mesh();
			void InvalidateDeviceObjects();
			void Initialize(crossplatform::RenderPlatform *renderPlatform,crossplatform::MeshType m);
			bool Initialize(crossplatform::RenderPlatform *renderPlatform
				,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs
				,int lPolygonCount
				,const unsigned int *lIndices
				,const unsigned short *sIndices);
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
			// Bind buffers, set vertex arrays, turn on lighting and texture.
			void BeginDraw(DeviceContext &deviceContext, ShadingMode pShadingMode) const;
			// Draw all the faces with specific material with given shading mode.
			void Draw(crossplatform::DeviceContext &deviceContext, int pMaterialIndex) const;
			// Unbind buffers, reset vertex arrays, turn off lighting and texture.
			void EndDraw(crossplatform::DeviceContext &deviceContext) const;
			void apply(crossplatform::DeviceContext &deviceContext, unsigned instanceStride, Buffer *instanceBuffer);
			// Get the count of material groups
			int GetSubMeshCount() const;
			void SetSubMesh(int submesh,int index_start,int num_indices,Material *m);
			
			int VERTEX_STRIDE;
			int NORMAL_STRIDE;
			int UV_STRIDE;
			int TRIANGLE_VERTEX_COUNT;
			struct SubMesh
			{
				SubMesh() : IndexOffset(0), TriangleCount(0),drawAs(AS_TRIANGLES),material(nullptr) {}
				int IndexOffset;
				int TriangleCount;

				enum DrawAs {AS_TRIANGLES,AS_TRISTRIP};
				DrawAs drawAs;
				Material *material;
			};
			//! The submeshes are the parts that have different materials.
			SubMesh *GetSubMesh(int index);
			const SubMesh *GetSubMesh(int index) const;
			bool mHasNormal;
			bool mHasUV;
			bool mAllByControlPoint; // Save data in VBO by control point or by polygon vertex.
			// For every material, record the offsets in every VBO and triangle counts
			std::vector<SubMesh*> mSubMeshes;
			//! The children are meshes with different orientation.
			std::vector<Mesh*> children;
			simul::geometry::SimulOrientation orientation;
			unsigned stride;		// number of bytes per vertex.
			unsigned indexSize;
			unsigned numVertices;
			unsigned numIndices;
		protected:
			void releaseBuffers();
			// Template function to initialize vertices from an arbitrary vertex structure.
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,const std::vector<T> &vertices,std::vector<U> indices)
			{
				stride = sizeof(T);
				indexSize = sizeof(U);
				numVertices = (int)vertices.size();
				numIndices = (int)indices.size();
				T *v				=new T[numVertices];
				U *ind				=new U[numIndices];
				for(int i=0;i<numVertices;i++)
					v[i]=vertices[i];
				for(int i=0;i<numIndices;i++)
					ind[i]=indices[i];
				init(renderPlatform,numVertices,numIndices,v,ind);
				delete [] v;
				delete [] ind;
			}
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int num_indices,T *vertices,U *indices)
			{
				releaseBuffers();
				delete vertexBuffer;
				delete indexBuffer;
				stride			=sizeof(T);
				indexSize		=sizeof(U);
				numVertices		=num_vertices;
				numIndices		=num_indices;
				vertexBuffer	=renderPlatform->CreateBuffer();
				vertexBuffer->EnsureVertexBuffer(renderPlatform, num_vertices, layout, vertices, true);
				indexBuffer		=renderPlatform->CreateBuffer();
				indexBuffer->EnsureIndexBuffer(renderPlatform, num_indices, indexSize, indices);
			}
			Buffer		*vertexBuffer;
			Buffer		*indexBuffer;
			Layout		*layout;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif