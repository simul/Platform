#include "BaseFramebuffer.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/RandomNumberGenerator.h"
using namespace simul;
using namespace crossplatform;

crossplatform::TargetsAndViewport BaseFramebuffer::defaultTargetsAndViewport;
// Change this from a static construtor to a dynamic construtor so that the memory is allocated by the application rather than the C runtime
std::stack<crossplatform::TargetsAndViewport*>& BaseFramebuffer::GetFrameBufferStack()
{
	static std::stack<crossplatform::TargetsAndViewport*> targetStack;
	return targetStack;
}

void BaseFramebuffer::setDefaultRenderTargets(const ApiRenderTarget* rt,
										const ApiDepthRenderTarget* dt,
										uint32_t viewportLeft,
										uint32_t viewportTop,
										uint32_t viewportRight,
										uint32_t viewportBottom
										)
{
	memset(&defaultTargetsAndViewport,0,sizeof(defaultTargetsAndViewport));
	defaultTargetsAndViewport.num=1;
	defaultTargetsAndViewport.m_rt[0] = rt;
	defaultTargetsAndViewport.m_rt[1] = nullptr;
	defaultTargetsAndViewport.m_rt[2] = nullptr;
	defaultTargetsAndViewport.m_rt[3] = nullptr;
	defaultTargetsAndViewport.m_dt = dt;
	defaultTargetsAndViewport.viewport.x = viewportLeft;
	defaultTargetsAndViewport.viewport.y = viewportTop;
	defaultTargetsAndViewport.viewport.w = viewportRight-viewportLeft;
	defaultTargetsAndViewport.viewport.h = viewportBottom-viewportTop;
	defaultTargetsAndViewport.viewport.znear=0.0f;
	defaultTargetsAndViewport.viewport.zfar=1.0f;
}

// Amortization: 0 = 1x1, 1=2x1, 2=1x2, 3=2x2, 4=3x2, 5=2x3, 6=3x3, etc.
void AmortizationStruct::setAmortization(int a)
{
	if(amortization==a)
		return;
	delete [] pattern;
	pattern=NULL;
	simul::math::RandomNumberGenerator rand;
	amortization=a;
	if(a<=1)
		return;
	uint3 xyz=scale();
	
	int sz=xyz.x*xyz.y;
	std::vector<uint3> src;
	src.reserve(sz);
	int n=0;
	for(unsigned i=0;i<xyz.x;i++)
	{
		for(unsigned j=0;j<xyz.y;j++)
		{
			uint3 v(i,j,0);
			src.push_back(v);
			n++;
		}
	}
	numOffsets=n;
	pattern=new uint3[numOffsets];
	for(int i=0;i<n;i++)
	{
		int idx=(int)rand.IRand((int)src.size());
		auto u=src.begin()+idx;
		uint3 v=*u;
		pattern[i]=v;
		src.erase(u);
	}
}

BaseFramebuffer::BaseFramebuffer(const char *n)
	:Width(0)
	,Height(0)
	,mips(0)
	,numAntialiasingSamples(1)	// no AA by default
	,depth_active(false)
	,colour_active(false)
	,renderPlatform(0)
	,buffer_texture(NULL)
	,buffer_depth_texture(NULL)
	,GenerateMips(false)
	,is_cubemap(false)
	,current_face(-1)
	,target_format(crossplatform::RGBA_32_FLOAT)
	,depth_format(crossplatform::UNKNOWN)
	,bands(4)
	,activate_count(0)
	,sphericalHarmonicsEffect(NULL)
	,external_texture(false)
	,external_depth_texture(false)
	,shSeed(0)
{
	if(n)
		name=n;
}

BaseFramebuffer::~BaseFramebuffer()
{
	InvalidateDeviceObjects();
}

void BaseFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	static int seed = 0;
	seed = seed % 1001;
	shSeed=seed++;
	if(renderPlatform)
	{
		if(!external_texture)
			buffer_texture=renderPlatform->CreateTexture("BaseFramebuffer");
		if(!external_depth_texture)
			buffer_depth_texture=renderPlatform->CreateTexture("BaseFramebuffer");
	}
	// The table of coefficients.
	int s=(bands+1);
	if(s<4)
		s=4;
	sphericalHarmonics.InvalidateDeviceObjects();
	sphericalSamples.InvalidateDeviceObjects();
	sphericalHarmonicsConstants.RestoreDeviceObjects(renderPlatform);
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
	CreateBuffers();
}

void BaseFramebuffer::InvalidateDeviceObjects()
{
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=NULL;
	buffer_depth_texture=NULL;
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	sphericalHarmonics.InvalidateDeviceObjects();
	SAFE_DELETE(sphericalHarmonicsEffect);
	sphericalSamples.InvalidateDeviceObjects();
}

void BaseFramebuffer::SetWidthAndHeight(int w,int h,int m)
{
	if(Width!=w||Height!=h||mips!=m)
	{
		Width=w;
		Height=h;
		mips=m;
		if(buffer_texture)
			buffer_texture->InvalidateDeviceObjects();
		if(buffer_depth_texture)
			buffer_depth_texture->InvalidateDeviceObjects();
	}
}

void BaseFramebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==target_format)
		return;
	target_format=f;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
}

void BaseFramebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
}

void BaseFramebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void BaseFramebuffer::SetAsCubemap(int w,int num_mips,crossplatform::PixelFormat f)
{
	SetWidthAndHeight(w,w,num_mips);
	SetFormat(f);
	is_cubemap=true;
}

void BaseFramebuffer::SetCubeFace(int f)
{
	if(!is_cubemap)
	{
		SIMUL_BREAK_ONCE("Setting cube face on non-cubemap framebuffer");
	}
	current_face=f;
}

bool BaseFramebuffer::IsDepthActive() const
{
	return depth_active;
}

bool BaseFramebuffer::IsColourActive() const
{
	return colour_active;
}

bool BaseFramebuffer::IsValid() const
{
	bool ok=(buffer_texture!=NULL)||(buffer_depth_texture!=NULL);
	return ok;
}

void BaseFramebuffer::SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth)
{
	if(buffer_texture==colour&&buffer_depth_texture==depth&&(!colour||(colour->width==Width&&colour->length==Height)))
		return;
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=colour;
	buffer_depth_texture=depth;

	external_texture=true;
	external_depth_texture=true;
	if(colour)
	{
		Width=colour->width;
		Height=colour->length;
		is_cubemap=(colour->depth==6);
	}
	else if(depth)
	{
		Width=depth->width;
		Height=depth->length;
	}
}

bool BaseFramebuffer::CreateBuffers()
{
	if(!Width||!Height)
		return false;
	if(!renderPlatform)
	{
		SIMUL_BREAK("renderPlatform should not be NULL here");
	}
	if(!renderPlatform)
		return false;
	if((buffer_texture&&buffer_texture->IsValid()))
		return true;
	if(buffer_depth_texture&&buffer_depth_texture->IsValid())
		return true;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(!buffer_texture)
		buffer_texture=renderPlatform->CreateTexture("BaseFramebuffer");
	if(!buffer_depth_texture)
		buffer_depth_texture=renderPlatform->CreateTexture("BaseFramebuffer");
	static int quality=0;
	if(!external_texture&&target_format!=crossplatform::UNKNOWN)
	{
		if(!is_cubemap)
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,target_format,false,true,false,numAntialiasingSamples,quality);
		else
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform,Width,Height,1,mips,target_format,false,true,true);
	}
	if(!external_depth_texture&&depth_format!=crossplatform::UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,depth_format,false,false,true,numAntialiasingSamples,quality);
	}
	return true;
}

void BaseFramebuffer::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext,"Calc Spherical Harmonics")
	int num_coefficients=bands*bands;
	static int BLOCK_SIZE=4;
	static int sqrt_jitter_samples					=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true);
	}
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	SIMUL_COMBINED_PROFILE_START(deviceContext,"clear")
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.numCoefficients		=num_coefficients;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	sphericalHarmonicsConstants.randomSeed			=shSeed;
	sphericalHarmonicsConstants.Apply(deviceContext);
	sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");
	crossplatform::EffectTechnique *clear		=sphericalHarmonicsEffect->GetTechniqueByName("clear");
	sphericalHarmonicsEffect->Apply(deviceContext,clear,0);
	renderPlatform->DispatchCompute(deviceContext,(num_coefficients+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_START(deviceContext,"jitter")
	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this buffer_texture with random 3d directions.
		crossplatform::EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		sphericalSamples.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "samplesBufferRW");
	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapTexture"	,buffer_texture);
		sphericalHarmonicsConstants.Apply(deviceContext);
		sphericalHarmonicsEffect->Apply(deviceContext,jitter,0);
		int u = (sphericalHarmonicsConstants.numJitterSamples + BLOCK_SIZE - 1) / BLOCK_SIZE;
		renderPlatform->DispatchCompute(deviceContext, u, 1, 1);
		sphericalHarmonicsEffect->UnbindTextures(deviceContext);
		sphericalHarmonicsEffect->SetUnorderedAccessView(deviceContext,"samplesBufferRW",NULL);
		sphericalHarmonicsEffect->Unapply(deviceContext);
	}
	static bool test=false;
	if(test)
	{
	// testing:
		sphericalSamples.CopyToReadBuffer(deviceContext);
		const SphericalHarmonicsSample* sam=(const SphericalHarmonicsSample* )sphericalSamples.OpenReadBuffer(deviceContext);
		if(sam)
		{
			for(int i=0;i<sphericalSamples.count;i++)
			{
				std::cout<<i<<": ("<<sam[i].dir.x<<","<<sam[i].dir.y<<","<<sam[i].dir.z<<") coefficients: ";
				for(int j=0;j<9;j++)
				{
					if(j)
						std::cout<<", ";
					std::cout<<sam[i].coeff[j];
				}
				std::cout<<std::endl;
			}
			std::cout<<std::endl;
		}
		sphericalSamples.CloseReadBuffer(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_START(deviceContext,"encode")
	crossplatform::EffectTechnique *tech	=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapTexture"	,buffer_texture);
	sphericalSamples.Apply(deviceContext, sphericalHarmonicsEffect, "samplesBuffer");
	sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");
	
	static bool sh_by_samples=false;
	sphericalHarmonicsConstants.Apply(deviceContext);
	sphericalHarmonicsEffect->Apply(deviceContext,tech,0);
	int n = sh_by_samples ? sphericalHarmonicsConstants.numJitterSamples : num_coefficients;
	//static int ENCODE_BLOCK_SIZE=2;
	//int U = ((n) + ENCODE_BLOCK_SIZE - 1) / ENCODE_BLOCK_SIZE;
	renderPlatform->DispatchCompute(deviceContext, n, 1, 1);
	sphericalHarmonicsEffect->UnbindTextures(deviceContext);
	sphericalHarmonicsConstants.Unbind(deviceContext);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	sphericalHarmonics.CopyToReadBuffer(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}

void BaseFramebuffer::RecompileShaders()
{
	SAFE_DELETE(sphericalHarmonicsEffect);
	if(!renderPlatform)
		return;
	sphericalHarmonicsEffect=renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
}