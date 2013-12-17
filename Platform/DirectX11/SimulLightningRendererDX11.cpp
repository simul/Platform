
#define NOMINMAX
#include "SimulLightningRendererDX11.h"
using namespace simul;
using namespace dx11;
SimulLightningRendererDX11::SimulLightningRendererDX11(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk)
	:BaseLightningRenderer(ck,sk)
{
}

SimulLightningRendererDX11::~SimulLightningRendererDX11()
{
}

void SimulLightningRendererDX11::RestoreDeviceObjects(void* dev)
{
}
void SimulLightningRendererDX11::RecompileShaders()
{
}
void SimulLightningRendererDX11::InvalidateDeviceObjects()
{
}
void SimulLightningRendererDX11::SetMatrices(const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj)
{
}
void SimulLightningRendererDX11::Render(void *context)
{
}