#include "Layout.h"
#include "MacrosDX1x.h"
#include "SimulDirectXHeader.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"

using namespace simul;
using namespace dx11on12;

Layout::Layout():
	d3d11InputLayout(0),
	previousInputLayout(0),
	previousTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED),
	mAppliedCount(0)
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
	mAppliedCount++;
	dx11on12::RenderPlatform* renderPlat = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;
	renderPlat->SetCurrentInputLayout(&Dx12LayoutDesc);
}

void Layout::Unapply(crossplatform::DeviceContext &deviceContext)
{
	if (mAppliedCount == 0)
	{
		SIMUL_BREAK("Calling Unapply without a previous Apply");
		return;
	}
	mAppliedCount--;
	dx11on12::RenderPlatform* renderPlat = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;
	renderPlat->SetCurrentInputLayout(nullptr);
}