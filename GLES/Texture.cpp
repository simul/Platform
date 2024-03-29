
#include "Texture.h"
#include "RenderPlatform.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringFunctions.h"
#include <algorithm>
#define GLFW_INCLUDE_ES31
#include <GLFW/glfw3.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using namespace simul;
using namespace gles;

void DeleteTextures(size_t num,GLuint *t)
{
	if(!t||!num)
		return;
	for(int i=0;i<num;i++)
	{
		if(t[i]!=0&&!glIsTexture(t[i]))
		{
			SIMUL_BREAK_ONCE("Not a texture");
		}
	}
	glDeleteTextures((GLsizei)num,t);
}

SamplerState::SamplerState()
	:mSamplerID(0)
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::Init(crossplatform::SamplerStateDesc* desc)
{
	InvalidateDeviceObjects();
	
	glGenSamplers(1, &mSamplerID);

	// Wrapping:
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_S, gles::RenderPlatform::toGLWrapping(desc->x));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_T, gles::RenderPlatform::toGLWrapping(desc->y));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_R, gles::RenderPlatform::toGLWrapping(desc->z));

	// Filtering:
	glSamplerParameteri(mSamplerID, GL_TEXTURE_MIN_FILTER, gles::RenderPlatform::toGLMinFiltering(desc->filtering));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_MAG_FILTER, gles::RenderPlatform::toGLMaxFiltering(desc->filtering));
}

void SamplerState::InvalidateDeviceObjects()
{
	if (mSamplerID != 0)
	{
		glDeleteSamplers(1, &mSamplerID);
		mSamplerID = 0;
	}
}

GLuint SamplerState::asGLuint()
{
	return mSamplerID;
}

Texture::Texture()
	:mInternalGLFormat(0), mGLFormat(0), mTextureID(0), mCubeArrayView(0)
{
}

Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::SetName(const char* n)
{
	if (!n)
	{
		name = "texture";
	}
	else
	{
		name = n;
	}
}

void Texture::LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8, bool gen_mips)
{
	InvalidateDeviceObjects();
	renderPlatform = r;

	// Load from file:
	LoadedTexture tdata = { 0, 0, 0, 0, nullptr };
	for (auto& path : r->GetTexturePathsUtf8())
	{
		std::string mainPath = path + "/" + std::string(pFilePathUtf8);
		LoadTextureData(tdata ,mainPath.c_str());
		if (tdata.data)
			break;
	}
	if (!tdata.data)
	{
		SIMUL_CERR << "Failed to load the texture: " << pFilePathUtf8 << std::endl;
	}

	// Choose a format:
	if (tdata.n == 4)
	{
		pixelFormat = crossplatform::PixelFormat::RGBA_8_UNORM;
	}
	else if (tdata.n == 3)
	{
		pixelFormat = crossplatform::PixelFormat::RGB_8_UNORM;
	}
	else
	{
		SIMUL_BREAK("");
	}
	// TO-DO: we force textures to be 4 components (loading X8R8G8B8 returns 3 components
	// per pixel, so thats why we just override all to RGBA_8_UNORM)
	pixelFormat = crossplatform::PixelFormat::RGBA_8_UNORM;
	
	renderPlatform		= r;
	width				= tdata.x;
	length				= tdata.y;
	arraySize			= 1;
	mips				= 1 + (gen_mips ? int(floor(log2(width >= length ? width : length))) : 0);
	dim					= 2;
	depth				= 1;
	cubemap				= false;
	mGLFormat			= gles::RenderPlatform::ToGLFormat(pixelFormat);
	mInternalGLFormat	= gles::RenderPlatform::ToGLInternalFormat(pixelFormat);

	glGenTextures(1, &mTextureID);
	
	glBindTexture(GL_TEXTURE_2D, mTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, mInternalGLFormat, tdata.x, tdata.y, 0, mGLFormat, GL_UNSIGNED_BYTE, tdata.data);

	// Set default wrapping:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set default filter:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glBindTexture(GL_TEXTURE_2D, 0);

	// By default, generate mips:
	crossplatform::GraphicsDeviceContext dc;
	//if(gen_mips)
		GenerateMips(dc);

	glObjectLabelKHR(GL_TEXTURE, mTextureID, -1, pFilePathUtf8);
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform* r, const std::vector<std::string>& texture_files,bool gen_mips)
{
	InvalidateDeviceObjects();
	renderPlatform = r;

	std::vector<LoadedTexture> loadedTextures(texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		for (auto& path : r->GetTexturePathsUtf8())
		{
			std::string mainPath = path + "/" + texture_files[i];
			LoadTextureData(loadedTextures[i] ,mainPath.c_str());
			if (loadedTextures[i].data)
				break;
		}
	}
	
	width				= loadedTextures[0].x;
	length				= loadedTextures[0].y;
	arraySize			= (int)loadedTextures.size();
	mips				= gen_mips?std::min(16,1 + int(floor(log2(width >= length ? width : length)))):1;
	dim					= 2;
	depth				= 1;
	cubemap				= false;
	mInternalGLFormat	= gles::RenderPlatform::ToGLInternalFormat(crossplatform::PixelFormat::RGBA_8_UNORM);
	mGLFormat			= gles::RenderPlatform::ToGLFormat(crossplatform::PixelFormat::RGBA_8_UNORM);
	
	renderPlatform		= r;

	glGenTextures(1, &mTextureID);
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, mTextureID);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 8, GL_RGBA8, width, length,(GLsizei) loadedTextures.size());
		for (unsigned int i = 0; i < loadedTextures.size(); i++)
		{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, length, 1, mGLFormat, GL_UNSIGNED_BYTE, loadedTextures[i].data);
		}
		// Set default wrapping:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// Set default filter:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// By default, generate mips:
	crossplatform::GraphicsDeviceContext dc;
	GenerateMips(dc);

	glObjectLabelKHR(GL_TEXTURE, mTextureID, -1, texture_files[0].c_str());

	InitViews(mips, arraySize, true);
	for (unsigned int i = 0; i < loadedTextures.size(); i++)
	{
		FreeTranslatedTextureData( loadedTextures[i].data);
	}
	loadedTextures.clear();
	
	// CreateFBOs(1);
}

bool Texture::IsValid()const
{
	return mTextureID != 0;
}

void Texture::InvalidateDeviceObjects()
{
	for(auto i:mTextureFBOs)
	{
		glDeleteFramebuffers((GLsizei)i.size(),i.data());
	}
	mTextureFBOs.clear();
    std::set<GLuint> toDeleteTextures;
    for (auto &texIdVector: mLayerMipViews)
    {
        for (GLuint texId : texIdVector)
        {
            if (texId && texId != mTextureID)
                toDeleteTextures.insert(texId);
        }
        texIdVector.clear();
    }
    mLayerMipViews.clear();
    for (GLuint texId : mMainMipViews)
    {
        if (texId && texId != mTextureID)
            toDeleteTextures.insert(texId);
    }
    mMainMipViews.clear();
    for (GLuint texId : mLayerViews)
    {
        if (texId && texId != mTextureID)
            toDeleteTextures.insert(texId);
    }
    mLayerViews.clear();
    if (mCubeArrayView != 0 && mCubeArrayView != mTextureID)
    {
        toDeleteTextures.insert(mCubeArrayView);
    }
	toDeleteTextures.insert(mTextureID);
	for(auto t:toDeleteTextures)
	{
		if(external_texture&&t==mTextureID)
			toDeleteTextures.erase(t);
	}
	/*auto *rp=(gles::RenderPlatform*)renderPlatform;
	if(rp)
	{
		//rp->ClearResidentTextures();
		//rp->DeleteGLTextures(toDeleteTextures);
	}*/
	for(auto t:toDeleteTextures)
	{
		glDeleteTextures(1,&t);
	}

    mCubeArrayView = 0;
	mTextureID = 0;
}

void Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* t, void* srv, int w, int l, crossplatform::PixelFormat f, bool make_rt, bool setDepthStencil, bool need_srv, int numOfSamples)
{
	float qw=w, qh=l;
	GLuint gt=GLuint(uintptr_t(t));
	// GLES Doesn't support querying texture size:
	//	glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_WIDTH, &qw);
	//glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_HEIGHT, &qh);

	if (mTextureID == 0 || mTextureID != gt )//|| qw != width || qh != length)
	{
		renderPlatform=r;
		InitViews(1, 1, false);

		mTextureID				= gt;
		external_texture		= true;
		mMainMipViews[0]		= mTextureID;
		mLayerMipViews[0][0]	= mMainMipViews[0];

		dim			= 2;
		cubemap		= false;
		arraySize	= 1;
		width		= int(qw);
		length		= int(qh);
		depth		= 1;
		mNumSamples = numOfSamples;
	}
}

bool Texture::ensureTexture2DSizeAndFormat( crossplatform::RenderPlatform* r, int w, int l, int m,
											crossplatform::PixelFormat f, bool computable, bool rendertarget, bool depthstencil, int num_samples, int aa_quality, bool wrap,
											vec4 clear, float clearDepth, uint clearStencil)
{
	if (!IsSame(w, l, 1, 1, m, num_samples))
	{
		InvalidateDeviceObjects();
		renderPlatform=r;

		width				= w;
		length				= l;
		mips				= m; // TODO: Support higher mips.
		arraySize			= 1;
		depth				= 1;
		dim					= 2;
		pixelFormat			= f;
		cubemap				= false;
		mInternalGLFormat	= gles::RenderPlatform::ToGLInternalFormat(f);
		mNumSamples			= num_samples;

		glGenTextures(1, &mTextureID);
		GLenum target = num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
		glBindTexture(target, mTextureID);
		if (num_samples == 1)
		{
			glTexImage2D( target, 0, mInternalGLFormat, w, l,0,mGLFormat,GL_RGBA,nullptr);
			SetDefaultSampling(mTextureID);
		}
		else
		{
			glTexImage2D(target, 0, mInternalGLFormat, w, l, 0, mGLFormat, GL_RGBA, nullptr);
			//glTexStorageMem2DMultisampleEXT( target, num_samples, mInternalGLFormat, w, l, GL_TRUE,0,0);
		}
		glBindTexture(target, 0);
		SetName(name.c_str());

		InitViews(1, 1, rendertarget);

		// Layer view:
		{

		}
		std::string viewName;
		// Mip views:
		{
			glGenTextures(mips, mMainMipViews.data());
			for (int mip = 0; mip < mips; mip++)
			{
			//	glTextureViewEXT(mMainMipViews[mip], target, mTextureID, mInternalGLFormat, mip, 1, 0, arraySize);

				// Debug name:
				viewName = name + "_mip_" + std::to_string(mip);
				SetGLName(viewName.c_str(), mMainMipViews[mip]);
			}
		}
		// Layer mip views:
		{
			for (int i = 0; i < arraySize; i++)
			{
				glGenTextures(mips, mLayerMipViews[i].data());
				for (int mip = 0; mip < mips; mip++)
				{
				//	glTextureViewEXT(mLayerMipViews[i][mip], target, mTextureID, mInternalGLFormat, mip, 1, i, 1);
					viewName = name + "_layer_" + std::to_string(i) + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mLayerMipViews[i][mip]);
				}
			}
		}

		// Render targets
		if (rendertarget)
		{
			CreateFBOs(num_samples);
		}

		// Depth:
		if(depthStencil)
		{

		}

		return true;
	}

	return false;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int nmips, crossplatform::PixelFormat f, bool computable, bool rendertarget, bool depthstencil, bool ascubemap)
{
	int totalCount = ascubemap ? 6 * num : num;
	if (!IsSame(w, l, 1, num, nmips, 1))
	{
		InvalidateDeviceObjects();
		renderPlatform=r;

		width		= w;
		length		= l;
		mips		= nmips;
		arraySize	= num;
		depth		= 1;
		dim			= 2;
		pixelFormat = f;
		cubemap		= ascubemap;

		mInternalGLFormat	= gles::RenderPlatform::ToGLInternalFormat(f);
		
		glGenTextures(1, &mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, mCubeArrayView);
		//glTextureStorage3DEXT(mCubeArrayView, GL_TEXTURE_2D_ARRAY, mips, mInternalGLFormat, width, length, totalCount);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, mInternalGLFormat, width, length, totalCount,0,mGLFormat,GL_RGBA,nullptr);
		SetDefaultSampling(mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		SetGLName((name + "_arrayview").c_str(), mCubeArrayView);

		InitViews(mips, totalCount, rendertarget);

		// Main view:
		{
			// We need to set the proper target:
			if (cubemap)
			{
				mTextureID = mCubeArrayView;
			/*	glGenTextures(1, &mTextureID);
				glTextureViewEXT
				(
					mTextureID, 
					num <= 1 ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_CUBE_MAP_ARRAY, 
					mCubeArrayView, mInternalGLFormat,
					0, nmips, 
					0, totalCount
				);*/

				SetGLName((name + "_cubeview").c_str(), mTextureID);
			}
			else
			{
				mTextureID = mCubeArrayView;
			}
		}

		std::string viewName;

		// Layer view:
		{
			glGenTextures(totalCount, mLayerViews.data());
			for (int i = 0; i < totalCount; i++)
			{
			//	glTextureViewEXT(mLayerViews[i], GL_TEXTURE_2D, mCubeArrayView, mInternalGLFormat, 0, mips, i, 1);
				
				// Debug name:
				viewName = name + "_layer_" + std::to_string(i);
				SetGLName(viewName.c_str(), mLayerViews[i]);
			}
		}

		// Mip views:
		{
			if (cubemap)
			{
				glGenTextures(nmips, mMainMipViews.data());
				SIMUL_ASSERT(num ==1);
				GLenum target =  GL_TEXTURE_CUBE_MAP;
				//GLenum target = num > 1 ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
				for (int mip = 0; mip < mips; mip++)
				{
				//	glTextureViewEXT(mMainMipViews[mip], target, mCubeArrayView, mInternalGLFormat, mip, 1, 0, totalCount);
					
					// Debug name:
					viewName = name + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mMainMipViews[mip]);
				}
			}
			else
			{
				glGenTextures(mips, mMainMipViews.data());
				for (int mip = 0; mip < mips; mip++)
				{
				//	glTextureViewEXT(mMainMipViews[mip], GL_TEXTURE_2D_ARRAY, mCubeArrayView, mInternalGLFormat, mip, 1, 0, totalCount);

					// Debug name:
					viewName = name + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mMainMipViews[mip]);
				}
			}
		}

		// Layer mip views:
		{
			for (int i = 0; i < totalCount; i++)
			{
				glGenTextures(mips, mLayerMipViews[i].data());
				for (int mip = 0; mip < mips; mip++)
				{
				//	glTextureViewEXT(mLayerMipViews[i][mip], GL_TEXTURE_2D, mCubeArrayView, mInternalGLFormat, mip, 1, i, 1);

					// Debug name:
					viewName = name + "_layer_" + std::to_string(i) + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mLayerMipViews[i][mip]);
				}
			}
		}

		return true;
	}

	return false;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int d, crossplatform::PixelFormat frmt, bool computable, int nmips, bool rendertargets)
{
	if (!IsSame(w, l, d, 1, nmips, 1))
	{
		InvalidateDeviceObjects();
		renderPlatform=r;

		width		= w;
		length		= l;
		mips		= nmips;
		arraySize	= 1;
		depth		= d;
		dim			= 3;
		pixelFormat = frmt;
		cubemap		= false;

		mInternalGLFormat = gles::RenderPlatform::ToGLInternalFormat(frmt);

		glGenTextures(1, &mTextureID);
		glBindTexture(GL_TEXTURE_3D, mTextureID);
		//glTextureStorage3DEXT(mTextureID, GL_TEXTURE_3D,mips, mInternalGLFormat, width, length, depth);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, mInternalGLFormat, width, length, d, 0, mGLFormat, GL_RGBA, nullptr);
		SetDefaultSampling(mTextureID);
		glBindTexture(GL_TEXTURE_3D, 0);
		SetName(name.c_str());

		InitViews(nmips, 1, false);

		std::string viewName;

		// Layer views:
		{

		}

		// Mip views:
		{
			glGenTextures(mips, mMainMipViews.data());
			for (int mip = 0; mip < mips; mip++)
			{
			//	glTextureViewEXT(mMainMipViews[mip], GL_TEXTURE_3D, mTextureID, mInternalGLFormat, mip, 1, 0, 1);

				// Debug name:
				viewName = name + "_mip_" + std::to_string(mip);
				SetGLName(viewName.c_str(), mMainMipViews[mip]);
			}
		}

		// Layer mips:
		{
			for (int mip = 0; mip < nmips; mip++)
			{
				// Same as mip views (we just have 1 layer):
				mLayerMipViews[0][mip] = mMainMipViews[mip];
			}
		}
	
		return true;
	}

	return false;
}

void Texture::ClearColour(crossplatform::GraphicsDeviceContext& deviceContext, vec4 colourClear)
{
	GLuint framebuffer = 0;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	GLenum texture_target = mNumSamples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, mTextureID, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		SIMUL_CERR << "FBO is not complete!\n";
	}

	glClearColor(colourClear.x, colourClear.y, colourClear.z, colourClear.w);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer);
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	GLuint framebuffer = 0;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	GLenum texture_target = mNumSamples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, mTextureID, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		SIMUL_CERR << "FBO is not complete!\n";
	}

	glDepthMask(GL_TRUE);
	glClearDepthf(depthClear);
	glClearStencil(stencilClear);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer);
}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (IsValid())
	{
		//glGenerateTextureMipmap(mTextureID);
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	if (dim == 2)
	{
	//	glTextureSubImage2D(mTextureID, 0, 0, 0, width, length, RenderPlatform::ToGLFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
	else if (dim == 3)
	{
	//	glTextureSubImage3D(mTextureID, 0, 0, 0, 0, width, length, depth, RenderPlatform::ToGLFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
}

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, int array_index, int mip_index)
{
	if (array_index == -1)
	{
		array_index = 0;
	}
	if (mip_index == -1)
	{
		mip_index = 0;
	}
	if (mTextureFBOs[array_index][mip_index] == 0)
	{
		CreateFBOs(mNumSamples);
	}

	targetsAndViewport.num				= 1;
	targetsAndViewport.m_rt[0]			= (crossplatform::ApiRenderTarget*)(uint64_t)mTextureFBOs[array_index][mip_index];
	targetsAndViewport.m_dt				= nullptr;
	targetsAndViewport.viewport.x		= 0;
	targetsAndViewport.viewport.y		= 0;
	targetsAndViewport.viewport.w		= std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h		= std::max(1, (length >> mip_index));
		
	// Activate the render target and set the viewport:
	GLuint id = GLuint(uintptr_t(targetsAndViewport.m_rt[0]));
	glBindFramebuffer(GL_FRAMEBUFFER, id);
	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

}

void Texture::deactivateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
}

int Texture::GetLength()const
{
	return cubemap ? arraySize * 6 : arraySize;
}

int Texture::GetWidth()const
{
	return width;
}

int Texture::GetDimension()const
{
	return dim;
}

int Texture::GetSampleCount()const
{
	return mNumSamples == 1 ? 0 : mNumSamples; //Updated for MSAA texture -AJR
}

bool Texture::IsComputable()const
{
	return true;
}

void Texture::copyToMemory(crossplatform::DeviceContext& deviceContext, void* target, int start_texel, int num_texels)
{

}

GLuint Texture::AsOpenGLView(crossplatform::ShaderResourceType type, int layer, int mip, bool rw)
{
	if (mip == mips)
	{
		mip = 0;
	}

	int realArray = GetArraySize();
	bool no_array = !cubemap && (arraySize <= 1);
	bool isUAV = (crossplatform::ShaderResourceType::RW & type) == crossplatform::ShaderResourceType::RW;

	auto bounds_check_return_texture_view = [&](const std::vector<GLuint>& arr, int idx) -> GLuint
	{
	#if _DEBUG
		if (idx > -1 && idx < arr.size())
			return arr[idx];
		else
			return mTextureID;
	#else
		return arr[idx];
	#endif

	};
	auto bounds_check_nested_return_texture_view = [&](const std::vector<std::vector<GLuint>>& arr, int idx0, int idx1) -> GLuint
	{
	#if _DEBUG
		if (idx0 > -1 && idx0 < arr.size())
		{
			if (idx1 > -1 && idx1 < arr[idx0].size())
			{
				return arr[idx0][idx1];
			}
			else
				return mTextureID;
		}
		else
			return mTextureID;
	#else
		return arr[idx0][idx1];
	#endif
	};

	// Base view:
	if ((mips <= 1 && no_array) || (layer < 0 && mip < 0))
	{
		if (cubemap && ((type & crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY) == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY))
		{
			return mCubeArrayView;
		}
		return mTextureID;
	}
	// Layer view:
	if (mip < 0 || mips <= 1)
	{
		if (layer < 0 || no_array)
		{
			return mTextureID;
		}
		return bounds_check_return_texture_view(mLayerViews,layer);
	}
	// Mip view:
	if (layer < 0)
	{
		return bounds_check_return_texture_view(mMainMipViews,mip);
	}
	// Layer mip view:
	return bounds_check_nested_return_texture_view(mLayerMipViews,layer,mip);
}

GLuint Texture::GetGLMainView()
{
	return mTextureID;
}

 void Texture::LoadTextureData(LoadedTexture &lt,const char* path)
{
	lt	= {0, 0, 0, 0, nullptr};
	const auto& pathsUtf8 = renderPlatform->GetTexturePathsUtf8();
	int index = simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(path, pathsUtf8);
	std::string filenameInUseUtf8 = path;
	if (index == -2 || index >= (int)pathsUtf8.size())
	{
		errno = 0;
		std::string file;
		std::vector<std::string> split_path = base::SplitPath(path);
		if (split_path.size() > 1)
		{
			file = split_path[1];
			index = simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(file.c_str(), pathsUtf8);
		}
		if (index < -1 || index >= (int)pathsUtf8.size())
		{
			SIMUL_CERR << "Failed to find texture file " << filenameInUseUtf8 << std::endl;
			return;
		}
		filenameInUseUtf8 = file;
	}
	if (index < renderPlatform->GetTexturePathsUtf8().size())
		filenameInUseUtf8 = (renderPlatform->GetTexturePathsUtf8()[index] + "/") + filenameInUseUtf8;

	int x, y, n;
	void* buffer = nullptr;
	unsigned size = 0;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(buffer, size, filenameInUseUtf8.c_str(), false);
	//void *data			 = stbi_load(filenameInUseUtf8.c_str(), &x, &y, &n, 4);
	if (!buffer)
	{
		SIMUL_CERR << "Failed to load the texture: " << filenameInUseUtf8.c_str() << std::endl;
		return;
	}
	void* data = nullptr;
	TranslateLoadedTextureData(data, buffer, size, x, y, n, 4);
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(buffer);
	lt.data = (unsigned char *)data;
	lt.x = x;
	lt.y = y;
	lt.n = n;
	if (!lt.data)
	{
#if _DEBUG
		SIMUL_CERR << "Failed to load the texture: " << path << std::endl;
#endif
		errno = 0; //ERRNO_CLEAR
	}
}

bool Texture::IsSame(int w, int h, int d, int arr, int m, int msaa)
{
	// If we are not created yet...
	if (mTextureID == 0)
	{
		return false;
	}

	if (w != width || h != length || d != depth || arr != arraySize || m != mips || msaa != mNumSamples)
	{
		return false;
	}

	return true;
}

void Texture::InitViews(int mipCount, int layers, bool isRenderTarget)
{
	if(computable&&pixelFormat==crossplatform::RGBA_16_FLOAT)
	{
		SIMUL_BREAK("GLES nVidia driver cannot cope with compute writing/reading 16 bit float textures.");
	}
	mLayerViews.resize(layers);

	mMainMipViews.resize(mipCount);

	mLayerMipViews.resize(layers);
	for (int i = 0; i < layers; i++)
	{
		mLayerMipViews[i].resize(mipCount);
	}

	if (isRenderTarget)
	{
		mTextureFBOs.resize(layers);
		for (int i = 0; i < layers; i++)
		{
			mTextureFBOs[i].resize(mipCount);
		}
	}
}

void Texture::CreateFBOs(int sampleCount)
{
	for (int i = 0; i < arraySize * (cubemap ? 6 : 1); i++)
	{
		for (int mip = 0; mip < mips; mip++)
		{
			glGenFramebuffers(1, &mTextureFBOs[i][mip]);
			glBindFramebuffer(GL_FRAMEBUFFER, mTextureFBOs[i][mip]);
			GLenum texture_target = sampleCount == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, mLayerMipViews[i][mip], 0);
			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				SIMUL_CERR << "FBO is not complete!\n";
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}
}

void Texture::SetDefaultSampling(GLuint texId)
{
	// Wrapping:
	//glTextureParameteri(texId, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTextureParameteri(texId, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//glTextureParameteri(texId, GL_TEXTURE_WRAP_R, GL_REPEAT);
	//
	//// Filter:
	//glTextureParameteri(texId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTextureParameteri(texId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::SetGLName(const char* n, GLuint id)
{
	if (mTextureID != 0)
	{
		glObjectLabelKHR(GL_TEXTURE, id, -1, n);
	}
}
