#pragma once

#include "Export.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "glad/glad.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

namespace platform
{
	namespace opengl
	{
		// Save mesh vertices, normals, UVs and indices in GPU with OpenGL Vertex Buffer Objects
		class SIMUL_OPENGL_EXPORT Mesh:public crossplatform::Mesh
		{
		public:
			Mesh(crossplatform::RenderPlatform*);
			~Mesh();
			bool Initialize(const std::vector<vec3> &vertices,const std::vector<unsigned int> &indices);
			bool Initialize(crossplatform::RenderPlatform *renderPlatform,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount,const unsigned int *lIndices);
			void BeginDraw(crossplatform::DeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const;
			void Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const;
			void EndDraw(crossplatform::DeviceContext &deviceContext) const;
		/// Identify the types of vertex buffer.
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
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
