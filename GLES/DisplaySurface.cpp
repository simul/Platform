
#include "DisplaySurface.h"
#include "RenderPlatform.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using namespace simul;
using namespace gles;

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
		PFD_SUPPORT_OPENGL |			// Format Must Support GL
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
	pixelFormat	=RenderPlatform::FromGLInternalFormat(glPixelFormat);
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
}

void DisplaySurface::InvalidateDeviceObjects()
{
#ifdef _MSC_VER
	if (!wglGetCurrentContext())
	{
#ifdef _DEBUG
		SIMUL_COUT<<"wglGetCurrentContext() returned NULL."<<std::endl; //glfwMakeCurrentContext(nullptr) or DeviceManager::Deactivate() would have been called - AJR
#endif 
		return;
	}
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
#endif
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

void DisplaySurface::Render(simul::base::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber)
{
	crossplatform::DeviceContext &immediateContext=renderPlatform->GetImmediateContext();
	deferredContext.platform_context=immediateContext.platform_context;
	deferredContext.renderPlatform=renderPlatform;
	
	HGLRC hglrc	=wglGetCurrentContext();
	if(!wglMakeCurrent(hDC,hRC))
		return;

	renderPlatform->StoreRenderState(deferredContext);

	glViewport(0, 0, viewport.w, viewport.h);   
	if(renderer)
		renderer->Render(mViewId, 0, 0,viewport.w, viewport.h,frameNumber);

	renderPlatform->RestoreRenderState(deferredContext);
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