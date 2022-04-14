#include "Layout.h"
#include "MacrosDX1x.h"
#include "DirectXHeader.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
using namespace platform;
using namespace dx11;

Layout::Layout()
	:d3d11InputLayout(0)
{
}

Layout::~Layout()
{
	InvalidateDeviceObjects();
}

void Layout::InvalidateDeviceObjects()
{
	SAFE_RELEASE(d3d11InputLayout);
}

void Layout::Apply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=0)
		SIMUL_BREAK("Layout::Apply without a corresponding Unapply!");
	apply_count++;
	deviceContext.asD3D11DeviceContext()->IASetInputLayout(AsD3D11InputLayout());
}

void Layout::Unapply(crossplatform::DeviceContext &)
{
	if(apply_count<=0)
		SIMUL_BREAK("Layout::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Layout::Apply has been called too many times!")
	apply_count--;
}