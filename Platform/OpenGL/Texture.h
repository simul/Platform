#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			GLuint sampler_state;
			SamplerState();
			virtual ~SamplerState();
			void InvalidateDeviceObjects();
			GLuint asGLuint()
			{
				return sampler_state;
			}
		};
		class SIMUL_OPENGL_EXPORT Texture:public simul::crossplatform::Texture
		{
			GLuint **m_fb;// 2D table: layers and mips.
			int main_viewport[4];
			bool externalTextureObject;
			bool computable;
		public:
			Texture();
			~Texture();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files);
			bool IsValid() const;
			void InvalidateDeviceObjects();
			GLuint AsGLuint(int index=-1,int mip=-1);
			GLuint FramebufferAsGLuint(int index=0,int mip=0)
			{
				return m_fb[index][mip];
			}
			/// Initialize from native OpenGL texture GLuint t. srv is ignored.
			void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false) override;
			bool ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				, crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
				vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0);
			bool ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int mips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false) override;
			bool ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false);
			void ClearDepthStencil(crossplatform::DeviceContext &deviceContext, float depthClear, int stencilClear) override;
			void GenerateMips(crossplatform::DeviceContext &deviceContext);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			void activateRenderTarget(crossplatform::DeviceContext &deviceContext,int array_index=-1,int mip_index=0);
			void deactivateRenderTarget(crossplatform::DeviceContext &deviceContext);
			int GetLength() const;
			int GetWidth() const;
			int GetDimension() const;
			int GetSampleCount() const;
			bool IsComputable() const override;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels);
			GLuint pTextureObject;
			GLuint *mipObjects;			// Whole texture, at different mips.
			GLuint *layerObjects;		// Layer of texture array, all mips.
			GLuint **layerMipObjects;	// Layer of texture array at different mips.
			// Former Texture functions
			void setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d);
			
			void InitRTVTables(int l,int m);
			void FreeRTVTables();
			void InitViewTables(int l,int m);
			void FreeViewTables();
		};
	}
}
