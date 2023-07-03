#pragma once

#include "Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

typedef int GLint;
typedef unsigned int GLenum;
typedef uint64_t GLuint64;

namespace platform
{
	namespace opengl
	{
		class Material;
        class Texture;

        //! Holds a GL state snapshot
        struct GLSnapState
        {
            GLint                       Vao;       
            GLint                       Program;
            GLint                       Framebuffer;

            crossplatform::RenderState  Blend;
            crossplatform::RenderState  Depth;
            crossplatform::RenderState  Raster;
        };

		//! OpenGL renderplatform implementation
		class SIMUL_OPENGL_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			            RenderPlatform();
			virtual~    RenderPlatform() override;
			const char* GetName() const override;
			crossplatform::RenderPlatformType GetType() const override
			{
				return crossplatform::RenderPlatformType::OpenGL;
			}
			virtual const char *GetSfxConfigFilename() const override
			{
				return "GLSL/GLSL.json";
			}
			void        RestoreDeviceObjects(void*) override;
			void        InvalidateDeviceObjects() override;
			void        BeginFrame() override;
			void        EndFrame() override;
            void        BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)override;
            void        EndEvent(crossplatform::DeviceContext& deviceContext)override;
            //! Before starting trueSKY rendering is a good idea to save all the previous state
            void        StoreGLState();
            //! Once we are done, we can restore it
            void        RestoreGLState();
			//! Ensures that all UAV read and write operation to the textures are completed.
			void		ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* texture) override;
			//! Ensures that all UAV read and write operation to the PlatformStructuredBuffer are completed.
			void		ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb) override;
			void        DispatchCompute(crossplatform::DeviceContext& deviceContext, int w, int l, int d) override;
			void        Draw(crossplatform::GraphicsDeviceContext& deviceContext, int num_verts, int start_vert) override;
			void        DrawIndexed(crossplatform::GraphicsDeviceContext& deviceContext, int num_indices, int start_index = 0, int base_vertex = 0) override;
			void        DrawQuad(crossplatform::GraphicsDeviceContext& deviceContext) override;
			void        GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx = -1)override;
            //! This should be called after a Draw/Dispatch command that uses
            //! textures. Here we will apply the textures.
            void        ApplyCurrentPass(crossplatform::DeviceContext& deviceContext);
			void		ApplyPass(crossplatform::DeviceContext& deviceContext, crossplatform::EffectPass* pass) override;

            void        InsertFences(crossplatform::DeviceContext& deviceContext);

            crossplatform::Material*                CreateMaterial();
			crossplatform::Mesh*                    CreateMesh() override;
			crossplatform::Light*                   CreateLight();
			crossplatform::Framebuffer*         CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState*            CreateSamplerState(crossplatform::SamplerStateDesc *) override;
			crossplatform::Effect*                  CreateEffect() override;
			crossplatform::Effect*                  CreateEffect(const char *filename_utf8) override;
			crossplatform::PlatformConstantBuffer*  CreatePlatformConstantBuffer() override;
			crossplatform::PlatformStructuredBuffer*CreatePlatformStructuredBuffer() override;
			crossplatform::Buffer*                  CreateBuffer() override;
			crossplatform::Layout*                  CreateLayout(int num_elements,const crossplatform::LayoutDesc *,bool interleaved) override;
			crossplatform::RenderState*             CreateRenderState(const crossplatform::RenderStateDesc &desc) override;
			crossplatform::Query*                   CreateQuery(crossplatform::QueryType type) override;
			crossplatform::Shader*                  CreateShader() override;

			crossplatform::DisplaySurface*			CreateDisplaySurface() override;
			void*                                   GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext, int slot, int num_buffers,const crossplatform::Buffer *const*buffers, const crossplatform::Layout *layout, const int *vertexSteps = NULL) override;
			
			void									SetStreamOutTarget(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0) override;
			void									ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth) override;
			void									DeactivateRenderTargets(crossplatform::GraphicsDeviceContext &) override;
			void									SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps) override;

			void									SetIndexBuffer(crossplatform::GraphicsDeviceContext &deviceContext, const crossplatform::Buffer *buffer) override;
			
			void									SetTopology(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Topology t) override;
			void									EnsureEffectIsBuilt(const char *filename_utf8) override;

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									PopRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
            void					                SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)override;
			void									Resolve(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::GraphicsDeviceContext&, crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			void									SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& res, crossplatform::Texture* tex, const crossplatform::SubresourceLayers& subresource) override;

            static GLenum                           toGLTopology(crossplatform::Topology t);
            static GLenum                           toGLMinFiltering(crossplatform::SamplerStateDesc::Filtering f);
            static GLenum                           toGLMaxFiltering(crossplatform::SamplerStateDesc::Filtering f);
			static GLint							toGLWrapping(crossplatform::SamplerStateDesc::Wrapping w);
			//! Returns the format that OpenGL stores the texture data internally. e.g. GL_RGBA8
			static                                  GLuint ToGLInternalFormat(crossplatform::PixelFormat p);
			//! Returns the crossplatform::PixelFormat from the OpenGL internal format. e.g PixelFormat::RGBA_8_UNORM
			static                                  crossplatform::PixelFormat FromGLInternalFormat(GLuint p);
			//! Return the format that OpenGL uses the load in pixel data. e.g GL_RGBA
			static                                  GLuint ToGLFormat(crossplatform::PixelFormat p);
			static                                  GLenum DataType(crossplatform::PixelFormat p);
			static int                              FormatCount(crossplatform::PixelFormat p);
            
			void									DeleteGLTextures(const std::set<GLuint> &t);
            //! Makes the handle resident only if its not resident already
            void                                    MakeTextureResident(GLuint64 handle);

			//! If ANY texture is deleted, we should probably regard the resident textures list as invalid.
			void									ClearResidentTextures();
            //! Returns 2D dummy texture 1 white texel
            opengl::Texture*                        GetDummy2D();
            //! Returns 3D dummy texture 1 white texel
            opengl::Texture*                        GetDummy3D();

        protected:
			crossplatform::Texture* createTexture() override;
            //! GL forces us to have a VertexArrayObject bound, we bind it but we dont use it
            GLuint              mNullVAO;
            GLenum              mCurTopology;
            //! Used to hold the current resident textures.
            std::set<GLuint64>  mResidentTextures;

            GLint               mMaxViewports;
            GLint               mMaxColorAttatch;

            opengl::Texture*    mDummy2D;
            opengl::Texture*    mDummy3D;

            GLSnapState         mCachedState;

			static std::set<GLuint>	texturesToDelete[3];
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
