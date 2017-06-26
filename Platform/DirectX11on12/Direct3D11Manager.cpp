#include "Simul/Platform/DirectX11on12/Direct3D11Manager.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11on12/MacrosDx1x.h"
#include "Simul/Platform/DirectX11on12/Utilities.h"
#include <iomanip>
#ifndef _XBOX_ONE
#include <dxgi.h>
#endif




#include <D3Dcompiler.h>
#include <DirectXMath.h>

//#include "client.h"
//using namespace DirectX;
//using Microsoft::WRL::ComPtr;


using namespace simul;
using namespace dx11on12;


Window::Window():
	hwnd(0)
	,view_id(-1)
	,vsync(false)
	,m_swapChain(0)
//cpo array	,m_renderTargetView(0)
	,m_depthStencilTexture(0)
	,m_depthStencilState(0)
	,m_depthStencilView(0)
	,m_rasterState(0)
	,renderer(NULL)
	,m_viewport(0.0f, 0.0f, 0.0f, 0.0f)
	,m_scissorRect(0, 0, 0, 0)
{
}

Window::~Window()
{
	Release();
}




//cpo new restore device objects
void Window::RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator)
{
	if (!d3dDevice)
		return;

//cpo old dxc11 //todo remove	DXGI_SWAP_CHAIN_DESC swapChainDesc;
//cpo dx12?	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
//cpo old dxc11 //todo remove OR USE	D3D11_RASTERIZER_DESC rasterDesc;
	RECT rect;
#if defined(WINVER) && !defined(_XBOX_ONE)
	GetWindowRect(hwnd, &rect);
#endif
	int screenWidth = abs(rect.right - rect.left);
	int screenHeight = abs(rect.bottom - rect.top);
	static int uu = 3;


	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = (float)screenWidth;
	m_viewport.Height = (float)screenHeight;

	
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = screenWidth;
	m_scissorRect.bottom = screenHeight;

//	m_viewport(0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight));
//	m_scissorRect(0, 0, static_cast<LONG>(screenWidth), static_cast<LONG>(screenHeight));


	//Now that we have the refresh rate from the system we can start the DirectX initialization.
	//The first thing we'll do is fill out the description of the swap chain.
	//The swap chain is the front and back buffer to which the graphics will be drawn.
	//Generally you use a single back buffer, do all your drawing to it, and then swap it to the front buffer which then displays
	// on the user's screen. That is why it is called a swap chain.

	// Initialize the swap chain description.
	/*********************************************************************
	//cpo dx11 swapchain mimiced below for 12
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// Set number of back buffers.
	swapChainDesc.BufferCount = uu;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

	// Set the width and height of the back buffer.
	swapChainDesc.BufferDesc.Width = screenWidth;
	swapChainDesc.BufferDesc.Height = screenHeight;
	// Set regular 32-bit surface for the back buffer.
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// The next part of the description of the swap chain is the refresh rate.
	// The refresh rate is how many times a second it draws the back buffer to the front buffer.
	// If vsync is set to true in our graphicsclass.h header then this will lock the refresh rate
	// to the system settings (for example 60hz). That means it will only draw the screen 60 times
	// a second (or higher if the system refresh rate is more than 60). However if we set vsync to
	// false then it will draw the screen as many times a second as it can, however this can cause some visual artifacts.

	// Set the refresh rate of the back buffer.
	if (m_vsync_enabled)
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	swapChainDesc.OutputWindow = hwnd;

	// Turn multisampling off.
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	// Set to  windowed mode initially always. Later we can make it fullscreen, otherwise DX gets confused.
	//if(fullscreen)
	{
		//	swapChainDesc.Windowed = false;
	}
	//else
	{
		swapChainDesc.Windowed = true;
	}

	// Set the scan line ordering and scaling to unspecified.
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	swapChainDesc.Flags = 0;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	*********************************************************************/
	// Sometimes this call to create the device will fail if the primary video card is not compatible with DirectX 11.
	// Some machines may have the primary card as a DirectX 10 video card and the secondary card as a DirectX 11 video card.
	// Also some hybrid graphics cards work that way with the primary being the low power Intel card and the secondary being
	// the high power Nvidia card. To get around this you will need to not use the default device and instead enumerate all
	// the video cards in the machine and have the user choose which one to use and then specify that card when creating the device.

	// Now that we have a swap chain we need to get a pointer to the back buffer and then attach it to the swap chain.
	// We'll use the CreateRenderTargetView function to attach the back buffer to our swap chain.

	// Get the pointer to the back buffer.
//cpo should add some error checking	HRESULT result;
#ifndef _XBOX_ONE
	///NOTE: ***YOU CANNOT DO THIS***
	///IDXGIFactory * factory;
	///result = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&factory);
	///SIMUL_ASSERT(result==S_OK);

	//cpo dx11 swap chain created here
	//cpo so dx12 swap chain created here
	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc12 = {};
	swapChainDesc12.BufferCount = FrameCount; // FrameCount;
	swapChainDesc12.Width = screenWidth; // m_width;
	swapChainDesc12.Height = screenHeight; // m_height;
	swapChainDesc12.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc12.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc12.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  //cpo DXGI_SWAP_EFFECT_SEQUENTIAL; WEIRD  DXGI_SWAP_EFFECT_FLIP;  REQUIRED BY DX12 //cpo //todo
	swapChainDesc12.SampleDesc.Count = 1;
	swapChainDesc12.SampleDesc.Quality = 0;

	// Don't set the advanced flags.
	swapChainDesc12.Flags = 0;
	swapChainDesc12.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Set the scan line ordering and scaling to unspecified.
//cpo //todo this in dx11		swapChainDesc12.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//cpo //todo this in dx11		swapChainDesc12.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

//cpo //todo this in dx11	swapChainDesc12.Windowed = true;
//cpo //todo this in dx11	swapChainDesc12.OutputWindow = hwnd;


	//cpo using original dx11 var for swap chain IDXGISwapChain1 *swapChain12;												//cpo very spare
	// need the dxgi factory that created the device
	// cant get the factory same way as dx11

	IDXGIAdapter * pDXGIAdapter;
	IDXGIFactory4 *factory4;
	IDXGIFactory2* d12factory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory4));

	factory4->EnumAdapterByLuid( d3dDevice->GetAdapterLuid(), IID_PPV_ARGS(&pDXGIAdapter));
	V_CHECK(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&d12factory));

	// Describe and create the command queue.
	//cpo already done in create device as we need it for dx11on12 device
	

	IDXGISwapChain1 *swapChain;
	d12factory->CreateSwapChainForHwnd(
		m_dx12CommandQueue,		// Swap chain needs the queue so that it can force a flush on it.
		hwnd, // Win32Application::GetHwnd(),
		&swapChainDesc12,
		nullptr,
		nullptr,
		&swapChain
	);

	swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)& m_swapChain);

	d12factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

#endif
	CreateRenderTarget(d3dDevice);
//	CreateDepthBuffer(d3dDevice);				
	CreateDepthBuffer(d3d11Device);				//cpo create a d3d11 depth buffer 
												//With that created we can now call OMSetRenderTargets.
	//This will bind the render target view and the depth stencil buffer to the output render pipeline.
	//This way the graphics that the pipeline renders will get drawn to our back buffer that we previously created.
	//With the graphics written to the back buffer we can then swap it to the front and display our graphics on the user's screen.

	//Now that the render targets are setup we can continue on to some extra functions that will give us more control
	//over our scenes for future tutorials. First thing is we'll create is a rasterizer state.
	//This will give us control over how polygons are rendered. We can do things like make our scenes render in wireframe
	//mode or have DirectX draw both the front and back faces of polygons. By default DirectX already has a rasterizer
	//state set up and working the exact same as the one below but you have no control to change it unless you set up one yourself.

	// Setup the raster description which will determine how and what polygons will be drawn.
	
	//cpo removed as set up done in fx??
	/*
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	*/
	// Create the rasterizer state from the description we just filled out.
//cpo following two removed as we set states up
//cpofixme	result = d3dDevice->CreateRasterizerState(&rasterDesc, &m_rasterState);
//cpofixme	SIMUL_ASSERT(result == S_OK);
	
}




void Window::ResizeSwapChain(ID3D12Device* d3dDevice)
{
	RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
	if(!GetWindowRect(hwnd,&rect))
		return;
#endif
	int W	=abs(rect.right-rect.left);
	int H	=abs(rect.bottom-rect.top);
	ID3D11RenderTargetView *t[]={NULL};


	//cpo dx11 code
	//DXGI_SWAP_CHAIN_DESC swapDesc;

	DXGI_SWAP_CHAIN_DESC1 swapDesc;
	HRESULT hr = m_swapChain->GetDesc1(&swapDesc);
	if (hr != S_OK)
		return;
	if (swapDesc.Width == W&&swapDesc.Height == H)
		return;



	SAFE_RELEASE_ARRAY(m_renderTargetView,FrameCount);
	SAFE_RELEASE(m_depthStencilTexture);
	SAFE_RELEASE(m_depthStencilView);
	SAFE_RELEASE(m_depthStencilState);
	SAFE_RELEASE(m_rasterState);
			//		*m_depthStencilState;
			//		*m_rasterState;
	V_CHECK(m_swapChain->ResizeBuffers(2,W,H,DXGI_FORMAT_R8G8B8A8_UNORM,0));
	CreateRenderTarget(d3dDevice);
//	CreateDepthBuffer(d3dDevice);					//cpo create a d3d11 depth buffer 
	CreateDepthBuffer(d3d11Device);					//cpo create a d3d11 depth buffer 

	DXGI_SURFACE_DESC surfaceDesc;
	m_swapChain->GetDesc1(&swapDesc);

	surfaceDesc.Format		=swapDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.Width;
	surfaceDesc.Height		=swapDesc.Height;
	if(renderer)
		renderer->ResizeView(view_id,surfaceDesc.Width,surfaceDesc.Height);
}

void Window::CreateRenderTarget(ID3D12Device* d3dDevice)
{
	if(!d3dDevice)
		return;
	if(!m_swapChain)
		return;

	/* 
	//cpodx11 pre cursor 
	ID3D12Resource* backBufferPtr;
	HRESULT result = m_swapChain->GetBuffer(0, __uuidof(ID3D12Resource), (LPVOID*)&backBufferPtr);
	SIMUL_ASSERT(result==S_OK);
	// Create the render target view with the back buffer pointer.
	SAFE_RELEASE(m_renderTargetView);
	*/
	//cpo dx11 version
	//cpo result = d3dDevice->CreateRenderTargetView(backBufferPtr, NULL, &m_renderTargetView);

	//dx12 version

	HRESULT result = S_OK;
	//UINT m_rtvDescriptorSize;
	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount; 
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		result = d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

		m_rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	
	// get a ptr to the d3d11on12device
	//cpo //todo do this earlier
	d3d11Device->QueryInterface(__uuidof(ID3D11On12Device), (void **)&d3d11on12Device);
	
	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		//cpo  only do first //todo why
		// Create a RTV, D2D render target, and a command allocator for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			result = m_swapChain->GetBuffer(n, __uuidof(ID3D12Resource), (LPVOID*)&backBufferPtr[n]);				//cpo make sense to get each buffer
			d3dDevice->CreateRenderTargetView(backBufferPtr[n], nullptr, rtvHandle);

			//extra for dx11 on 12

			// Create a wrapped 11On12 resource of this back buffer. Since we are 
			// rendering all D3D12 content first and then all D2D content, we specify 
			// the In resource state as RENDER_TARGET - because D3D12 will have last 
			// used it in this state - and the Out resource state as PRESENT. When 
			// ReleaseWrappedResources() is called on the 11On12 device, the resource 
			// will be transitioned to the PRESENT state.
			D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
			result = d3d11on12Device->CreateWrappedResource(
				//cpo //todo m_renderTargets[n].Get(),										// the d3d12 back buffer(s)
				backBufferPtr[n],																// the d3d12 back buffer jsut a singular
				&d3d11Flags,								// D3D11 FLAGS
				D3D12_RESOURCE_STATE_RENDER_TARGET,			// in state
				D3D12_RESOURCE_STATE_PRESENT,				// out state  
				IID_PPV_ARGS(&m_wrappedBackBuffer[n])										// the d3d11 back buffer(s)
			);
			result = d3d11Device->CreateRenderTargetView(m_wrappedBackBuffer[n], NULL, &m_renderTargetView[n]);
			SetDebugObjectName(m_renderTargetView[n], "Window m_renderTargetView");
			// end extra dx11on12	
			rtvHandle.Offset(1, m_rtvDescriptorSize);
			backBufferPtr[n]->SetName(L"Window backBufferPtr");									//cpo dx12 debug name
			//cpo Hmmm SAFE_RELEASE(backBufferPtr[n]);														//cpo was below seems poinntless to name before release
			
			
			//cpo SetDebugObjectName( backBufferPtr,"Window backBufferPtr");

			result = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n]));
		}

	}







	//cpo end ikky bit TWO

	SIMUL_ASSERT(result==S_OK);
	// Release pointer to the back buffer as we no longer need it.
}


void Window::CreateDepthBuffer(ID3D11Device* d3dDevice)
{
	RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
	GetWindowRect(hwnd, &rect);
#endif
	int screenWidth = abs(rect.right - rect.left);
	int screenHeight = abs(rect.bottom - rect.top);
	// We will also need to set up a depth buffer description.
	// We'll use this to create a depth buffer so that our polygons can be rendered properly in 3D space.
	// At the same time we will attach a stencil buffer to our depth buffer.
	// The stencil buffer can be used to achieve effects such as motion blur, volumetric shadows, and other things.
	// Initialize the description of the depth buffer.
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = screenWidth;
	depthBufferDesc.Height = screenHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;
	// Now we create the depth/stencil buffer using that description.
	// You will notice we use the CreateTexture2D function to make the buffers,
	// hence the buffer is just a 2D texture.
	// The reason for this is that once your polygons are sorted and then rasterized they just end up being colored pixels in this 2D buffer.
	// Then this 2D buffer is drawn to the screen.

	// Create the texture for the depth buffer using the filled out description.
	SAFE_RELEASE(m_depthStencilTexture);
	HRESULT result = d3dDevice->CreateTexture2D(&depthBufferDesc, NULL, &m_depthStencilTexture);
	SetDebugObjectName(m_depthStencilTexture, "Window m_depthStencilTexture");
	SIMUL_ASSERT(result == S_OK);
	//Now we need to setup the depth stencil description.
	//This allows us to control what type of depth test Direct3D will do for each pixel.

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	//With the description filled out we can now create a depth stencil state.

	// Create the depth stencil state.
	SAFE_RELEASE(m_depthStencilState);
	result = d3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
	SetDebugObjectName(m_depthStencilState, "Window m_depthStencilState");
	SIMUL_ASSERT(result == S_OK);
	//With the created depth stencil state we can now set it so that it takes effect. Notice we use the device context to set it.

	//The next thing we need to create is the description of the view of the depth stencil buffer.
	//We do this so that Direct3D knows to use the depth buffer as a depth stencil texture.
	//After filling out the description we then call the function CreateDepthStencilView to create it.

	// Initalize the depth stencil view.
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	SAFE_RELEASE(m_depthStencilView)
		result = d3dDevice->CreateDepthStencilView(m_depthStencilTexture, &depthStencilViewDesc, &m_depthStencilView);
	SetDebugObjectName(m_depthStencilView, "Window m_depthStencilView");
	SIMUL_ASSERT(result == S_OK);
	//The viewport also needs to be setup so that Direct3D can map clip space coordinates to the render target space.
	//Set this to be the entire size of the window.

	// Setup the viewport for rendering.
	viewport.Width = (float)screenWidth;
	viewport.Height = (float)screenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
}




void Window::CreateDepthBuffer(ID3D12Device* d3dDevice)
{
	RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
	GetWindowRect(hwnd,&rect);
#endif
	int screenWidth	=abs(rect.right-rect.left);
	int screenHeight=abs(rect.bottom-rect.top);
	// We will also need to set up a depth buffer description.
	// We'll use this to create a depth buffer so that our polygons can be rendered properly in 3D space.
	// At the same time we will attach a stencil buffer to our depth buffer.
	// The stencil buffer can be used to achieve effects such as motion blur, volumetric shadows, and other things.
	// Initialize the description of the depth buffer.
	D3D12_RESOURCE_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = screenWidth;
	depthBufferDesc.Height = screenHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;

	// Now we create the depth/stencil buffer using that description.
	// You will notice we use the CreateTexture2D function to make the buffers,
	// hence the buffer is just a 2D texture.
	// The reason for this is that once your polygons are sorted and then rasterized they just end up being colored pixels in this 2D buffer.
	// Then this 2D buffer is drawn to the screen.

	// Create the texture for the depth buffer using the filled out description.
	SAFE_RELEASE(m_depthStencilTexture);
	
	//cpofixme original ish dx11
	//old HRESULT result = d3dDevice->CreateCommittedResource(&depthBufferDesc, NULL, &m_depthStencilTexture);

	//cpo good guess at d3d12
	//Creating the depth texture
	CD3DX12_RESOURCE_DESC depth_texture(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0,
		static_cast< UINT >(screenWidth), static_cast< UINT >(screenHeight), 1, 1,
		DXGI_FORMAT_D24_UNORM_S8_UINT, 1, 0, D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);


	D3D12_CLEAR_VALUE clear_value;
	clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clear_value.DepthStencil.Depth = 1.0f;
	clear_value.DepthStencil.Stencil = 0;

	HRESULT result = d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &depth_texture, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value,
		IID_PPV_ARGS(&m_depthStencilTexture12));

//cpo	m_depthStencilTexture->SetName(L"Window m_depthStencilTexture");						//cpo //todo this whole thingy needs clean up for 12
	//cpo END iffy bit

	//cpofixme SetDebugObjectName( m_depthStencilTexture,"Window m_depthStencilTexture");
	SIMUL_ASSERT(result==S_OK);
	//Now we need to setup the depth stencil description.
	//This allows us to control what type of depth test Direct3D will do for each pixel.

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;				//cpo D3D11_COMPARISON_FUNC_LESS;		//cpo from			

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;  //cpo D3D11_COMPARISON_FUNC_ALWAYS; //cpo from		

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp= D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc		= D3D11_COMPARISON_ALWAYS;  // D3D11_COMPARISON_FUNC_ALWAYS; //cpo from		
//With the description filled out we can now create a depth stencil state.

	// Create the depth stencil state.
	SAFE_RELEASE(m_depthStencilState);
//cpofixme	
//cpo	
	result = d3d11Device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
	SetDebugObjectName( m_depthStencilState,"Window m_depthStencilState");
	SIMUL_ASSERT(result==S_OK);




	//With the created depth stencil state we can now set it so that it takes effect. Notice we use the device context to set it.

	//The next thing we need to create is the description of the view of the depth stencil buffer.
	//We do this so that Direct3D knows to use the depth buffer as a depth stencil texture.
	//After filling out the description we then call the function CreateDepthStencilView to create it.

	// Initalize the depth stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	SAFE_RELEASE(m_depthStencilView);
	//cpo dx11 version
	//result = d3dDevice->CreateDepthStencilView(m_depthStencilTexture, &depthStencilViewDesc, &m_depthStencilView);
	//cpo dx12 version
	UINT FrameCount = 3; //cpo pull this in 

	ID3D12DescriptorHeap *m_dsvHeap;
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1 + FrameCount * 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
	assert(result == S_OK && "ERROR CREATING THE DSV HEAP");
	m_dsvHeap->SetName(L"DX11on12 DSV heap");

	d3dDevice->CreateDepthStencilView(m_depthStencilTexture12, &depthStencilViewDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	//------------------------------------------------------------------------------------------------------------------
	// create wrapped resource ???????
	// Create a wrapped 11On12
	D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_DEPTH_STENCIL };
	result = d3d11on12Device->CreateWrappedResource(
		//cpo //todo m_renderTargets[n].Get(),										// the d3d12 back buffer(s)
		m_depthStencilTexture12,																// the d3d12 back buffer jsut a singular
		&d3d11Flags,								// D3D11 FLAGS
		D3D12_RESOURCE_STATE_DEPTH_WRITE| D3D12_RESOURCE_STATE_DEPTH_READ,				// in state
		D3D12_RESOURCE_STATE_DEPTH_WRITE| D3D12_RESOURCE_STATE_DEPTH_READ,				// out state  
		IID_PPV_ARGS(&m_WrappedDepthStencilTexture)										// the d3d11 back buffer(s)
	);


	// Initalize the D3Ddepth stencil view.
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc11;
	ZeroMemory(&depthStencilViewDesc11, sizeof(depthStencilViewDesc));

	// Set up the D3D11depth stencil view description.
	depthStencilViewDesc11.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc11.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	//?SAFE_RELEASE(m_depthStencilView)
	result = d3d11Device->CreateDepthStencilView(m_WrappedDepthStencilTexture, &depthStencilViewDesc11, &m_depthStencilView);
	// end create wrapped resource ???????
	//------------------------------------------------------------------------------------------------------------------

	SetDebugObjectName( m_depthStencilView,"Window m_depthStencilView");
	SIMUL_ASSERT(result==S_OK);
	//The viewport also needs to be setup so that Direct3D can map clip space coordinates to the render target space.
	//Set this to be the entire size of the window.

	// Setup the viewport for rendering.
	viewport.Width = (float)screenWidth;
	viewport.Height = (float)screenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
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


	//cpo //todo commandlist set here
	//cpo framestart

	DXGI_SWAP_CHAIN_DESC swapDesc;
	DXGI_SURFACE_DESC surfaceDesc;
	m_swapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	if(view_id<0)
		view_id				=renderer->AddView();
	renderer->ResizeView(view_id,surfaceDesc.Width,surfaceDesc.Height);
}



void Window::preFrame(UINT frameIndex) {
	// Acquire our wrapped render target resource for the current back buffer.
	d3d11on12Device->AcquireWrappedResources(&m_wrappedBackBuffer[frameIndex], 1);

//	d3d11on12Device->AcquireWrappedResources(&m_WrappedDepthStencilTexture, 1);

}

void Window::postFrame(UINT frameIndex) {



//	d3d11on12Device->ReleaseWrappedResources(&m_WrappedDepthStencilTexture, 1);


	d3d11on12Device->ReleaseWrappedResources(&m_wrappedBackBuffer[frameIndex], 1);

	
}

void Window::Release()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if(m_swapChain)
		m_swapChain->SetFullscreenState(false, NULL);
	SAFE_RELEASE(m_swapChain);
	SAFE_RELEASE_ARRAY(m_renderTargetView,FrameCount);
	SAFE_RELEASE(m_depthStencilTexture);
	SAFE_RELEASE(m_depthStencilState);
	SAFE_RELEASE(m_depthStencilView);
	SAFE_RELEASE(m_rasterState);
}








//cpo required extra info for window, set here
void Window::setCommandQueue(ID3D12CommandQueue *commandQueue) {
	m_dx12CommandQueue = commandQueue;
}


//cpo required extra info for window, set here
void Window::setRootSignature(ID3D12RootSignature *rootSignature) {
	m_rootSignature = rootSignature;
}

//cpo required extra info for window, set here
void simul::dx11on12::Window::setD3D11Device(ID3D11Device * d3dDevice)
{
	d3d11Device = d3dDevice;
}


Direct3D11Manager::Direct3D11Manager()
	:d3dDevice(0)
	,d3dDeviceContext(0)
	,d3dDebug(NULL)
	,d3dInfoQueue(NULL)
{
}


Direct3D11Manager::~Direct3D11Manager()
{
	Shutdown();
}



//cpo replace D3D11CreateDevice
// with dx12 device setup 
// followed by create dx11on12 device
//


HRESULT Direct3D11Manager::createDx11on12Device(_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_Out_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_Out_opt_ ID3D11DeviceContext** ppImmediateContext) {

	HRESULT result;


//#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	
		ID3D12Debug *debugController;
		ID3D12Debug1 *spDebugController1;

		if (D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))  == S_OK )
		{
			debugController->EnableDebugLayer();

			debugController->QueryInterface(IID_PPV_ARGS(&spDebugController1));
			spDebugController1->SetEnableGPUBasedValidation(false);					//cpo true is good //todo

			Flags |= D3D11_CREATE_DEVICE_DEBUG;					// already covered but may wrap the two together
		}
	
//#endif

	
	D3D12CreateDevice(
		pAdapter,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_d3d12Device)
	);


	m_d3d12Device->SetName(L"dx11on12 d3d12 device");

	//cpo once we have a dx12 device we can set extra debugging
	// output messagae to console
/*
	ID3D12DebugDevice *debugDevice;
	m_d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice));
	debugDevice->SetFeatureMask(D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS | 
								D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_CONSERVATIVE_RESOURCE_STATE_TRACKING |
								D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_DISABLE_VIRTUALIZED_BUNDLES_VALIDATION);


	//cpo more extra debug, break on certain classes of error
	//cpo these mimic code used for dx11 break on
	ID3D12InfoQueue *d3d12InfoQueue = nullptr;
	if (SUCCEEDED(debugDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&d3d12InfoQueue)))
	{
		//#ifdef _DEBUG
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//#endif
	}
*/
	// set up debug informastion of d3d12 device


#if 1//./defined(_DEBUG)

	//cpo once we have a dx12 device we can set extra debugging
	// output messagae to console
	ID3D12DebugDevice *debugDevice;
	m_d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice));
	debugDevice->SetFeatureMask(D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS |
		D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_CONSERVATIVE_RESOURCE_STATE_TRACKING |
		D3D12_DEBUG_FEATURE::D3D12_DEBUG_FEATURE_DISABLE_VIRTUALIZED_BUNDLES_VALIDATION);



	// Filter a debug error coming from the 11on12 layer.
	ID3D12InfoQueue *d3d12InfoQueue = nullptr;
	if (SUCCEEDED(m_d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue))))
	{
		//cpo 
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);

		
		// Suppress whole categories of messages.
		//D3D12_MESSAGE_CATEGORY categories[] = {};

		// Suppress messages based on their severity level.
		D3D12_MESSAGE_SEVERITY severities[] =
		{
		D3D12_MESSAGE_SEVERITY_INFO,
		};

		// Suppress individual messages by their ID.
		D3D12_MESSAGE_ID denyIds[] =
		{
		// This occurs when there are uninitialized descriptors in a descriptor table, even when a
		// shader does not access the missing descriptors.
		D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

		//cpo this one generated by call to device11-generateMips
		D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE,
		//cpo again in generratemips???
		D3D12_MESSAGE_ID_RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH,
		//D3D12_MESSAGE_ID_DATA_STATIC_WHILE_SET_AT_EXECUTE_DESCRIPTOR_INVALID_DATA_CHANGE,
		
		//cpo despatch compute
		//D3D12_MESSAGE_ID_STATIC_DESCRIPTOR_INVALID_DESCRIPTOR_CHANGE

		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		//filter.DenyList.NumCategories = _countof(categories);
		//filter.DenyList.pCategoryList = categories;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;

		d3d12InfoQueue->PushStorageFilter(&filter);

		// force break on error for other stuff
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		
	}
#endif
	// end set up debug information on d3d12 device
	
	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));


	m_commandQueue->SetName(L"dx11on12 command queue");
	//cpo create d3d12 swap chain
	//cpo swap chain created with Window


	//cpo //fudge
	//cpo from 11on12 D3D1211on12::LoadAssets()
	// Create an empty root signature.
	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ID3DBlob *signature;
		ID3DBlob *error;
		D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		m_d3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		m_rootSignature->SetName(L"dx11on12 Root Signature");


		/*
		//cpo extra set up code
 		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature;
		psoDesc.VS = nullptr; //cpo CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = nullptr; //cpo CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		m_d3d12Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		m_pipelineState->SetName(L"dx11on12 m_pipelineState");
		*/
	//}
	/*
	//cpo more extra set up code
	m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex], m_pipelineState, IID_PPV_ARGS(&m_commandList));
	m_commandList->SetName(L"dx11on12 m_commandList");
	*/



	}

	// end fudge
	result = S_OK;
	result = D3D11On12CreateDevice(
		m_d3d12Device,
		Flags,
		pFeatureLevels,
		FeatureLevels,
		reinterpret_cast<IUnknown**>(&m_commandQueue),
		1,
		0,
		ppDevice,
		ppImmediateContext,
		pFeatureLevel
	);

	return result;
}



void Direct3D11Manager::Initialize(bool use_debug,bool instrument)
{
	HRESULT result;
//	IDXGIFactory* factory;
//	int  i;//, numerator, denominator;
	DXGI_ADAPTER_DESC adapterDesc;
	D3D_FEATURE_LEVEL featureLevel;

	// Store the vsync setting.
	m_vsync_enabled = false;
#ifndef _XBOX_ONE
	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&factory);
	SIMUL_ASSERT(result==S_OK);
	// Use the factory to create an adapter for the primary graphics interface (video card).
	SAFE_RELEASE(adapter);
	result = factory->EnumAdapters(0, &adapter);
	SIMUL_ASSERT(result==S_OK);

	// Enumerate the primary adapter output (monitor).
	//result = adapter->EnumOutputs(0, &adapterOutput);
	IDXGIOutput* output=NULL;
	int i=0;
	while(adapter->EnumOutputs(i,&output)!=DXGI_ERROR_NOT_FOUND)
	{
		outputs[i]=output;
		SIMUL_ASSERT(result==S_OK);
		i++;
		if(i>100)
		{
			std::cerr<<"Tried 100 outputs: no adaptor was found."<<std::endl;
			return;
		}
	}
	//We now have the numerator and denominator for the refresh rate.
	//The last thing we will retrieve using the adapter is the name of the video card and the amount of memory on the video card.

	// Get the adapter (video card) description.
	result = adapter->GetDesc(&adapterDesc);
	SIMUL_ASSERT(result==S_OK);

	// Store the dedicated video card memory in megabytes.
	m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	size_t stringLength;
	int error;
	error = wcstombs_s(&stringLength, m_videoCardDescription, 128, adapterDesc.Description, 128);
	SIMUL_ASSERT(error==0);
	//Now that we have stored the numerator and denominator for the refresh rate and the video card information we can release the structures and interfaces used to get that information.

	// Release the factory.
	SAFE_RELEASE(factory);
#endif
	

	//After setting up the swap chain description we also need to setup one more variable called the feature level.
	// This variable tells DirectX what version we plan to use. Here we set the feature level to 11.0 which is DirectX 11.
	// You can set this to 10 or 9 to use a lower level version of DirectX if you plan on supporting multiple versions or running on lower end hardware.

	// Set the feature level to DirectX 11.1
	featureLevel = D3D_FEATURE_LEVEL_11_0;
	//Now that the swap chain description and feature level have been filled out we can create the swap chain, the Direct3D device, and the Direct3D device context.
	// The Direct3D device and Direct3D device context are very important, they are the interface to all of the Direct3D functions. We will use the device and device context for almost everything from this point forward.

	//Those of you reading this who are familiar with the previous versions of DirectX will recognize the Direct3D device but will be unfamiliar with the new Direct3D device context.
	//Basically they took the functionality of the Direct3D device and split it up into two different devices so you need to use both now.

	// Note that if the user does not have a DirectX 11 video card this function call will fail to create the device and device context.
	// Also if you are testing DirectX 11 functionality yourself and don't have a DirectX 11 video card then you can replace D3D_DRIVER_TYPE_HARDWARE
	// with D3D_DRIVER_TYPE_REFERENCE and DirectX will use your CPU to draw instead of the video card hardware.
	// Note that this runs 1/1000 the speed but it is good for people who don't have DirectX 11 video cards yet on all their machines.

	// Create the swap chain, Direct3D device, and Direct3D device context.
	//result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1, 
	//				       D3D11_SDK_VERSION, &swapChainDesc, &m_swapChain, &d3dDevice, NULL, &d3dDeviceContext);
	UINT flags=0;
	if(use_debug)
		flags|=D3D11_CREATE_DEVICE_DEBUG;
#ifdef _XBOX_ONE
	if(instrument)
		flags|=D3D11_CREATE_DEVICE_INSTRUMENTED;
#endif
	//std::cout<<"D3D11CreateDevice "<<std::endl;
	//result = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, &featureLevel, 1, D3D11_SDK_VERSION, &d3dDevice, NULL, &d3dDeviceContext);
	result = createDx11on12Device(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, &featureLevel, 1, D3D11_SDK_VERSION, &d3dDevice, NULL, &d3dDeviceContext);

	if(result!=S_OK)
	{
		SIMUL_CERR<<"D3D11CreateDevice result "<<result<<std::endl;
		return;
	}
	//d3dDevice->AddRef();
	//UINT refcount=d3dDevice->Release();
	UINT exc=d3dDevice->GetExceptionMode();
	if(exc>0)
		std::cout<<"d3dDevice Exception mode is "<<exc<<std::endl;
#ifndef _XBOX_ONE
	SAFE_RELEASE(d3dDebug);
#endif
	SAFE_RELEASE(d3dInfoQueue);
	if(use_debug)
	{
	#ifndef _XBOX_ONE
		d3dDevice->QueryInterface( __uuidof(ID3D11Debug), (void**)&d3dDebug );
		if(d3dDebug)
			d3dDebug->QueryInterface( __uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue );
	#endif
		if(d3dInfoQueue)
		{
			d3dInfoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_CORRUPTION, true );
			d3dInfoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_ERROR, true );
			d3dInfoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_WARNING, true );
	
			ReportMessageFilterState();
			d3dInfoQueue->ClearStoredMessages();
			d3dInfoQueue->ClearRetrievalFilter();
			d3dInfoQueue->ClearStorageFilter();
			ReportMessageFilterState();
		}
	
/*	D3D11_MESSAGE_CATEGORY cats[] = {/*D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED
										,D3D11_MESSAGE_CATEGORY_MISCELLANEOUS	
										,D3D11_MESSAGE_CATEGORY_INITIALIZATION	
										,D3D11_MESSAGE_CATEGORY_CLEANUP
										,D3D11_MESSAGE_CATEGORY_COMPILATION 
										,D3D11_MESSAGE_CATEGORY_STATE_CREATION
										,D3D11_MESSAGE_CATEGORY_STATE_SETTING
										,D3D11_MESSAGE_CATEGORY_STATE_GETTING
										,D3D11_MESSAGE_CATEGORY_RESOURCE_MANIPULATION
										D3D11_MESSAGE_CATEGORY_EXECUTION
									};*/
	D3D11_MESSAGE_SEVERITY sevs[] = { 
		D3D11_MESSAGE_SEVERITY_ERROR
		,D3D11_MESSAGE_SEVERITY_WARNING};
/*	UINT ids[] = { D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
		D3D11_MESSAGE_ID_DESTROY_VERTEXSHADER,
		D3D11_MESSAGE_ID_DESTROY_PIXELSHADER,
		D3D11_MESSAGE_ID_DESTROY_COMPUTESHADER };*/

		D3D11_INFO_QUEUE_FILTER filter;
		memset( &filter, 0, sizeof(filter) );

		// To set the type of messages to allow, 
		// set filter.AllowList as follows:
		//filter.AllowList.NumCategories = sizeof(cats) / sizeof(D3D11_MESSAGE_CATEGORY); 
		//filter.AllowList.pCategoryList = cats;
		filter.AllowList.NumSeverities = 2; 
		filter.AllowList.pSeverityList = sevs;
		filter.AllowList.NumIDs = 0;//sizeof(ids) / sizeof(UINT);
		//..filter.AllowList.pIDList = ids;

		// To set the type of messages to deny, set filter.DenyList 
		// similarly to the preceding filter.AllowList.
		if(d3dInfoQueue)
		// The following single call sets all of the preceding information.
			V_CHECK(d3dInfoQueue->AddStorageFilterEntries( &filter ));
		ReportMessageFilterState();
	}
	//d3dDevice->CreateVertexShader((uint32_t *)0, 0, NULL, NULL);
	//d3dDevice->AddRef();
	//UINT refcount2=d3dDevice->Release();
	//std::cout<<"result "<<result<<std::endl;
	SIMUL_ASSERT(result==S_OK);
}

int Direct3D11Manager::GetNumOutputs()
{
	return (int)outputs.size();
}

crossplatform::Output Direct3D11Manager::GetOutput(int i)
{
	unsigned numModes;
	crossplatform::Output o;
	IDXGIOutput *output=outputs[i];
#ifndef _XBOX_ONE
	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	HRESULT result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	SIMUL_ASSERT(result==S_OK);

	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
	o.width		=abs(outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left);
	o.height	=abs(outputDesc.DesktopCoordinates.top-outputDesc.DesktopCoordinates.bottom);

	// Now get extended information about what monitor this is:
	
#if defined(WINVER) &&!defined(_XBOX_ONE)
	MONITORINFOEX monitor;
    monitor.cbSize = sizeof(monitor);
    if (::GetMonitorInfo(outputDesc.Monitor, &monitor) && monitor.szDevice[0])
    {
		DISPLAY_DEVICE dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
		dispDev.cb = sizeof(dispDev);

       if (::EnumDisplayDevices(monitor.szDevice, 0, &dispDev, 0))
       {
           o.monitorName=base::WStringToUtf8(dispDev.DeviceName);
           o.desktopX	=monitor.rcMonitor.left;
           o.desktopY	=monitor.rcMonitor.top;
       }
   }
#endif
	// Create a list to hold all the possible display modes for this monitor/video card combination.
	DXGI_MODE_DESC* displayModeList;
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	SIMUL_ASSERT(result==S_OK);
	
	// It is useful to get the refresh rate from the video card/monitor.
	//Each computer may be slightly different so we will need to query for that information.
	//We query for the numerator and denominator values and then pass them to DirectX during the setup and it will calculate the proper refresh rate.
	// If we don't do this and just set the refresh rate to a default value which may not exist on all computers then DirectX will respond by performing
	// a blit instead of a buffer flip which will degrade performance and give us annoying errors in the debug output.


	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for(i=0; i<(int)numModes; i++)
	{
		if(displayModeList[i].Width == (unsigned int)o.width)
		{
			if(displayModeList[i].Height == (unsigned int)o.height)
			{
				o.numerator = displayModeList[i].RefreshRate.Numerator;
				o.denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}
	// Release the display mode list.
	delete [] displayModeList;
	displayModeList = 0;
#endif
	return o;
}
void Direct3D11Manager::Shutdown()
{
	// Release the adapter.
	SAFE_RELEASE(adapter);
	for(OutputMap::iterator i=outputs.begin();i!=outputs.end();i++)
	{
		SAFE_RELEASE(i->second);
	}
	outputs.clear();
	for(WindowMap::iterator i=windows.begin();i!=windows.end();i++)
	{
		SetFullScreen(i->second->hwnd,false,0);
		delete i->second;
	}
	windows.clear();
	if(d3dDeviceContext)
	{
		d3dDeviceContext->ClearState();
		d3dDeviceContext->Flush();
	}
	SAFE_RELEASE(d3dDeviceContext);
#ifndef _XBOX_ONE
	if(d3dDebug)
	{
		// Watch out - this will ALWAYS BREAK, but that doesn't mean there are live objects.
	//	d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	}
#endif
	ReportMessageFilterState();
	SAFE_RELEASE(d3dInfoQueue);
#ifndef _XBOX_ONE
	SAFE_RELEASE(d3dDebug);
#endif
	// Finally, we can destroy the device.
	if(d3dDevice)
	{
		UINT exc=d3dDevice->GetExceptionMode();
		if(exc>0)
			std::cout<<"d3dDevice Exception mode is "<<exc<<std::endl;
		UINT references=d3dDevice->Release();
		if(references>0)
		{
//cpo //todo //fudge			SIMUL_BREAK("Unfreed references remain in DirectX 11");
		}
		d3dDevice=NULL;
	}
}

void Direct3D11Manager::RemoveWindow(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	SetFullScreen(hwnd,false,0);
	delete w;
	windows.erase(hwnd);
}

IDXGISwapChain *Direct3D11Manager::GetSwapChain(HWND h)
{
	if(windows.find(h)==windows.end())
		return NULL;
	Window *w=windows[h];
	if(!w)
		return NULL;
	return w->m_swapChain;
}

void Direct3D11Manager::Render(HWND h)
{
	if(windows.find(h)==windows.end())
		return;
ERRNO_BREAK
	Window *w=windows[h];
	if(!w)
	{
		SIMUL_CERR<<"No window exists for HWND "<<std::hex<<h<<std::endl;
		return;
	}
	if(h!=w->hwnd)
	{
		SIMUL_CERR<<"Window for HWND "<<std::hex<<h<<" has hwnd "<<w->hwnd<<std::endl;
		return;
	}


	m_frameIndex = w->m_swapChain->GetCurrentBackBufferIndex();
	//ResizeSwapChain(w->hwnd);
	if(!w->m_renderTargetView[m_frameIndex])
	{
		SIMUL_CERR<<"No renderTarget exists for HWND "<<std::hex<<h<<std::endl;
		return;
	}
ERRNO_BREAK
/*
	//d3d12 render triangle 
	PopulateCommandList(w);

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
*/


	
	/**/
	// dx11on12 preframe goes here
	// aquire wrapped resources
	w->preFrame(m_frameIndex);				//cpo d3d11on12Device->AcquireWrappedResources(&m_wrappedBackBuffer, 1);
		
	// Set the depth stencil state.
	d3dDeviceContext->OMSetDepthStencilState(w->m_depthStencilState, 1);
	// Bind the render target view and depth stencil buffer to the output render pipeline.
	d3dDeviceContext->OMSetRenderTargets(1, &w->m_renderTargetView[m_frameIndex],w->m_depthStencilView);				 // m_renderTargetView created for 11 device
	// Create the viewport.
	d3dDeviceContext->RSSetViewports(1, &w->viewport);
	// Now set the rasterizer state.
	d3dDeviceContext->RSSetState(w->m_rasterState);
	
	if(w->renderer)
	{
		w->renderer->Render(w->view_id,GetDevice(),GetDeviceContext());
	}
	// dx11on12 postframe goes here release wrapped resources
	w->postFrame(m_frameIndex);													// d3d11on12Device->ReleaseWrappedResources(&m_wrappedBackBuffer, 1);
	// Flush to submit the 11 command list to the shared command queue.
	d3dDeviceContext->Flush();										//cpo actually the 11 device context
/**/

	static DWORD dwFlags = 0;
	// 0 - don't wait for 60Hz refresh.
	static UINT SyncInterval = 1;									//cpo was  0 impacts on Present below  quads worked witn 1
    // Show the frame on the primary surface.
	// TODO: what if the device is lost?
	V_CHECK(w->m_swapChain->Present(SyncInterval,dwFlags));			//cpo the d3d12 swapchain


	MoveToNextFrame(w);

ERRNO_BREAK
}

//cpo from d3d11on12 example to draw atriangle
void Direct3D11Manager::PopulateCommandList(Window *window)
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	HRESULT result = window->m_commandAllocators[m_frameIndex]->Reset();

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	result = m_commandList->Reset(window->m_commandAllocators[m_frameIndex], m_pipelineState);

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature);


	m_commandList->RSSetViewports(1, &(window->m_viewport));
	m_commandList->RSSetScissorRects(1, &(window->m_scissorRect));

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(window->backBufferPtr[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(window->m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, window->m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// Note: do not transition the render target to present here.
	// the transition will occur when the wrapped 11On12 render
	// target resource is released.

	//cpo //todo remove before reinstating d3d11on12
	// Indicate that the back buffer will now be used to present.
	//m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(window->backBufferPtr[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	result = m_commandList->Close();
}



void Direct3D11Manager::SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci, int view_id)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	w->SetRenderer(ci,  view_id);
}

int Direct3D11Manager::GetViewId(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return -1;
	Window *w=windows[hwnd];
	return w->view_id;
}

Window *Direct3D11Manager::GetWindow(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return NULL;
	Window *w=windows[hwnd];
	return w;
}

void Direct3D11Manager::ReportMessageFilterState()
{
	if(!d3dInfoQueue)
		return;
	SIZE_T filterlength;
	d3dInfoQueue->GetStorageFilter(NULL, &filterlength);
	D3D11_INFO_QUEUE_FILTER *filter = (D3D11_INFO_QUEUE_FILTER*)malloc(filterlength);
	memset( filter, 0, filterlength );
	int numfilt=d3dInfoQueue->GetStorageFilterStackSize();
	d3dInfoQueue->GetStorageFilter(filter,&filterlength);
	std::cout<<filter->AllowList.NumSeverities<<std::endl;
	free(filter);
}

void Direct3D11Manager::SetFullScreen(HWND hwnd,bool fullscreen,int which_output)
{
	Window *w=(Window*)GetWindow(hwnd);
	if(!w)
		return;
	IDXGIOutput *output=outputs[which_output];
	if(!w->m_swapChain)
		return;
	BOOL current_fullscreen;
	V_CHECK(w->m_swapChain->GetFullscreenState(&current_fullscreen, NULL));
	if((current_fullscreen==TRUE)==fullscreen)
		return;
	V_CHECK(w->m_swapChain->SetFullscreenState(fullscreen, NULL));
/*	DXGI_OUTPUT_DESC outputDesc;
	V_CHECK(output->GetDesc(&outputDesc));
	int W=0,H=0;
	if(fullscreen)
	{
		W=outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left;
		H=outputDesc.DesktopCoordinates.bottom-outputDesc.DesktopCoordinates.top;
	}
	else
	{
		// Will this work, or will the hwnd just be the size of the full screen initially?
		RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
		GetWindowRect(hwnd,&rect);
#endif
		W	=abs(rect.right-rect.left);
		H=abs(rect.bottom-rect.top);
	}*/
	ResizeSwapChain(hwnd);
}

void Direct3D11Manager::ResizeSwapChain(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	w->ResizeSwapChain(m_d3d12Device);
}





void* Direct3D11Manager::GetDevice()
{
	return d3dDevice;
}

void* Direct3D11Manager::GetDeviceContext()
{
	return d3dDeviceContext;
}

void Direct3D11Manager::AddWindow(HWND hwnd)
{
	if(windows.find(hwnd)!=windows.end())
		return;
	Window *window=new Window;
	windows[hwnd]=window;
	window->hwnd=hwnd;

	window->setCommandQueue(m_commandQueue);
	window->setRootSignature(m_rootSignature);
	window->setD3D11Device(d3dDevice);
	

	crossplatform::Output o=GetOutput(0);
//	window->RestoreDeviceObjects(d3dDevice, m_vsync_enabled, o.numerator, o.denominator);
	window->RestoreDeviceObjects(m_d3d12Device, m_vsync_enabled, o.numerator, o.denominator); //cpo


	//cpo m_rtvDescriptorSize = m_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	LoadAssets(window);
}




//cpo extra set up to render some dx12 stuff
void Direct3D11Manager::LoadAssets(Window *window)
{


	m_frameIndex = window->m_swapChain->GetCurrentBackBufferIndex();


	/*
	// Create an empty root signature.
	{
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(m_d3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	NAME_D3D12_OBJECT(m_rootSignature);
	}
	*/
	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ID3DBlob *vertexShader;
		ID3DBlob *pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif


		//cpo //todo sorrt out fixed path
		D3DCompileFromFile(L"C:\\Simul\\master\\Simul\\Samples\\DirectX11On12Sample\\shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
		D3DCompileFromFile(L"C:\\Simul\\master\\Simul\\Samples\\DirectX11On12Sample\\shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature;
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;							// actually  number of entries in array below (sort of the same)
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		m_d3d12Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		m_pipelineState->SetName(L"pipelinestate");
	}

	m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, window->m_commandAllocators[m_frameIndex], m_pipelineState, IID_PPV_ARGS(&m_commandList));
	//cpo //todo shoul work? m_commandList->SetName(L"window command list");



	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	ID3D12Resource *vertexBufferUpload;

	// Create the vertex buffer.
	{
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		m_d3d12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer));

		m_d3d12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBufferUpload));

		m_vertexBuffer->SetName(L"window vertex buffer");

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = reinterpret_cast<UINT8*>(triangleVertices);
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;

		UpdateSubresources<1>(m_commandList, m_vertexBuffer, vertexBufferUpload, 0, 0, 1, &vertexData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Close the command list and execute it to begin the vertex buffer copy into
	// the default heap.
	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		m_d3d12Device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		m_fenceValues[m_frameIndex]++;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			HRESULT result = HRESULT_FROM_WIN32(GetLastError());
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}
}


//cpo frame and gpu sync functions

// Wait for pending GPU work to complete.
void Direct3D11Manager::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	m_commandQueue->Signal(m_fence, m_fenceValues[m_frameIndex]);

	// Wait until the fence has been processed.
	m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
	DWORD ret = WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValues[m_frameIndex]++;
}

// Prepare to render the next frame.
void Direct3D11Manager::MoveToNextFrame(Window *window)
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	m_commandQueue->Signal(m_fence, currentFenceValue);

	// Update the frame index.
	m_frameIndex = window->m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	DWORD ret = 0x1234F0AD;
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
		ret = WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;

	//cpo pure debug
	m_totalFrames++;
}