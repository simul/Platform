#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

using namespace simul;
using namespace crossplatform;

Material::Material() : mShininess(0)
	,effect(NULL)
{

}

Material::~Material()
{
	InvalidateDeviceObjects();
} 

void Material::Apply(crossplatform::DeviceContext &, crossplatform::PhysicalLightRenderData &)
{
}

void Material::InvalidateDeviceObjects()
{
	effect=NULL;
}

void Material::SetEffect(crossplatform::Effect *e)
{
	effect=e;
}
crossplatform::Effect *Material::GetEffect()
{
	return effect;
}