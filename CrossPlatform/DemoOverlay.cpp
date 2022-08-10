#include "DemoOverlay.h"
#include "Texture.h"
#include "RenderPlatform.h"
#include "DeviceContext.h"
#include "Macros.h"
using namespace platform;
using namespace crossplatform;

DemoOverlay::DemoOverlay()
	:texture1(NULL)
	,texture2(NULL)
	,renderPlatform(NULL)
{
}

DemoOverlay::~DemoOverlay()
{
	InvalidateDeviceObjects();
}


void DemoOverlay::RestoreDeviceObjects(RenderPlatform *r)
{
	renderPlatform =r;
	SAFE_DELETE(texture1);
	SAFE_DELETE(texture2);
	texture1=renderPlatform->CreateTexture("RealTimeTextOverlay.png");
	texture1->width=544;
	texture1->length=120;
	texture2=renderPlatform->CreateTexture("TrueSkyTextOverlay.png");
	texture2->width=444;
	texture2->length=120;
}

void DemoOverlay::InvalidateDeviceObjects()
{
	SAFE_DELETE(texture1);
	SAFE_DELETE(texture2);
	renderPlatform=NULL;
}

void DemoOverlay::Render(GraphicsDeviceContext &deviceContext)
{
	if(!texture2||!texture1)
		return;
	crossplatform::Viewport V=renderPlatform->GetViewport(deviceContext,0);
	renderPlatform->DrawTexture(deviceContext,8,8,texture2->width,texture2->length,texture2,1.f,true);
	renderPlatform->DrawTexture(deviceContext,V.w-8-texture1->width,V.h-8-texture1->length,texture1->width,texture1->length,texture1,1.f,true);
}
