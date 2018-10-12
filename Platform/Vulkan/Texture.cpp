#define NOMINMAX

#include "Texture.h"
#include "RenderPlatform.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>

using namespace simul;
using namespace vulkan;

void DeleteTextures(size_t num,GLuint *t)
{
	if(!t||!num)
		return;
	for(int i=0;i<num;i++)
	{
//		if(t[i]!=0&&!glIsTexture(t[i]))
		{
	//		SIMUL_BREAK_ONCE("Not a texture");
		}
	}
	//glDeleteTextures(num,t);
}

// TODO: This is ridiculous. But GL, at least in the current NVidia implementation, seems unable to write to a re-used texture id that it has generated after that id 
// was previously freed. Bad bug.
// Therefore we force the issue by making the tex id's go up sequentially, not standard GL behaviour:
void glGenTextures_DONT_REUSE(int count,GLuint *tex)
{
	//glGenTextures(count,tex);
}

SamplerState::SamplerState()
	//mSamplerID(0)
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::Init(crossplatform::SamplerStateDesc* desc)
{
	InvalidateDeviceObjects();
	
}

void SamplerState::InvalidateDeviceObjects()
{
}

Texture::Texture()
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
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform* r, const std::vector<std::string>& texture_files,int m)
{
	InvalidateDeviceObjects();

	std::vector<LoadedTexture> loadedTextures(texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		std::string mainPath	= r->GetTexturePathsUtf8()[0] + "/" + texture_files[i];
		loadedTextures[i]		= LoadTextureData(mainPath.c_str());
	}
	
	width		= loadedTextures[0].x;
	length		= loadedTextures[0].y;
	arraySize	= loadedTextures.size();
	mips		= std::min(m,1 + int(floor(log2(width >= length ? width : length))));
	dim		 = 2;
	depth		= 1;
	cubemap	 = false;
	InitViews(mips, arraySize, true);
	
	// CreateFBOs(1);
}

bool Texture::IsValid()const
{
	return false;
}

void Texture::InvalidateDeviceObjects()
{
}

void Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* renderPlatform, void* t, void* srv, bool make_rt /*= false*/, bool setDepthStencil /*= false*/,bool need_srv /*= true*/)
{
	float qw, qh;
}

bool Texture::ensureTexture2DSizeAndFormat( crossplatform::RenderPlatform* renderPlatform, int w, int l,
											crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool depthstencil /*= false*/, int num_samples /*= 1*/, int aa_quality /*= 0*/, bool wrap /*= false*/,
											vec4 clear /*= vec4(0.5f, 0.5f, 0.2f, 1.0f)*/, float clearDepth /*= 1.0f*/, uint clearStencil /*= 0*/)
{
	return false;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int num, int nmips, crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool ascubemap /*= false*/)
{

	return false;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int d, crossplatform::PixelFormat frmt, bool computable /*= false*/, int nmips /*= 1*/, bool rendertargets /*= false*/)
{
	if (!IsSame(w, l, d, 1, nmips, 1))
	{
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
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	if (dim == 2)
	{
	}
	else if (dim == 3)
	{
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

	targetsAndViewport.num				= 1;
//	targetsAndViewport.m_rt[0]			= (void*)mTextureFBOs[array_index][mip_index];
	targetsAndViewport.m_dt			 = nullptr;
	targetsAndViewport.viewport.x		= 0;
	targetsAndViewport.viewport.y		= 0;
	targetsAndViewport.viewport.w		= std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h		= std::max(1, (length >> mip_index));

	// Activate the render target and set the viewport:
	GLuint id = GLuint(uintptr_t(targetsAndViewport.m_rt[0]));
	//glBindFramebuffer(GL_FRAMEBUFFER, id);
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
	return 0;
}

bool Texture::IsComputable()const
{
	return true;
}

void Texture::copyToMemory(crossplatform::DeviceContext& deviceContext, void* target, int start_texel, int num_texels)
{

}

uint Texture::AsVulkanView(crossplatform::ShaderResourceType type, int layer, int mip, bool rw)
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


LoadedTexture Texture::LoadTextureData(const char* path)
{
	LoadedTexture lt	= {0,0,0,0,nullptr};
	lt.data			 = stbi_load(path, &lt.x, &lt.y, &lt.n, 4);
	if (!lt.data)
	{
		SIMUL_CERR << "Failed to load the texture: " << path << std::endl;
	}
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
		}
	}
}

void Texture::SetDefaultSampling(GLuint texId)
{
}

void Texture::SetGLName(const char* n, GLuint id)
{
	if (mTextureID != 0)
	{
	}
}
