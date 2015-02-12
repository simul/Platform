#include "HdrRenderer.h"
#include <string>
#include <algorithm>			// for std::min / max
#include <assert.h>
#include <stdarg.h>
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
using namespace simul;
using namespace crossplatform;

HdrRenderer::HdrRenderer()
	:renderPlatform(NULL)
	,glow_fb(NULL)
	,alt_fb(NULL)
	,hdr_effect(NULL)
	,m_pGaussianEffect(NULL)
	,Width(0)
	,Height(0)
	,glowTexture(NULL)
	,exposureGammaTechnique(NULL)
	,glowExposureGammaTechnique(NULL)
	,glowTechnique(NULL)
	,Glow(false)
	,ReverseDepth(false)
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
	Width=w;
	Height=h;
	if(Width>0&&Height>0)
	{
		if(glow_fb)
			glow_fb->SetWidthAndHeight(Width/2,Height/2);
		if(alt_fb)
			alt_fb->SetWidthAndHeight(Width/2,Height/2);
	}
	RecompileShaders();
}

void HdrRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	SAFE_DELETE(glow_fb);
	SAFE_DELETE(alt_fb);
	glow_fb=renderPlatform->CreateFramebuffer();
	glow_fb->RestoreDeviceObjects(renderPlatform);
	alt_fb=renderPlatform->CreateFramebuffer();
	alt_fb->RestoreDeviceObjects(renderPlatform);
	glow_fb->SetFormat(crossplatform::RGBA_16_FLOAT);
	alt_fb->SetFormat(crossplatform::RGBA_16_FLOAT);
	glow_fb->SetWidthAndHeight(Width/2,Height/2);
	alt_fb->SetWidthAndHeight(Width/2,Height/2);
	
	SAFE_DELETE(glowTexture);
	glowTexture=renderPlatform->CreateTexture();

	if(renderPlatform&&Width>0&&Height>0)
	{
		glowTexture->ensureTexture2DSizeAndFormat(renderPlatform,Width/2,Height/2,crossplatform::R_32_UINT,true);
	}
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	imageConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

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

template<typename t> t max3(t a,t b,t c)
{
	return std::max(a,std::max(b,c));
}
	static int	threadsPerGroup = 128;

void HdrRenderer::RecompileShaders()
{
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	if(!renderPlatform)
		return;
	std::map<std::string,std::string> defs;
	hdr_effect					=renderPlatform->CreateEffect("hdr",defs);
	exposureGammaTechnique		=hdr_effect->GetTechniqueByName("exposure_gamma");
	glowExposureGammaTechnique	=hdr_effect->GetTechniqueByName("glow_exposure_gamma");
	warpExposureGamma			=hdr_effect->GetTechniqueByName("warp_exposure_gamma");
	warpGlowExposureGamma		=hdr_effect->GetTechniqueByName("warp_glow_exposure_gamma");
	glowTechnique				=hdr_effect->GetTechniqueByName("simul_glow");
	hdrConstants.LinkToEffect(hdr_effect,"HdrConstants");
	
	int W=Width;
	int H=Height;
	if(!H||!W)
		return;
	// Just set scan_mem_size to the largest value we can ever expect:
	int scan_smem_size			=1920;//max3(H,W,(int)threadsPerGroup*2);//1920;//
	defs["SCAN_SMEM_SIZE"]		=string_format("%d",scan_smem_size);
	defs["THREADS_PER_GROUP"]	=string_format("%d",threadsPerGroup);
	/*
	m_pGaussianEffect			=renderPlatform->CreateEffect("gaussian",defs);
	hdrConstants.LinkToEffect(m_pGaussianEffect,"HdrConstants");
	imageConstants.LinkToEffect(m_pGaussianEffect,"ImageConstants");*/
}

void HdrRenderer::InvalidateDeviceObjects()
{
	hdrConstants.InvalidateDeviceObjects();
	imageConstants.InvalidateDeviceObjects();
	SAFE_DELETE(glow_fb);
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	SAFE_DELETE(glowTexture);
	renderPlatform=NULL;
}
void HdrRenderer::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma)
{
	Render(deviceContext,texture,0,Exposure, Gamma);
}

void HdrRenderer::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float offsetX,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"HDR")
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdrConstants.gamma		=Gamma;
	hdrConstants.exposure	=Exposure;
	hdrConstants.offset		=vec2(offsetX,0.0f);
	hdrConstants.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=exposureGammaTechnique;
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture);
		tech=glowExposureGammaTechnique;
		hdr_effect->SetTexture(deviceContext,"glowTexture",glowTexture);
	}
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}
void HdrRenderer::RenderInfraRed(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,vec3 infrared_integration_factors,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"RenderInfraRed")
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdrConstants.gamma						=Gamma;
	hdrConstants.exposure					=Exposure;
	hdrConstants.infraredIntegrationFactors	=infrared_integration_factors;
	hdrConstants.offset						=vec2(0.f,0.f);
	hdrConstants.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=hdr_effect->GetTechniqueByName("infra_red");
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	renderPlatform->DrawQuad(deviceContext);

	hdr_effect->SetTexture(deviceContext,"imageTexture",NULL);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}


void HdrRenderer::RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture
	,float offsetX,float Exposure,float Gamma)
{
hdr_effect->SetTexture(deviceContext,"imageTexture",texture);
	hdr_effect->SetTexture(deviceContext,"imageTextureMS",texture);
	hdrConstants.gamma				=Gamma;
	hdrConstants.exposure			=Exposure;
	
	float direction=(offsetX-0.5f)*2.0f;


    float as = float(640) / float(800);
	
	vec4 distortionK(1.0f,0.22f,0.24f,0.0f);
    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].
	static float xco				= 0.15197642f;
	float Distortion_XCenterOffset	= direction*xco;
	static float Distortion_Scale	= 1.7146056f;
    float scaleFactor				=1.0f/Distortion_Scale;
	hdrConstants.warpHmdWarpParam	=distortionK;
	hdrConstants.warpLensCentre		=vec2(0.5f+Distortion_XCenterOffset*0.5f, 0.5f);
	hdrConstants.warpScreenCentre	=vec2(0.5f,0.5f);
	hdrConstants.warpScale			=vec2(0.5f* scaleFactor, 0.5f* scaleFactor * as);
	hdrConstants.warpScaleIn		=vec2(2.f,2.f/ as);
	hdrConstants.offset				=vec2(offsetX,0.0f);
	hdrConstants.Apply(deviceContext);
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture);
		hdr_effect->SetTexture(deviceContext,"glowTexture",glowTexture);
		hdr_effect->Apply(deviceContext,warpGlowExposureGamma,0);
	}
	else
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

void HdrRenderer::RenderGlowTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture)
{
	if(!m_pGaussianEffect)
		return;
	glowTexture->ensureTexture2DSizeAndFormat(renderPlatform,Width/2,Height/2,crossplatform::R_32_UINT,true);
		
	static int g_NumApproxPasses=3;
	static int	g_MaxApproxPasses = 8;
	static float g_FilterRadius = 30;
	// Render to the low-res glow.
	if(glowTechnique)
	{
		hdr_effect->SetTexture(deviceContext,"imageTexture",texture);
		hdr_effect->SetTexture(deviceContext,"imageTextureMS",texture);
		hdrConstants.offset				=vec2(1.f/Width,1.f/Height);
		hdrConstants.Apply(deviceContext);
		hdr_effect->Apply(deviceContext,glowTechnique,(0));
		glow_fb->Activate(deviceContext);
		glow_fb->Clear(deviceContext, 0, 0, 0, 0, 0);
		renderPlatform->DrawQuad(deviceContext);
		glow_fb->Deactivate(deviceContext);
		hdr_effect->Unapply(deviceContext);
	}

	float box_width					= CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width			= box_width * 0.5f;
	float frac_half_box_width		= (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width	= 1.0f - frac_half_box_width;
	float rcp_box_width				= 1.0f / box_width;
	// Step 1. Vertical passes: Each thread group handles a column in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",glow_fb->GetTexture());
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",glowTexture);
	imageConstants.imageSize					=uint2(glow_fb->Width,glow_fb->Height);
	// Each thread is a chunk of threadsPerGroup(=128) texels, so to cover all of them we divide by threadsPerGroup
	imageConstants.texelsPerThread				=(glow_fb->Height + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.g_NumApproxPasses			=g_NumApproxPasses-1;
	imageConstants.g_HalfBoxFilterWidth			=half_box_width;
	imageConstants.g_FracHalfBoxFilterWidth		=frac_half_box_width;
	imageConstants.g_InvFracHalfBoxFilterWidth	=inv_frac_half_box_width;
	imageConstants.g_RcpBoxFilterWidth			=rcp_box_width;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianColTechnique						=m_pGaussianEffect->GetTechniqueByName("simul_gaussian_col");
	m_pGaussianEffect->Apply(deviceContext,gaussianColTechnique,0);
	// We perform the Gaussian blur for each column. Each group is a column, and each thread 
	renderPlatform->DispatchCompute(deviceContext,glow_fb->Width,1,1);
	m_pGaussianEffect->UnbindTextures(deviceContext);
	// Unbound CS resource and output
//	ID3D11ShaderResourceView* srv_array[] = {NULL, NULL, NULL, NULL};
//	pContext->CSSetShaderResources(0, 4, srv_array);
//	ID3D11UnorderedAccessView* uav_array[] = {NULL, NULL, NULL, NULL};
//	pContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);
	
	m_pGaussianEffect->Unapply(deviceContext);
	// Step 2. Horizontal passes: Each thread group handles a row in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",glow_fb->GetTexture());
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",glowTexture);
	imageConstants.texelsPerThread				=(glow_fb->Width + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianRowTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_row");
	m_pGaussianEffect->Apply(deviceContext,gaussianRowTechnique,0);
	renderPlatform->DispatchCompute(deviceContext,glow_fb->Height,1,1);
	
	m_pGaussianEffect->UnbindTextures(deviceContext);
	// Unbound CS resource and output
	//pContext->CSSetShaderResources(0,4,srv_array);
	//pContext->CSSetUnorderedAccessViews(0,4,uav_array, NULL);
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",NULL);
	m_pGaussianEffect->Unapply(deviceContext);
}

void HdrRenderer::RenderDebug(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h)
{
	renderPlatform->DrawTexture(deviceContext,x0,y0,w/2,h/2,glow_fb->GetTexture());
	//renderPlatform->DrawTexture(deviceContext,x0+w/2,y0,w/2,h/2,&glowTexture);
		hdr_effect->SetTexture(deviceContext,"glowTexture",glowTexture);
//	simul::dx11::setTexture(hdr_effect->asD3DX11Effect(),"glowTexture",glowTexture->AsD3D11ShaderResourceView());
	renderPlatform->DrawQuad(deviceContext,x0+w/2,y0,w/2,h/2,hdr_effect,hdr_effect->GetTechniqueByName("show_compressed_texture"));
}