#include "Layout.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace opengl;

Layout::Layout()
{
}

Layout::~Layout()
{
	InvalidateDeviceObjects();
}

void Layout::InvalidateDeviceObjects()
{
}
			
void Layout::Apply(crossplatform::DeviceContext& deviceContext)
{
}

void Layout::Unapply(crossplatform::DeviceContext& deviceContext)
{
}