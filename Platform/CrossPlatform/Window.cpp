#include "Window.h"
#include "SwapChain.h"

using namespace simul;
using namespace crossplatform;
#pragma optimize("",off)

Window::Window():
	hwnd(0)
	,view_id(-1)
	,vsync(false)
	,m_swapChain(nullptr)
	,m_texture(nullptr)
	,m_rasterState(nullptr)
	,renderer(nullptr)
{
}

Window::~Window()
{
	Release();
}
#define REFCT \

void Window::RestoreDeviceObjects(RenderPlatform* r,bool m_vsync_enabled,int numerator,int denominator)
{
	renderPlatform=r;
	if(!r)
		return;

	m_swapChain=renderPlatform->CreateSwapChain();
	m_swapChain->RestoreDeviceObjects(renderPlatform,renderPlatform->GetImmediateContext(),hwnd,PixelFormat::RGBA_8_UNORM,3);
	m_texture=renderPlatform->CreateTexture("SwapChainTexture");
	m_texture->InitFromSwapChain(renderPlatform,m_swapChain);

	RenderStateDesc rasterDesc;
	rasterDesc.type=RenderStateType::RASTERIZER;
	rasterDesc.rasterizer.cullFaceMode=CullFaceMode::CULL_FACE_BACK;
	rasterDesc.rasterizer.frontFace=FrontFace::FRONTFACE_CLOCKWISE;
	rasterDesc.rasterizer.polygonMode=PolygonMode::POLYGON_MODE_FILL;
	rasterDesc.rasterizer.polygonOffsetMode=PolygonOffsetMode::POLYGON_OFFSET_DISABLE;
	rasterDesc.rasterizer.viewportScissor=ViewportScissor::VIEWPORT_SCISSOR_DISABLE;

	m_rasterState=renderPlatform->CreateRenderState(rasterDesc);

	if(renderer)
		renderer->ResizeView(view_id,m_swapChain->GetSize().x,m_swapChain->GetSize().y);
}

void Window::ResizeSwapChain(DeviceContext &deviceContext)
{
//	RECT rect;
	uint2 sz=m_swapChain->GetSize();
	m_swapChain->RestoreDeviceObjects(renderPlatform,deviceContext,hwnd,RGBA_8_UNORM,3);
	if(sz==m_swapChain->GetSize())
		return;
	m_texture->InitFromSwapChain(renderPlatform,m_swapChain);
	renderer->ResizeView(view_id,m_swapChain->GetSize().x,m_swapChain->GetSize().y);
	viewport.x=viewport.y=0;
	viewport.h=m_swapChain->GetSize().y;
	viewport.w=m_swapChain->GetSize().x;
}

void Window::SetRenderer(crossplatform::PlatformRendererInterface *ci,int vw_id)
{
	if(renderer==ci)
		return;
	if(renderer)
		renderer->RemoveView(view_id);
	view_id		=vw_id;
	renderer	=ci;
	if(!m_swapChain)
		return;
	if(view_id<0)
		view_id				=renderer->AddView();
	renderer->ResizeView(view_id,m_swapChain->GetSize().x,m_swapChain->GetSize().y);
}

void Window::Release()
{
	if(renderer)
		renderer->RemoveView(view_id);
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if(m_swapChain)
		m_swapChain->SetFullscreen(false);
	delete m_swapChain;
	delete m_texture;
	delete m_rasterState;
}
