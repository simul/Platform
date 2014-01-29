#include "Direct3D11Manager.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"

using namespace simul;
using namespace dx11;

Window::Window():
	hwnd(0)
	,vsync(false)
	,m_swapChain(0)
	,m_renderTargetView(0)
	,m_depthStencilBuffer(0)
	,m_depthStencilState(0)
	,m_depthStencilView(0)
	,m_rasterState(0)
	,renderer(NULL)
{
}

Window::~Window()
{
	Release();
}
void Window::RestoreDeviceObjects(ID3D11Device* d3dDevice)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	D3D11_RASTERIZER_DESC rasterDesc;

	RECT rect;
	GetWindowRect(hwnd,&rect);
	int screenWidth	=abs(rect.right-rect.left);
	int screenHeight=abs(rect.bottom-rect.top);

	//Now that we have the refresh rate from the system we can start the DirectX initialization.
	//The first thing we'll do is fill out the description of the swap chain.
	//The swap chain is the front and back buffer to which the graphics will be drawn.
	//Generally you use a single back buffer, do all your drawing to it, and then swap it to the front buffer which then displays
	// on the user's screen. That is why it is called a swap chain.

	// Initialize the swap chain description.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// Set to a single back buffer.
	swapChainDesc.BufferCount = 1;

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
	//if(m_vsync_enabled)
	{
	//	swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
	//	swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
//	else
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
	swapChainDesc.BufferDesc.ScanlineOrdering	= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling			= DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	swapChainDesc.Flags = 0;
	// Sometimes this call to create the device will fail if the primary video card is not compatible with DirectX 11.
	// Some machines may have the primary card as a DirectX 10 video card and the secondary card as a DirectX 11 video card.
	// Also some hybrid graphics cards work that way with the primary being the low power Intel card and the secondary being
	// the high power Nvidia card. To get around this you will need to not use the default device and instead enumerate all
	// the video cards in the machine and have the user choose which one to use and then specify that card when creating the device.

	// Now that we have a swap chain we need to get a pointer to the back buffer and then attach it to the swap chain.
	// We'll use the CreateRenderTargetView function to attach the back buffer to our swap chain.

	// Get the pointer to the back buffer.
	HRESULT result;
	IDXGIFactory* factory;
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	SIMUL_ASSERT(result==S_OK);
	factory->CreateSwapChain(d3dDevice,&swapChainDesc,&m_swapChain);
	SAFE_RELEASE(factory);

	CreateRenderTarget(d3dDevice);
	CreateDepthBuffer(d3dDevice);
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

	// Create the rasterizer state from the description we just filled out.
	result = d3dDevice->CreateRasterizerState(&rasterDesc, &m_rasterState);
	SIMUL_ASSERT(result==S_OK);
}

void Window::CreateRenderTarget(ID3D11Device* d3dDevice)
{
	ID3D11Texture2D* backBufferPtr;
	HRESULT result = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	SIMUL_ASSERT(result==S_OK);
	// Create the render target view with the back buffer pointer.
	SAFE_RELEASE(m_renderTargetView);
	result = d3dDevice->CreateRenderTargetView(backBufferPtr, NULL, &m_renderTargetView);
	SIMUL_ASSERT(result==S_OK);
	// Release pointer to the back buffer as we no longer need it.
	SAFE_RELEASE(backBufferPtr);
}

void Window::CreateDepthBuffer(ID3D11Device* d3dDevice)
{
	RECT rect;
	GetWindowRect(hwnd,&rect);
	int screenWidth	=abs(rect.right-rect.left);
	int screenHeight=abs(rect.bottom-rect.top);
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
	HRESULT result = d3dDevice->CreateTexture2D(&depthBufferDesc, NULL, &m_depthStencilBuffer);
	SIMUL_ASSERT(result==S_OK);
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
	depthStencilDesc.BackFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp= D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc		= D3D11_COMPARISON_ALWAYS;
//With the description filled out we can now create a depth stencil state.

	// Create the depth stencil state.
	result = d3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
	SIMUL_ASSERT(result==S_OK);
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
	result = d3dDevice->CreateDepthStencilView(m_depthStencilBuffer, &depthStencilViewDesc, &m_depthStencilView);
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

void Window::SetRenderer(Direct3D11CallbackInterface *ci)
{
	if(renderer==ci)
		return;
	if(renderer)
	{
		renderer->RemoveView(view_id);
	}
	renderer=ci;
	DXGI_SWAP_CHAIN_DESC swapDesc;
	DXGI_SURFACE_DESC surfaceDesc;
	m_swapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	if(view_id<0)
		view_id=renderer->AddView();
	renderer->ResizeView(view_id,&surfaceDesc);
}

void Window::Release()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if(m_swapChain)
		m_swapChain->SetFullscreenState(false, NULL);
	SAFE_RELEASE(m_swapChain);
	SAFE_RELEASE(m_renderTargetView);
	SAFE_RELEASE(m_depthStencilBuffer);
	SAFE_RELEASE(m_depthStencilState);
	SAFE_RELEASE(m_depthStencilView);
	SAFE_RELEASE(m_rasterState);
}

Direct3D11Manager::Direct3D11Manager()
	:d3dDevice(0)
	,d3dDeviceContext(0)
{
}


Direct3D11Manager::~Direct3D11Manager()
{
	Shutdown();
}

void Direct3D11Manager::Initialize()
{
	HRESULT result;
	IDXGIFactory* factory;
	int  i;//, numerator, denominator;
	DXGI_ADAPTER_DESC adapterDesc;
	int error;
	D3D_FEATURE_LEVEL featureLevel;

	// Store the vsync setting.
	m_vsync_enabled = true;
	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	SIMUL_ASSERT(result==S_OK);

	// Use the factory to create an adapter for the primary graphics interface (video card).
	SAFE_RELEASE(adapter);
	result = factory->EnumAdapters(0, &adapter);
	SIMUL_ASSERT(result==S_OK);

	// Enumerate the primary adapter output (monitor).
	//result = adapter->EnumOutputs(0, &adapterOutput);
	IDXGIOutput* output=NULL;
	 i=0;
	while(adapter->EnumOutputs(i,&output)!=DXGI_ERROR_NOT_FOUND)
	{
		outputs[i]=output;
		SIMUL_ASSERT(result==S_OK);
		i++;
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
	error = wcstombs_s(&stringLength, m_videoCardDescription, 128, adapterDesc.Description, 128);
	SIMUL_ASSERT(error==0);
	//Now that we have stored the numerator and denominator for the refresh rate and the video card information we can release the structures and interfaces used to get that information.

	// Release the factory.
	SAFE_RELEASE(factory);

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
#ifdef _DEBUG
	//flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
	result=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,flags, &featureLevel,1,D3D11_SDK_VERSION,&d3dDevice, NULL,&d3dDeviceContext);
	SIMUL_ASSERT(result==S_OK);
}

int Direct3D11Manager::GetNumOutputs()
{
	return outputs.size();
}

Output Direct3D11Manager::GetOutput(int i)
{
	unsigned numModes;
	IDXGIOutput *output=outputs[i];
	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	HRESULT result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	SIMUL_ASSERT(result==S_OK);

	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
	Output o;
	o.width		=abs(outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left);
	o.height	=abs(outputDesc.DesktopCoordinates.top-outputDesc.DesktopCoordinates.bottom);

	// Now get extended information about what monitor this is:

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
				unsigned numerator = displayModeList[i].RefreshRate.Numerator;
				unsigned denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}
	// Release the display mode list.
	delete [] displayModeList;
	displayModeList = 0;
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
	SAFE_RELEASE(d3dDevice);
	SAFE_RELEASE(d3dDeviceContext);
	return;
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

void Direct3D11Manager::StartRendering(HWND h)
{
	if(windows.find(h)==windows.end())
		return;
	Window *w=windows[h];
	if(!w)
		return;
	// Set the depth stencil state.
	d3dDeviceContext->OMSetDepthStencilState(w->m_depthStencilState, 1);
	// Bind the render target view and depth stencil buffer to the output render pipeline.
	d3dDeviceContext->OMSetRenderTargets(1, &w->m_renderTargetView,w->m_depthStencilView);
	// Create the viewport.
	d3dDeviceContext->RSSetViewports(1, &w->viewport);
	// Now set the rasterizer state.
	d3dDeviceContext->RSSetState(w->m_rasterState);
	if(w->renderer)
		w->renderer->Render(w->view_id,GetDevice(),GetDeviceContext());
}

void Direct3D11Manager::SetRenderer(HWND hwnd,Direct3D11CallbackInterface *ci)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	w->SetRenderer(ci);
}

Window *Direct3D11Manager::GetWindow(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return NULL;
	Window *w=windows[hwnd];
	return w;
}

void Direct3D11Manager::SetFullScreen(HWND hwnd,bool fullscreen,int which_output)
{
	Window *w=GetWindow(hwnd);
	if(!w)
		return;
	IDXGIOutput *output=outputs[which_output];
	if(!w->m_swapChain)
		return;
	BOOL current_fullscreen;
	w->m_swapChain->GetFullscreenState(&current_fullscreen,NULL);
	if((current_fullscreen==TRUE)==fullscreen)
		return;
	w->m_swapChain->SetFullscreenState(fullscreen,output);
	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
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
		GetWindowRect(hwnd,&rect);
		W	=abs(rect.right-rect.left);
		H=abs(rect.bottom-rect.top);
	}
	ResizeSwapChain(hwnd,W,H);
}

void Direct3D11Manager::ResizeSwapChain(HWND hwnd,int width,int height)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
		RECT rect;
		GetWindowRect(hwnd,&rect);
		int W	=abs(rect.right-rect.left);
		int H	=abs(rect.bottom-rect.top);
	ID3D11RenderTargetView *t[]={NULL};
	//d3dDeviceContext->OMSetRenderTargets(1,t,NULL);
	SAFE_RELEASE(w->m_renderTargetView);
	DXGI_SWAP_CHAIN_DESC swapDesc;
	w->m_swapChain->GetDesc(&swapDesc);
	HRESULT hr=w->m_swapChain->ResizeBuffers(1,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0);
	w->CreateRenderTarget(d3dDevice);
	w->CreateDepthBuffer(d3dDevice);
	
	DXGI_SURFACE_DESC surfaceDesc;
	w->m_swapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	if(w->renderer)
		w->renderer->ResizeView(w->view_id,&surfaceDesc);
	SIMUL_ASSERT(hr==S_OK);
}

ID3D11Device* Direct3D11Manager::GetDevice()
{
	return d3dDevice;
}

ID3D11DeviceContext* Direct3D11Manager::GetDeviceContext()
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
	window->RestoreDeviceObjects(d3dDevice);
}