#ifndef VBOMESH_H
#define VBOMESH_H

#include <vector>
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/ShaderMode.h"
#include "Simul/Platform/CrossPlatform/Topology.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class RenderPlatform;
		// Save mesh vertices, normals, UVs and indices in GPU with OpenGL Vertex Buffer Objects
		class SIMUL_CROSSPLATFORM_EXPORT Mesh
		{
		public:
			Mesh();
			virtual ~Mesh();
			void Initialize(crossplatform::RenderPlatform *renderPlatform,crossplatform::MeshType m);
			virtual bool Initialize(crossplatform::RenderPlatform *renderPlatform,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount,const unsigned int *lIndices)=0;
			virtual void UpdateVertexPositions(int lVertexCount, float *lVertices) const=0;
			// Bind buffers, set vertex arrays, turn on lighting and texture.
			virtual void BeginDraw(DeviceContext &deviceContext,ShadingMode pShadingMode) const=0;
			// Draw all the faces with specific material with given shading mode.
			virtual void Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex, ShadingMode pShadingMode) const=0;
			// Unbind buffers, reset vertex arrays, turn off lighting and texture.
			virtual void EndDraw(crossplatform::DeviceContext &deviceContext) const=0;
			// Get the count of material groups
			int GetSubMeshCount() const;
			static int VERTEX_STRIDE ;
			static int NORMAL_STRIDE ;
			static int UV_STRIDE ;
			static int TRIANGLE_VERTEX_COUNT;
			struct SubMesh
			{
				SubMesh() : IndexOffset(0), TriangleCount(0),drawAs(AS_TRIANGLES) {}
				int IndexOffset;
				int TriangleCount;
///
				enum DrawAs {AS_TRIANGLES,AS_TRISTRIP};
				DrawAs drawAs;
			};
			SubMesh *GetSubMesh(int index);
			const SubMesh *GetSubMesh(int index) const;
			bool mHasNormal;
			bool mHasUV;
			bool mAllByControlPoint; // Save data in VBO by control point or by polygon vertex.
			// For every material, record the offsets in every VBO and triangle counts
			std::vector<SubMesh*> mSubMeshes;
		protected:
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif