#pragma once

#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "glad/glad.h"

namespace simul
{
	namespace opengl
	{
        struct LoadedTexture
        {
            int x;
            int y;
            int z;
            int n;
            unsigned char* data;
        };

		class SIMUL_OPENGL_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			        SamplerState();
			virtual ~SamplerState();
            void    Init(crossplatform::SamplerStateDesc* desc);
            void    InvalidateDeviceObjects();
            GLuint  asGLuint()override;
        private:
            GLuint mSamplerID;
		};

		class SIMUL_OPENGL_EXPORT Texture:public simul::crossplatform::Texture
		{
		public:
			                Texture();
			                ~Texture();
            
            void            SetName(const char* n)override;

			void            LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void            LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1);
			bool            IsValid() const;
			void            InvalidateDeviceObjects();
			virtual void    InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false, bool setDepthStencil=false) override;
			bool            ensureTexture2DSizeAndFormat(   crossplatform::RenderPlatform *renderPlatform, int w, int l,
                                                            crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
				                                            vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0);
			bool            ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int nmips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool ascubemap=false) override;
			bool            ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int nmips=1,bool rendertargets=false);
			void            ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear) override;
			void            GenerateMips(crossplatform::DeviceContext& deviceContext);
			void            setTexels(crossplatform::DeviceContext& deviceContext,const void* src,int texel_index,int num_texels);
			void            activateRenderTarget(crossplatform::DeviceContext& deviceContext,int array_index=-1,int mip_index=0);
			void            deactivateRenderTarget(crossplatform::DeviceContext& deviceContext);
			int             GetLength() const;
			int             GetWidth() const;
			int             GetDimension() const;
			int             GetSampleCount() const;
			bool            IsComputable() const override;
			void            copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels);

            GLuint          AsOpenGLView(crossplatform::ShaderResourceType type, int layer = -1, int mip = -1, bool rw = false);
            GLuint          GetGLMainView();

        private:
            LoadedTexture   LoadTextureData(const char* path);
            bool            IsSame(int w, int h, int d,int arraySize, int m, int msaa);
            //! Initializes the texture views
            void            InitViews(int mipCount, int layers, bool isRenderTarget);
            //! Creates the Framebuffers for this texture
            void            CreateFBOs(int sampleCount);
            //! Applies default sampling parameters to the texId texture
            void            SetDefaultSampling(GLuint texId);
            //! Sets a debug label name to the provided texture _id_
            void            SetGLName(const char* n, GLuint id);

            GLenum                              mInternalGLFormat;
            GLuint                              mTextureID;
            GLuint                              mCubeArrayView;

            std::vector<GLuint>                 mLayerViews;
            std::vector<GLuint>                 mMainMipViews;
            std::vector<std::vector<GLuint>>    mLayerMipViews;
		
            std::vector<std::vector<GLuint>>    mTextureFBOs;
        };
	}
}
