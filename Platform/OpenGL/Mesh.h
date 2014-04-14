#ifndef SIMUL_OPENGL_MESH_H
#define SIMUL_OPENGL_MESH_H

#include "Export.h"
#include "Simul/Scene/Mesh.h"
#include "Simul/Platform/CrossPlatform/SL/Cppsl.hs"

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
			//bool Initialize(const fbxsdk_2014_2_1::FbxMesh * pMesh);
			bool Initialize(const std::vector<vec3> &vertices,const std::vector<unsigned int> &indices);
			void BeginDraw(void *,scene::ShadingMode pShadingMode,const double* mat) const;
			void Draw(void *,int pMaterialIndex,scene::ShadingMode pShadingMode) const;
			void EndDraw(void *) const;
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