
#include "Texture.h"
#include "RenderPlatform.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringFunctions.h"
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
	/*if (tdata.n == 4)
	{
		pixelFormat = crossplatform::PixelFormat::RGBA_8_UNORM;
	}
	else if (tdata.n == 3)
	{
		pixelFormat = crossplatform::PixelFormat::RGB_8_UNORM;
	}
	else if (tdata.n == 2)
	{
		pixelFormat = crossplatform::PixelFormat::RG_8_UNORM;
	}
	else if (tdata.n == 1)
	{
		pixelFormat = crossplatform::PixelFormat::R_8_UNORM;
	}
	else
	{
		SIMUL_BREAK("");
	}*/
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
	mGLFormat			= opengl::RenderPlatform::ToGLFormat(pixelFormat);
	mInternalGLFormat	= opengl::RenderPlatform::ToGLInternalFormat(pixelFormat);

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

	glObjectLabel(GL_TEXTURE, mTextureID, -1, pFilePathUtf8);
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
	mInternalGLFormat	= opengl::RenderPlatform::ToGLInternalFormat(crossplatform::PixelFormat::RGBA_8_UNORM);
	mGLFormat			= opengl::RenderPlatform::ToGLFormat(crossplatform::PixelFormat::RGBA_8_UNORM);
	
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

	glObjectLabel(GL_TEXTURE, mTextureID, -1, texture_files[0].c_str());

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
	for(auto u:residentHandles)
	{
		glMakeTextureHandleNonResidentARB(u);
	}
	residentHandles.clear();

	for(auto i:mTextureFBOs)
	{
		glDeleteFramebuffers((GLsizei)i.size(),i.data());
	}
	mTextureFBOs.clear();

	std::set<GLuint> toDeleteTextures;
	for (auto textureView : mTextureViews)
	{
		if (textureView.second && textureView.second != mTextureID)
			toDeleteTextures.insert(textureView.second);
	}
	mTextureViews.clear();

	if (mCubeArrayView != 0 && mCubeArrayView != mTextureID)
	{
		//OpenGL weirdness here: Deleting this texture id can
		// cause random crashes, when generating new TextureViews.
		//toDeleteTextures.insert(mCubeArrayView);
	}
	toDeleteTextures.insert(mTextureID);

	for(auto t:toDeleteTextures)
	{
		if(external_texture&&t==mTextureID)
			toDeleteTextures.erase(t);
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

bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* t, int w, int l, crossplatform::PixelFormat f, bool make_rt, bool setDepthStencil, int numOfSamples)
{
	float qw, qh;
	GLuint gt=GLuint(uintptr_t(t));
	glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_WIDTH, &qw);
	glGetTextureLevelParameterfv(gt, 0, GL_TEXTURE_HEIGHT, &qh);

	if (mTextureID == 0 || mTextureID != gt || qw != width || qh != length)
	{
		renderPlatform=r;
		InitViews(1, 1, false);

		mTextureID				= gt;
		external_texture		= true;

		dim			= 2;
		cubemap		= false;
		arraySize	= 1;
		width		= int(qw);
		length		= int(qh);
		depth		= 1;
		mNumSamples = numOfSamples;
        pixelFormat = f;
        mGLFormat = opengl::RenderPlatform::ToGLFormat(f);
        mInternalGLFormat = opengl::RenderPlatform::ToGLInternalFormat(f);
	}
	return true;
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
		mGLFormat			= opengl::RenderPlatform::ToGLFormat(f);
		mInternalGLFormat	= opengl::RenderPlatform::ToGLInternalFormat(f);
		mNumSamples			= num_samples;

		glGenTextures(1, &mTextureID);
		GLenum target = num_samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
		glBindTexture(target, mTextureID);
		if (num_samples == 1)
		{
			glTextureStorage2D(mTextureID, 1, mInternalGLFormat, w, l);
			SetDefaultSampling(mTextureID);
		}
		else
		{
			glTextureStorage2DMultisample(mTextureID, num_samples, mInternalGLFormat, w, l, GL_TRUE);
		}
		glBindTexture(target, 0);
		SetName(name.c_str());

		InitViews(1, 1, rendertarget);

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

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int nmips, crossplatform::PixelFormat f, bool computable, bool rendertarget, bool ascubemap)
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

		mInternalGLFormat	= opengl::RenderPlatform::ToGLInternalFormat(f);
		
		glGenTextures(1, &mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, mCubeArrayView);
		glTextureStorage3D(mCubeArrayView, mips, mInternalGLFormat, width, length, totalCount);
		SetDefaultSampling(mCubeArrayView);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		SetGLName((name + "_arrayview").c_str(), mCubeArrayView);

		InitViews(mips, totalCount, rendertarget);

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
					0, totalCount
				);

				SetGLName((name + "_cubeview").c_str(), mTextureID);
			}
			else
			{
				mTextureID = mCubeArrayView;
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

		mInternalGLFormat = opengl::RenderPlatform::ToGLInternalFormat(frmt);

		glGenTextures(1, &mTextureID);
		glBindTexture(GL_TEXTURE_3D, mTextureID);
		glTextureStorage3D(mTextureID, mips, mInternalGLFormat, width, length, depth);
		SetDefaultSampling(mTextureID);
		glBindTexture(GL_TEXTURE_3D, 0);
		SetName(name.c_str());

		InitViews(nmips, 1, false);

		return true;
	}

	return false;
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{

}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext)
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
		glTextureSubImage2D(mTextureID, 0, 0, 0, width, length, RenderPlatform::ToGLFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
	else if (dim == 3)
	{
		glTextureSubImage3D(mTextureID, 0, 0, 0, 0, width, length, depth, RenderPlatform::ToGLFormat(pixelFormat), RenderPlatform::DataType(pixelFormat), src);
	}
}

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::TextureView textureView)
{
	const int& array_index = textureView.subresourceRange.baseArrayLayer;
	const int& mip_index = textureView.subresourceRange.baseMipLevel;
	
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

GLuint Texture::AsOpenGLView(crossplatform::TextureView textureView)
{
	if (!ValidateTextureView(textureView))
		return 0;

	uint64_t hash = textureView.GetHash();
	if (mTextureViews.find(hash) != mTextureViews.end())
		return mTextureViews[hash];

	uint32_t baseMipLevel = textureView.subresourceRange.baseMipLevel;
	uint32_t mipLevelCount = textureView.subresourceRange.mipLevelCount == -1 ? mips : textureView.subresourceRange.mipLevelCount;
	uint32_t baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	uint32_t arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() : textureView.subresourceRange.arrayLayerCount;

	const crossplatform::ShaderResourceType& type = textureView.type;
	GLenum target = 0;
	if (type == crossplatform::ShaderResourceType::TEXTURE_1D || type == crossplatform::ShaderResourceType::RW_TEXTURE_1D)
		target = GL_TEXTURE_1D;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY || type == crossplatform::ShaderResourceType::RW_TEXTURE_1D_ARRAY)
		target = GL_TEXTURE_1D_ARRAY;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2D || type == crossplatform::ShaderResourceType::RW_TEXTURE_2D)
		target = GL_TEXTURE_2D;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY || type == crossplatform::ShaderResourceType::RW_TEXTURE_2D_ARRAY)
		target = GL_TEXTURE_2D_ARRAY;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
		target = GL_TEXTURE_2D_MULTISAMPLE;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
		target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_3D || type == crossplatform::ShaderResourceType::RW_TEXTURE_3D)
		target = GL_TEXTURE_3D;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_CUBE)
		target = GL_TEXTURE_CUBE_MAP;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
		target = GL_TEXTURE_CUBE_MAP_ARRAY;
	else
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");

	GLint status;
	glGetTextureParameteriv(mTextureID, GL_TEXTURE_IMMUTABLE_FORMAT, &status);
	if (status == GL_FALSE)
		return mTextureID;

	GLuint imageView;
	glGenTextures(1, &imageView);
	glTextureView(imageView, target, mTextureID, mInternalGLFormat, baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount);
	SetGLName(std::string(name + "_TextureView").c_str(), imageView);
	
	mTextureViews[hash] = imageView;
	return imageView;
}

GLuint Texture::GetGLMainView()
{
	return mTextureID;
}

 void Texture::LoadTextureData(LoadedTexture &lt,const char* path)
{
	lt	= {0, 0, 0, 0, nullptr};
	const auto& pathsUtf8 = renderPlatform->GetTexturePathsUtf8();
	int index = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(path, pathsUtf8);
	std::string filenameInUseUtf8 = path;
	if (index == -2 || index >= (int)pathsUtf8.size())
	{
		errno = 0;
		std::string file;
		std::vector<std::string> split_path = platform::core::SplitPath(path);
		if (split_path.size() > 1)
		{
			file = split_path[1];
			index = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(file.c_str(), pathsUtf8);
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
	platform::core::FileLoader::GetFileLoader()->AcquireFileContents(buffer, size, filenameInUseUtf8.c_str(), false);
	//void *data			 = stbi_load(filenameInUseUtf8.c_str(), &x, &y, &n, 4);
	if (!buffer)
	{
		SIMUL_CERR << "Failed to load the texture: " << filenameInUseUtf8.c_str() << std::endl;
		return;
	}
	void* data = nullptr;
	TranslateLoadedTextureData(data, buffer, size, x, y, n, 4);
	platform::core::FileLoader::GetFileLoader()->ReleaseFileContents(buffer);
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
		SIMUL_BREAK("OpenGL nVidia driver cannot cope with compute writing/reading 16 bit float textures.");
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
			crossplatform::ShaderResourceType srt = sampleCount == 1 ? crossplatform::ShaderResourceType::TEXTURE_2D : crossplatform::ShaderResourceType::TEXTURE_2DMS;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, AsOpenGLView({ srt, {mip, 1, i, 1} }), 0);
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
