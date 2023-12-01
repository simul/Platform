
#include "Platform/DirectX12/Framebuffer.h"

#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/Timer.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/Math/Pi.h"

#include <assert.h>
#include <string>
#include <tchar.h>

using namespace platform;
using namespace dx12;

Framebuffer::Framebuffer(const char *n) : crossplatform::Framebuffer(n)
{
	targetsAndViewport = {};
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
}

void Framebuffer::SetAntialiasing(int a)
{
	if (numAntialiasingSamples != a)
	{
		numAntialiasingSamples = a;
		if (!external_texture)
			SAFE_DELETE(buffer_texture);
		if (!external_depth_texture)
			SAFE_DELETE(buffer_depth_texture);
		buffer_texture = NULL;
		buffer_depth_texture = NULL;
	}
}

void Framebuffer::Activate(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
	{
		CreateBuffers();
	}
	SIMUL_ASSERT(IsValid());
	auto rPlat = (dx12::RenderPlatform *)deviceContext.renderPlatform;

	// Get the views that we want to activate:
	dx12::Texture *col12Texture = (dx12::Texture *)buffer_texture;
	dx12::Texture *depth12Texture = (dx12::Texture *)buffer_depth_texture;
	D3D12_CPU_DESCRIPTOR_HANDLE *rtView = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE *dsView = nullptr;

	crossplatform::TextureView rtv_tv, dsv_tv;
	rtv_tv.type = buffer_texture->GetShaderResourceTypeForRTVAndDSV();
	rtv_tv.subresourceRange = {crossplatform::TextureAspectFlags::COLOUR, 0, 1, (is_cubemap && current_face != -1 ? current_face : 0), 1};
	dsv_tv.type = buffer_depth_texture ? buffer_depth_texture->GetShaderResourceTypeForRTVAndDSV() : crossplatform::ShaderResourceType::UNKNOWN;
	dsv_tv.subresourceRange = {crossplatform::TextureAspectFlags::DEPTH, 0, 1, 0, 1};

	rtView = buffer_texture->AsD3D12RenderTargetView(deviceContext, rtv_tv);
	dsView = buffer_depth_texture ? buffer_depth_texture->AsD3D12DepthStencilView(deviceContext, dsv_tv) : NULL;

	// Push current target and viewport
	targetsAndViewport.num = 1;
	targetsAndViewport.m_rt[0] = rtView;
	targetsAndViewport.textureTargets[0].texture = buffer_texture;
	targetsAndViewport.textureTargets[0].subresource = {};
	targetsAndViewport.depthTarget.texture = buffer_depth_texture;
	targetsAndViewport.depthTarget.subresource = {};
	targetsAndViewport.rtFormats[0] = col12Texture->pixelFormat;
	targetsAndViewport.m_dt = dsView;
	if (buffer_depth_texture && buffer_depth_texture->IsValid())
	{
		targetsAndViewport.depthFormat = depth12Texture->pixelFormat;
	}
	targetsAndViewport.viewport.w = Width;
	targetsAndViewport.viewport.h = Height;
	targetsAndViewport.viewport.x = 0;
	targetsAndViewport.viewport.y = 0;

	deviceContext.renderPlatform->ActivateRenderTargets(deviceContext, &targetsAndViewport);

	// Inform current samples
	mCachedMSAAState = rPlat->GetMSAAInfo();
	int colSamples = col12Texture->GetSampleCount();
	rPlat->SetCurrentSamples(colSamples == 0 ? 1 : colSamples);

	// Cache current state
	colour_active = true;
	depth_active = dsView != nullptr;
}

void Framebuffer::ActivateDepth(crossplatform::GraphicsDeviceContext &)
{
	// SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::Deactivate(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if (!colour_active && !depth_active)
	{
		SIMUL_CERR << "Deactivate was called on an already deactivated FBO. \n";
		return;
	}
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
	colour_active = false;
	depth_active = false;

	// Restore MSAA
	((dx12::RenderPlatform *)deviceContext.renderPlatform)->SetCurrentSamples(mCachedMSAAState.Count, mCachedMSAAState.Quality);
}

void Framebuffer::DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if (!buffer_depth_texture->IsValid() || !depth_active)
	{
		SIMUL_CERR << "This FBO wasn't created with depth, or depth is not active.\n";
		return;
	}
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
	targetsAndViewport.depthFormat = crossplatform::UNKNOWN;
	targetsAndViewport.m_dt = nullptr;
	deviceContext.renderPlatform->ActivateRenderTargets(deviceContext, &targetsAndViewport);
	depth_active = false;
}
