#include "glad/glad.h"
#include "DisplaySurface.h"
#include "RenderPlatform.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

using namespace simul;
using namespace opengl;

#ifdef _MSC_VER
static const char *GetErr()
{
	LPVOID lpMsgBuf;
	DWORD err=GetLastError();
	FormatMessage( 	FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					err,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR) &lpMsgBuf,
					0,
					NULL 
					);
	if(lpMsgBuf)
		return (const char *)lpMsgBuf;
	else
		return "";
}
#endif

DisplaySurface::DisplaySurface()
	:pixelFormat(crossplatform::UNKNOWN)
	,hDC(nullptr)
	,hRC(nullptr)
{
}

DisplaySurface::~DisplaySurface()
{
	InvalidateDeviceObjects();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* r, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
	if (mHwnd && mHwnd == handle)
	{
		return;
	}
	renderPlatform=r;
	pixelFormat=outFmt;
	crossplatform::DeviceContext &immediateContext=r->GetImmediateContext();
	mHwnd							   = handle;
#ifdef _MSC_VER
	if (!(hDC=GetDC(mHwnd)))
	{
		return ;
	}
	static  PIXELFORMATDESCRIPTOR pfd=	// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// Size Of This Pixel Format Descriptor
		1,								// Version Number
		PFD_DRAW_TO_WINDOW |			// Format Must Support Window
		PFD_SUPPORT_OPENGL |			// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,				// Must Support Double Buffering
		PFD_TYPE_RGBA,					// Request An RGBA Format
		32,								// Select Our Color Depth
		0, 0, 0, 0, 0, 0,				// Color Bits Ignored
		0,								// No Alpha Buffer
		0,								// Shift Bit Ignored
		0,								// No Accumulation Buffer
		0, 0, 0, 0,						// Accumulation Bits Ignored
		0,								// 16Bit Z-Buffer (Depth Buffer)
		0,								// No Stencil Buffer
		0,								// No Auxiliary Buffer
		PFD_MAIN_PLANE,					// Main Drawing Layer
		0,								// Reserved
		0, 0, 0							// Layer Masks Ignored
	};
	GLuint glPixelFormat;
	//(HGLRC)immediateContext.platform_context
	HGLRC hglrc	=wglGetCurrentContext();
	HDC hdc		=wglGetCurrentDC();
	int pf		=GetPixelFormat(hdc);
	PIXELFORMATDESCRIPTOR pfd2;
	int maxidx=DescribePixelFormat(
             hdc,
			  pf,
			  sizeof(PIXELFORMATDESCRIPTOR),
			  &pfd2
			);
	if (!(glPixelFormat=ChoosePixelFormat(hDC,&pfd2))) 
	{
		SIMUL_CERR<<"ChoosePixelFormat Failed."<<std::endl;
		return ;
	}
	if(!SetPixelFormat(hDC, pf, &pfd2))
	{
		SIMUL_CERR<<"SetPixelFormat Failed."<<std::endl;
		return ;
	}
	pixelFormat	=RenderPlatform::FromGLFormat(glPixelFormat);
	hRC=hglrc;
	/*
	hRC=wglCreateContext(hDC);
	if(!wglMakeCurrent(nullptr,nullptr))
	{
		SIMUL_CERR<<"wglMakeCurrent Failed."<<GetErr()<<std::endl;
		return ;
	}
	if(!wglShareLists(hRC,hglrc))
	{
		SIMUL_CERR<<"wglShareLists Failed."<<GetErr()<<std::endl;
		return ;
	}*/
	if(!wglMakeCurrent(hDC,hRC))
	{
		SIMUL_CERR<<"wglMakeCurrent Failed."<<GetErr()<<std::endl;
		return ;
	}

#endif
	//mWindow=glfwAttachWin32Window(handle, nullptr);
	// Lets provie a user pointer to the app object
   // glfwSetWindowUserPointer(mWindow, this);
  //  glfwSetWindowSizeCallback(mWindow, GLFWResizeCallback);
  //  glfwMakeContextCurrent(mWindow);
}

void DisplaySurface::InvalidateDeviceObjects()
{
	if(!wglMakeCurrent(NULL,NULL))
	{
		SIMUL_CERR<<"wglMakeCurrent Failed."<<GetErr()<<std::endl;
		return;
	}
	if(!wglDeleteContext(hRC)  )                // Are We Able To Release The DC
	{
		SIMUL_CERR<<"wglDeleteContext Failed."<<GetErr()<<std::endl;
		hRC=NULL;                           // Set DC To NULL
	}
	if (hDC && !ReleaseDC(mHwnd,hDC))                    // Are We Able To Release The DC
	{
		SIMUL_CERR<<"Release Device Context Failed."<<GetErr()<<std::endl;
		hDC=NULL;                           // Set DC To NULL
	}
	hDC=nullptr;                           // Set DC To NULL
}

void DisplaySurface::InitSwapChain()
{
	RECT rect;

#if defined(WINVER) 
	GetWindowRect((HWND)mHwnd, &rect);
#endif

	int screenWidth  = abs(rect.right - rect.left);
	int screenHeight = abs(rect.bottom - rect.top);

	viewport.h		  = screenHeight;
	viewport.w		  = screenWidth;
	viewport.x		  = 0;
	viewport.y		  = 0;

	// Initialize the swap chain description.
}

void DisplaySurface::Render()
{
	crossplatform::DeviceContext &immediateContext=renderPlatform->GetImmediateContext();
	deferredContext.platform_context=immediateContext.platform_context;
	deferredContext.renderPlatform=renderPlatform;
	
	HGLRC hglrc	=wglGetCurrentContext();
	//glfwMakeContextCurrent(mWindow);
	if(!wglMakeCurrent(hDC,hRC))
		return;

	renderPlatform->StoreRenderState(deferredContext);

	static vec4 clear = { 0.0f,0.0f,1.0f,1.0f};
	glViewport(0, 0, viewport.w, viewport.h);   
    glClearColor(clear.x,clear.y,clear.z,clear.w);
	glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //  mDeferredContext->ClearRenderTargetView(mBackBufferRT, clear);
 //   mDeferredContext->RSSetViewports(1, &mViewport);
	if(renderer)
		renderer->Render(mViewId, 0, 0,viewport.w, viewport.h);

	//mDeferredContext->OMSetRenderTargets(0, nullptr, nullptr);
	renderPlatform->RestoreRenderState(deferredContext);
	//mDeferredContext->FinishCommandList(true,&mCommandList);          // Draw The Scene
	SwapBuffers(hDC); 
	wglMakeCurrent(hDC,hglrc);
}

void DisplaySurface::EndFrame()
{
	// We check for resize here, because we must manage the SwapChain from the main thread.
	// we may have to do it after executing the command list, because Resize destroys the CL, and we don't want to lose commands.
	Resize();
}

void DisplaySurface::Resize()
{
	RECT rect;
	InitSwapChain();
#if defined(WINVER) 
	if (!GetWindowRect((HWND)mHwnd, &rect))
		return;
#endif
	UINT W = abs(rect.right - rect.left);
	UINT H = abs(rect.bottom - rect.top);
	if (viewport.w == W&&viewport.h == H)
		return;
	
	viewport.w		  = W;
	viewport.h		  = H;
	viewport.x		  = 0;
	viewport.y		  = 0;

	renderer->ResizeView(mViewId,W,H);
}