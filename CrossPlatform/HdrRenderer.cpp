#include "HdrRenderer.h"
#include <string>
#include <algorithm>			// for std::min / max
#include <assert.h>
#include <stdarg.h>
#include "Platform/Core/Timer.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/Math/RandomNumberGenerator.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/Math/Pi.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
using namespace platform;
using namespace crossplatform;

HdrRenderer::HdrRenderer()
	:renderPlatform(NULL)
	,Width(0)
	,Height(0)
	,hdr_effect(NULL)
	,exposureGammaTechnique(NULL)
	,m_pGaussianEffect(NULL)
{
}

HdrRenderer::~HdrRenderer()
{
	InvalidateDeviceObjects();
}

void HdrRenderer::SetBufferSize(int w,int h)
{
	if(Width==w&&Height==h)
		return;
	if(!renderPlatform)
		return;
	Width=w;
	Height=h;
}

void HdrRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!renderPlatform)
		return;
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	imageConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

#if !defined(__ORBIS__) && !defined(__PROSPERO__)
static std::string string_format(const std::string fmt, ...)
{
    int size = 100;
    std::string str;
    va_list ap;
    while (1)
	{
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size)
		{
            str.resize(n);
            return str;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    return str;
}
#endif

template<typename t> t max3(t a,t b,t c)
{
	return std::max(a,std::max(b,c));
}
	static int	threadsPerGroup = 128;


void HdrRenderer::EnsureEffectsAreBuilt(crossplatform::RenderPlatform *r)
{
	if (!r)
		return;
	r->EnsureEffectIsBuilt("hdr");
}

void HdrRenderer::RecompileShaders()
{
	if(!renderPlatform)
		return;
	renderPlatform->Destroy(hdr_effect);
	renderPlatform->Destroy(m_pGaussianEffect);
	hdr_effect					=renderPlatform->CreateEffect("hdr");

	exposureGammaTechnique		=hdr_effect->GetTechniqueByName("exposure_gamma");
	
	exposureGammaMainPass		=exposureGammaTechnique->GetPass("main");
	hdrConstants.LinkToEffect(hdr_effect,"HdrConstants");
	
	m_pGaussianEffect			=NULL;//renderPlatform->CreateEffect("gaussian",defs);
	//hdrConstants.LinkToEffect(m_pGaussianEffect,"HdrConstants");
	//imageConstants.LinkToEffect(m_pGaussianEffect,"ImageConstants");
	hdr_effect_imageTexture=hdr_effect->GetShaderResource("imageTexture");
	hdr_effect_imageTextureMS=hdr_effect->GetShaderResource("imageTextureMS");
}

void HdrRenderer::InvalidateDeviceObjects()
{
	hdrConstants.InvalidateDeviceObjects();
	imageConstants.InvalidateDeviceObjects();
	if (renderPlatform)
	{
		renderPlatform->Destroy(hdr_effect);
		renderPlatform->Destroy(m_pGaussianEffect);
	}
	renderPlatform=NULL;
}
void HdrRenderer::Render(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma)
{
	Render(deviceContext,texture,0,Exposure, Gamma);
}

void HdrRenderer::Render(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,float offsetX,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext,"HDR")
	hdrConstants.gamma		=Gamma;
	hdrConstants.exposure	=Exposure;
	hdrConstants.offset		=vec2(offsetX,0.0f);

	hdr_effect->SetConstantBuffer(deviceContext,&hdrConstants);
	crossplatform::EffectTechnique *tech=exposureGammaTechnique;
	
	bool msaa=texture?(texture->GetSampleCount()>1):false;
	if(msaa)
		hdr_effect->SetTexture(deviceContext,hdr_effect_imageTextureMS	,texture);
	else
		hdr_effect->SetTexture(deviceContext,hdr_effect_imageTexture	,texture);
	
	renderPlatform->ApplyPass(deviceContext,msaa?exposureGammaMSAAPass:exposureGammaMainPass);
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,hdr_effect_imageTexture,NULL);
	hdr_effect->SetTexture(deviceContext,hdr_effect_imageTextureMS,NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	
	hdr_effect->UnbindTextures(deviceContext);
	renderPlatform->UnapplyPass(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}

void HdrRenderer::RenderInfraRed(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,vec3 infrared_integration_factors,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext,"RenderInfraRed")
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdrConstants.gamma						=Gamma;
	hdrConstants.exposure					=Exposure;
	hdrConstants.infraredIntegrationFactors	=infrared_integration_factors;
	hdrConstants.offset						=vec2(0.f,0.f);
	hdr_effect->SetConstantBuffer(deviceContext,&	hdrConstants);
	crossplatform::EffectTechnique *tech=hdr_effect->GetTechniqueByName("infra_red");
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}


void HdrRenderer::RenderWithOculusCorrection(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture
	,float offsetX,float Exposure,float Gamma)
{
hdr_effect->SetTexture(deviceContext,"imageTexture",texture);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",texture);
	hdrConstants.gamma				=Gamma;
	hdrConstants.exposure			=Exposure;
	
	//float direction=(offsetX-0.5f)*2.0f;
	
    float as = float(640) / float(800);
	
	vec4 distortionK(1.0f,0.22f,0.24f,0.0f);
    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].
	//static float xco				= 0.15197642f;
	static float Distortion_Scale	= 1.7146056f;
    float scaleFactor				=1.0f/Distortion_Scale;
	hdrConstants.warpHmdWarpParam	=distortionK;
	hdrConstants.warpScreenCentre	=vec2(0.5f,0.5f);
	hdrConstants.warpScale			=vec2(0.5f* scaleFactor, 0.5f* scaleFactor * as);
	hdrConstants.warpScaleIn		=vec2(2.f,2.f/ as);
	hdrConstants.offset				=vec2(offsetX,0.0f);
	hdr_effect->SetConstantBuffer(deviceContext,&	hdrConstants);
	hdr_effect->Apply(deviceContext,warpExposureGamma,0);
	renderPlatform->DrawQuad(deviceContext);
	
	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
}

static float CalculateBoxFilterWidth(float radius, int pass)
{
	// Calculate standard deviation according to cutoff width
	// We use sigma*3 as the width of filtering window
	float sigma = radius / 3.0f;
	// The width of the repeating box filter
	float box_width = sqrt(sigma * sigma * 12.0f / pass + 1);
	return box_width;
}

void HdrRenderer::RenderDebug(GraphicsDeviceContext &deviceContext,int x0,int y0,int width,int height)
{
}