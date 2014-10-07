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
		class SIMUL_OPENGL_EXPORT SamplerState:public simul::crossplatform::SamplerState
		{
			/*
			GLuint sampler_state = 0;
glGenSamplers(1, &sampler_state);
glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_REPEAT);
glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_REPEAT);
glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glSamplerParameterf(sampler_state, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);

 Use of the sampler

GLuint texture_unit = 0;
glBindSampler(texture_unit, sampler_state);

When a sampler is no longer necessary on a texture unit, just bind the sampler zero:

glBindSampler(texture_unit, 0);
Sampler releasing:

glDeleteSamplers(1, &sampler_state);
			*/
		};
		class SIMUL_OPENGL_EXPORT Texture:public simul::crossplatform::Texture
		{
			GLuint m_fb;
			int main_viewport[4];
		public:
			Texture();
			~Texture();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files);
			bool IsValid() const;
			void InvalidateDeviceObjects();
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return NULL;
			}
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()
			{
				return NULL;
			}
			GLuint AsGLuint()
			{
				return pTextureObject;
			}
			void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0);
			void ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,crossplatform::PixelFormat f,bool computable) override;
			void ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int mips=1);
			void GenerateMips(crossplatform::DeviceContext &deviceContext);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			void activateRenderTarget(crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			int GetLength() const;
			int GetWidth() const;
			int GetDimension() const;
			int GetSampleCount() const;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels);
			GLuint pTextureObject;

			// Former Texture functions
			void setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d);
		};
	}
}
