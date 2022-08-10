#pragma once

#include "Platform/GLES/Export.h"
#include "Platform/CrossPlatform/Buffer.h"
#include <GLES2/gl2.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

namespace simul
{
	namespace gles
	{
        //! GLES buffer (vertex/index) implementation
		class SIMUL_GLES_EXPORT Buffer:public simul::crossplatform::Buffer
		{
		public:
			        Buffer();
					~Buffer() override;
			void    InvalidateDeviceObjects() override;
			GLuint  AsGLuint() override;
			void    EnsureVertexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_vertices,const crossplatform::Layout* layout,const void* data,bool cpu_access=false,bool streamout_target=false) override;
			void    EnsureIndexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_indices,int index_size_bytes,const void *data) override;
			void*   Map(crossplatform::DeviceContext& deviceContext) override;
			void    Unmap(crossplatform::DeviceContext& deviceContext) override;

            void    BindVBO(crossplatform::DeviceContext& deviceContext);

        private:
            GLuint                  mBufferID;
            crossplatform::Layout*  mBufferLayout;
		};
	}
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
