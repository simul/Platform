#include "Layout.h"
#include "SimulDirectXHeader.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/DirectX12/RenderPlatform.h"

using namespace platform;
using namespace dx12;

Layout::Layout():
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

}

void Layout::Apply(crossplatform::DeviceContext &deviceContext)
{
	mAppliedCount++;
	dx12::RenderPlatform* renderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
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
	dx12::RenderPlatform* renderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	renderPlat->SetCurrentInputLayout(nullptr);
}