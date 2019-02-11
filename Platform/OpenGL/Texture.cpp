#define NOMINMAX

#include "Texture.h"
#include "RenderPlatform.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>

using namespace simul;
using namespace opengl;

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
	glDeleteTextures(num,t);
}

SamplerState::SamplerState():
	mSamplerID(0)
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
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_S, opengl::RenderPlatform::toGLWrapping(desc->x));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_T, opengl::RenderPlatform::toGLWrapping(desc->y));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_WRAP_R, opengl::RenderPlatform::toGLWrapping(desc->z));

	// Filtering:
	glSamplerParameteri(mSamplerID, GL_TEXTURE_MIN_FILTER, opengl::RenderPlatform::toGLMinFiltering(desc->filtering));
	glSamplerParameteri(mSamplerID, GL_TEXTURE_MAG_FILTER, opengl::RenderPlatform::toGLMaxFiltering(desc->filtering));
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

Texture::Texture():
	mTextureID(0),
	mCubeArrayView(0)
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

void Texture::LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8)
{
	InvalidateDeviceObjects();

	// Load from file:
	LoadedTexture tdata;
	for (auto& path : r->GetTexturePathsUtf8())
	{
		std::string mainPath = path + "/" + std::string(pFilePathUtf8);
		tdata = LoadTextureData(mainPath.c_str());
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
		renderPlatform=r;

	width		= tdata.x;
	length		= tdata.y;
	arraySize	= 1;
	mips		= 1 + int(floor(log2(width >= length ? width : length)));
	dim		 = 2;
	depth		= 1;
	cubemap	 = false;

	// TO-DO: we force textures to be 4 components (loading X8R8G8B8 returns 3 components
	// per pixel, so thats why we just override all to RGBA_8_UNORM)
	pixelFormat = crossplatform::PixelFormat::RGBA_8_UNORM;

	glGenTextures(1, &mTextureID);
	{
		glBindTexture(GL_TEXTURE_2D, mTextureID);
		glTexImage2D
		(
			GL_TEXTURE_2D, 0,
			opengl::RenderPlatform::ToGLExternalFormat(pixelFormat),
			tdata.x, tdata.y, 0,
			opengl::RenderPlatform::ToGLExternalFormat(pixelFormat),
			GL_UNSIGNED_BYTE,
			tdata.data
		);
		// Set default wrapping:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// Set default filter:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	// By default, generate mips:
	crossplatform::DeviceContext dc;
	GenerateMips(dc);

	glObjectLabel(GL_TEXTURE, mTextureID, -1, pFilePathUtf8);
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform* r, const std::vector<std::string>& texture_files,int m)
{
	InvalidateDeviceObjects();

	std::vector<LoadedTexture> loadedTextures(texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		for (auto& path : r->GetTexturePathsUtf8())
		{
			std::string mainPath = path + "/" + texture_files[i];
			loadedTextures[i] = LoadTextureData(mainPath.c_str());
			if (loadedTextures[i].data)
				break;
		}
	}
	
	width		= loadedTextures[0].x;
	length		= loadedTextures[0].y;
	arraySize	= loadedTextures.size();
	mips		= std::min(m,1 + int(floor(log2(width >= length ? width : length))));
	dim		 = 2;
	depth		= 1;
	cubemap	 = false;
	mInternalGLFormat = opengl::RenderPlatform::ToGLFormat(crossplatform::PixelFormat::RGBA_8_UNORM);
	
	renderPlatform=r;

	glGenTextures(1, &mTextureID);
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, mTextureID);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 8, GL_RGBA8, width, length, loadedTextures.size());
		for (unsigned int i = 0; i < loadedTextures.size(); i++)
		{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, length, 1, GL_RGBA, GL_UNSIGNED_BYTE, loadedTextures[i].data);
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
	crossplatform::DeviceContext dc;
	GenerateMips(dc);

	glObjectLabel(GL_TEXTURE, mTextureID, -1, texture_files[0].c_str());

	InitViews(mips, arraySize, true);
	
	// CreateFBOs(1);
}

bool Texture::IsValid()const
{
	return mTextureID != 0;
}

void Texture::InvalidateDeviceObjects()
{
	for(auto u:residentHandles)
	{
		glMakeTextureHandleNonResidentARB(u);
	}
	residentHandles.clear();
	for(auto i:mTextureFBOs)
	{
		glDeleteFramebuffers(i.size(),i.data());
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
	//renderPlatform->
	auto *rp=(opengl::RenderPlatform*)renderPlatform;
	if(rp)
	{
		//rp->ClearResidentTextures();
		//rp->DeleteGLTextures(toDeleteTextures);
	}
	for(auto t:toDeleteTextures)
	{
		glDeleteTextures(1,&t);
	}

    mCubeArrayView = 0;
	mTextureID = 0;
}

void Texture::MakeHandleResident(GLuint64 thandle)
{
	if(!renderPlatform)
		return;
	if(residentHandles.find(thandle)!=residentHandles.end())
		return;
	glMakeTextureHandleResidentARB(thandle);
	residentHandles.insert(thandle);
}

void Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* t, void* srv,int w,int l,crossplatform::PixelFormat f, bool make_rt /*= false*/, bool setDepthStencil /*= false*/,bool need_srv /*= true*/,int numOfSamples)
{
	float qw, qh;
	GLuint gt=GLuint(uintptr_t(t));
	glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_WIDTH, &qw);
	glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_HEIGHT, &qh);

	if (mTextureID == 0 || mTextureID != gt|| qw != width || qh != length)
	{
		renderPlatform=r;
		InitViews(1, 1, false);

		mTextureID				= gt;
		external_texture		=true;
		mMainMipViews[0]		= mTextureID;
		mLayerMipViews[0][0]	= mMainMipViews[0];	 

		dim		 = 2;
		cubemap	 = false;
		arraySize	= 1;
		width		= int(qw);
		length		= int(qh);
		depth		= 1;
		mNumSamples = numOfSamples;
	}
}

bool Texture::ensureTexture2DSizeAndFormat( crossplatform::RenderPlatform* r, int w, int l,
											crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool depthstencil /*= false*/, int num_samples /*= 1*/, int aa_quality /*= 0*/, bool wrap /*= false*/,
											vec4 clear /*= vec4(0.5f, 0.5f, 0.2f, 1.0f)*/, float clearDepth /*= 1.0f*/, uint clearStencil /*= 0*/)
{
	if (!IsSame(w, l, 1, 1, 1, num_samples))
	{
		InvalidateDeviceObjects();
		renderPlatform=r;
// TODO: SUpport higher mips.
int m=1;
		width		= w;
		length		= l;
		mips		= m;
		arraySize	= 1;
		depth		= 1;
		dim		 = 2;
		pixelFormat = f;
		cubemap	 = false;
		mInternalGLFormat = opengl::RenderPlatform::ToGLFormat(f);
		mNumSamples = num_samples;

		glGenTextures(1, &mTextureID);
		glBindTexture(num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE, mTextureID);
		if (num_samples == 1)
		{
			glTextureStorage2D(mTextureID, 1, mInternalGLFormat, w, l);
			SetDefaultSampling(mTextureID);
		}
		else
		{
			glTextureStorage2DMultisample(mTextureID, num_samples, mInternalGLFormat, w, l, GL_TRUE);
		}
		glBindTexture(num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE, 0);
		SetName(name.c_str());

		InitViews(1, 1, rendertarget);

		// Layer view:
		{

		}
		std::string viewName;
		// Mip views:
		{
			glGenTextures(m, mMainMipViews.data());
			GLenum target = num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
			for (int mip = 0; mip < m; mip++)
			{
				glTextureView(mMainMipViews[mip], target, mTextureID, mInternalGLFormat, mip, 1, 0, arraySize);

				// Debug name:
				viewName = name + "_mip_" + std::to_string(mip);
				SetGLName(viewName.c_str(), mMainMipViews[mip]);
			}
		}
		// Layer mip views:
		{
			for (int i = 0; i < arraySize; i++)
			{
				glGenTextures(m, mLayerMipViews[i].data());
				GLenum target = num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
				for (int mip = 0; mip < m; mip++)
				{
					glTextureView(mLayerMipViews[i][mip], target, mTextureID, mInternalGLFormat, mip, 1, i, 1);
					viewName = name + "_layer_" + std::to_string(i) + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mLayerMipViews[i][mip]);
				}
			}
		}
		// Mip views:
		{
		//	mMainMipViews[0] = mTextureID;
		}

		// Layer mip:
		{
		//	mLayerMipViews[0][0] = mMainMipViews[0];
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

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int nmips, crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool ascubemap /*= false*/)
{
	int totalCnt = ascubemap ? 6 * num : num;
	if (!IsSame(w, l, 1, totalCnt, nmips, 1))
	{
		InvalidateDeviceObjects();
		renderPlatform=r;

		width		= w;
		length		= l;
		mips		= nmips;
		arraySize	= 1;
		depth		= 1;
		dim		 = 2;
		pixelFormat = f;
		cubemap	 = ascubemap;

		mInternalGLFormat	= opengl::RenderPlatform::ToGLFormat(f);
		
		glGenTextures(1, &mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, mCubeArrayView);
		glTextureStorage3D(mCubeArrayView, mips, mInternalGLFormat, width, length, totalCnt);
		SetDefaultSampling(mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		SetGLName((name +	"_arrayview").c_str(), mCubeArrayView);

		InitViews(nmips, totalCnt, rendertarget);

		// Main view:
		{
			// We need to set the proper target:
			if (cubemap)
			{
				glGenTextures(1, &mTextureID);
				glTextureView
				(
					mTextureID, 
					num <= 1 ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_CUBE_MAP_ARRAY, 
					mCubeArrayView, mInternalGLFormat,
					0, nmips, 
					0, totalCnt
				);

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
			glGenTextures(totalCnt, mLayerViews.data());
			for (int i = 0; i < totalCnt; i++)
			{
				glTextureView(mLayerViews[i], GL_TEXTURE_2D, mCubeArrayView, mInternalGLFormat, 0, nmips, i, 1);
				
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
				GLenum target = GL_TEXTURE_CUBE_MAP;
				if (num > 1)
				{
					target = GL_TEXTURE_CUBE_MAP_ARRAY;
				}
				for (int mip = 0; mip < nmips; mip++)
				{
					glTextureView(mMainMipViews[mip], target, mCubeArrayView, mInternalGLFormat, mip, 1, 0, totalCnt);
					
					// Debug name:
					viewName = name + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mMainMipViews[mip]);
				}
			}
			else
			{
				glGenTextures(nmips, mMainMipViews.data());
				GLenum target = GL_TEXTURE_2D_ARRAY;
				for (int mip = 0; mip < nmips; mip++)
				{
					glTextureView(mMainMipViews[mip], target, mCubeArrayView, mInternalGLFormat, mip, 1, 0, totalCnt);


					// Debug name:
					viewName = name + "_mip_" + std::to_string(mip);
					SetGLName(viewName.c_str(), mMainMipViews[mip]);
				}
			}
		}

		// Layer mip views:
		{
			for (int i = 0; i < totalCnt; i++)
			{
				glGenTextures(nmips, mLayerMipViews[i].data());
				for (int mip = 0; mip < nmips; mip++)
				{
					glTextureView(mLayerMipViews[i][mip], GL_TEXTURE_2D, mCubeArrayView, mInternalGLFormat, mip, 1, i, 1);


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

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int d, crossplatform::PixelFormat frmt, bool computable /*= false*/, int nmips /*= 1*/, bool rendertargets /*= false*/)
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
		dim		 = 3;
		pixelFormat = frmt;
		cubemap	 = false;

		mInternalGLFormat = opengl::RenderPlatform::ToGLFormat(frmt);

		glGenTextures(1, &mTextureID);
		glBindTexture(GL_TEXTURE_3D, mTextureID);
		glTextureStorage3D(mTextureID, mips, mInternalGLFormat, width, length, depth);
		SetDefaultSampling(mTextureID);
		glBindTexture(GL_TEXTURE_3D, 0);
		SetName(name.c_str());

		InitViews(nmips,1, false);

		std::string viewName;

		// Layer views:
		{

		}

		// Mip views:
		{
			glGenTextures(nmips, mMainMipViews.data());
			for (int mip = 0; mip < nmips; mip++)
			{
				glTextureView(mMainMipViews[mip], GL_TEXTURE_3D, mTextureID, mInternalGLFormat, mip, 1, 0, 1);

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

void Texture::ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear)
{

}

void Texture::GenerateMips(crossplatform::DeviceContext& deviceContext)
{
	if (IsValid())
	{
		glGenerateTextureMipmap(mTextureID);
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	if (dim == 2)
	{
		glTextureSubImage2D(mTextureID, 0, 0, 0, width, length, RenderPlatform::ToGLExternalFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
	else if (dim == 3)
	{
		glTextureSubImage3D(mTextureID, 0, 0, 0, 0, width, length, depth, RenderPlatform::ToGLExternalFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
}

void Texture::activateRenderTarget(crossplatform::DeviceContext& deviceContext, int array_index /*= -1*/, int mip_index /*= 0*/)
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
		arraySize = cubemap ? 6 : 1;
		CreateFBOs(mNumSamples);
	}
	targetsAndViewport.num				= 1;
	targetsAndViewport.m_rt[0]			= (void*)mTextureFBOs[array_index][mip_index];
	targetsAndViewport.m_dt				= nullptr;
	targetsAndViewport.viewport.x		= 0;
	targetsAndViewport.viewport.y		= 0;
	targetsAndViewport.viewport.w		= std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h		= std::max(1, (length >> mip_index));
		
	// Activate the render target and set the viewport:
	GLuint id = GLuint(uintptr_t(targetsAndViewport.m_rt[0]));
	glBindFramebuffer(GL_FRAMEBUFFER, id);
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Texture::deactivateRenderTarget(crossplatform::DeviceContext& deviceContext)
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

	int realArray	= GetArraySize();
	bool no_array	= !cubemap && (arraySize <= 1);
	bool isUAV		= (crossplatform::ShaderResourceType::RW & type) == crossplatform::ShaderResourceType::RW;

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
		return mLayerViews[layer];
	}
	// Mip view:
	if (layer < 0)
	{
		return mMainMipViews[mip];
	}
	// Layer mip view:
	return mLayerMipViews[layer][mip];
}

GLuint Texture::GetGLMainView()
{
	return mTextureID;
}

LoadedTexture Texture::LoadTextureData(const char* path)
{
	LoadedTexture lt	= {0,0,0,0,nullptr};
	lt.data			 = stbi_load(path, &lt.x, &lt.y, &lt.n, 4);
	/*if (!lt.data)
	{
		SIMUL_CERR << "Failed to load the texture: " << path << std::endl;
	}*/
	return lt;
}

bool Texture::IsSame(int w, int h, int d, int arr, int m, int msaa)
{
	// If we are not created yet...
	if (mTextureID == 0)
	{
		return false;
	}

	if (w != width || h != length || d != depth || m != mips)
	{
		return false;
	}

	return true;
}

void Texture::InitViews(int mipCount, int layers, bool isRenderTarget)
{
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
	for (int i = 0; i < arraySize; i++)
	{
		for (int mip = 0; mip < mips; mip++)
		{
			glGenFramebuffers(1, &mTextureFBOs[i][mip]);
			glBindFramebuffer(GL_FRAMEBUFFER, mTextureFBOs[i][mip]);
			if (sampleCount == 1)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mLayerMipViews[i][mip], 0);
			}
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mLayerMipViews[i][mip], 0);
			}
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
	glTextureParameteri(texId, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(texId, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTextureParameteri(texId, GL_TEXTURE_WRAP_R, GL_REPEAT);
	
	// Filter:
	glTextureParameteri(texId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::SetGLName(const char* n, GLuint id)
{
	if (mTextureID != 0)
	{
		glObjectLabel(GL_TEXTURE, id, -1, n);
	}
}
