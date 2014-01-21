#ifndef SIMUL_OPENGL_MESH_H
#define SIMUL_OPENGL_MESH_H

#include "Export.h"
#include "Simul/Scene/Mesh.h"

namespace simul
{
	namespace opengl
	{
		// Save mesh vertices, normals, UVs and indices in GPU with OpenGL Vertex Buffer Objects
		class SIMUL_OPENGL_EXPORT Mesh:public scene::Mesh
		{
		public:
			Mesh();
			~Mesh();
			bool Initialize(const FbxMesh * pMesh);
			void BeginDraw(scene::ShadingMode pShadingMode,const double* mat) const;
			void Draw(int pMaterialIndex,scene::ShadingMode pShadingMode) const;
			void EndDraw() const;
			enum
			{
				VERTEX_VBO,
				NORMAL_VBO,
				UV_VBO,
				INDEX_VBO,
				VBO_COUNT,
			};
			GLuint mVBONames[VBO_COUNT];
		protected:
			bool Initialize(int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices);
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
		};
	}
}
#endif