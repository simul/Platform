#pragma once

#include "Platform/OpenGL/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "glad/glad.h"
#include <set>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

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

		class SIMUL_OPENGL_EXPORT SamplerState : public crossplatform::SamplerState
		{
		public:
			        SamplerState();
			virtual ~SamplerState() override;
            void    Init(crossplatform::SamplerStateDesc* desc);
			void    InvalidateDeviceObjects() override;
            GLuint  asGLuint() override;

        private:
            GLuint mSamplerID;
		};

		class SIMUL_OPENGL_EXPORT Texture : public simul::crossplatform::Texture
		{
		public:
			                Texture();
							~Texture() override;
            
            void            SetName(const char* n) override;

			void            LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8, bool gen_mips = false) override;
			void            LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1) override;
			bool            IsValid() const override;
			void            InvalidateDeviceObjects() override;
			virtual void    InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt=false, bool setDepthStencil=false,bool need_srv=true, int numOfSamples = 1) override;
			bool            ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform, int w, int l,int m,
                                                         crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
                                                         vec4 clear = vec4(0.0f, 0.0f, 0.0f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0) override;
			bool            ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int nmips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool ascubemap=false) override;
			bool            ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int nmips=1,bool rendertargets=false) override;
			void            ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear) override;
			void            GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext) override;
			void            setTexels(crossplatform::DeviceContext& deviceContext,const void* src,int texel_index,int num_texels) override;
			void            activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext,int array_index=-1,int mip_index=0) override;
			void            deactivateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext) override;
			int             GetLength() const override;
			int             GetWidth() const override;
			int             GetDimension() const override;
			int             GetSampleCount() const override;
			bool            IsComputable() const override;
			void            copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels) override;

            GLuint          AsOpenGLView(crossplatform::ShaderResourceType type, int layer = -1, int mip = -1, bool rw = false);
            GLuint          GetGLMainView();

			/// This is used to make a handle (created from the GL texture view, or from the view and a sampler state) resident, and also for the texture to keep track of its own
			/// handles so they can be made unresident when it is deleted.
			void			MakeHandleResident(GLuint64 h);
        private:
            LoadedTexture   LoadTextureData(const char* path);
            bool            IsSame(int w, int h, int d, int arraySize, int m, int msaa);
            //! Initializes the texture views
            void            InitViews(int mipCount, int layers, bool isRenderTarget);
            //! Creates the Framebuffers for this texture
            void            CreateFBOs(int sampleCount);
            //! Applies default sampling parameters to the texId texture
            void            SetDefaultSampling(GLuint texId);
            //! Sets a debug label name to the provided texture _id_
            void            SetGLName(const char* n, GLuint id);

            GLenum                              mInternalGLFormat;
            GLenum                              mGLFormat;
            GLuint                              mTextureID;
            GLuint                              mCubeArrayView;

            std::vector<GLuint>                 mLayerViews;
            std::vector<GLuint>                 mMainMipViews;
            std::vector<std::vector<GLuint>>    mLayerMipViews;
		
            std::vector<std::vector<GLuint>>    mTextureFBOs;
			std::set<GLuint64>					residentHandles;

			int									mNumSamples = 1;
        };
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
