#pragma once
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "SimulDirectXHeader.h"
#include <vector>

namespace simul
{
	namespace dx12
	{
		class Mesh:public crossplatform::Mesh
		{
		public:
			Mesh();
			~Mesh();
			void InvalidateDeviceObjects();
			//! Implementing crossplatform::Mesh
			bool Initialize(crossplatform::RenderPlatform *renderPlatform, int lPolygonVertexCount, const float *lVertices, const float *lNormals, const float *lUVs, int lPolygonCount, const unsigned int *lIndices);
			void GetVertices(void *target, void *indices);
			void releaseBuffers();
			//! Implementing crossplatform::Mesh
			void BeginDraw	(crossplatform::DeviceContext &deviceContext,crossplatform::ShadingMode pShadingMode) const;
			//! Draw all the faces with specific material with given shading mode.
			void Draw		(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const;
			//! Unbind buffers, reset vertex arrays, turn off lighting and texture.
			void EndDraw	(crossplatform::DeviceContext &deviceContext) const;
			//! Template function to initialize vertices from an arbitrary vertex structure.
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,const std::vector<T> &vertices,std::vector<U> indices)
			{
			}
			template<class T,typename U> void init(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int num_indices,T *vertices,U *indices)
			{
			}
			void apply(crossplatform::DeviceContext &deviceContext,unsigned instanceStride, crossplatform::Buffer *instanceBuffer);
			crossplatform::Buffer		*vertexBuffer;
			crossplatform::Buffer		*indexBuffer;
			crossplatform::Layout	*inputLayout;
		protected:
			void UpdateVertexPositions(int lVertexCount, float *lVertices) const;
		};
	}
}