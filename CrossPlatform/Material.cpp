#include "Platform/CrossPlatform/Material.h"
#include "Platform/CrossPlatform/Effect.h"

using namespace platform;
using namespace crossplatform;

Material::Material()
{
	transparencyAlpha.value = 1.0f;
}

Material::Material(const char *n)
	: effect(nullptr),name(n)
{
	transparencyAlpha.value = 1.0f;
}

Material::~Material()
{
	InvalidateDeviceObjects();
} 

void Material::InvalidateDeviceObjects()
{
	effect= nullptr;
}

void Material::SetEffect(crossplatform::Effect *e)
{
	effect=e;
}
crossplatform::Effect *Material::GetEffect()
{
	return effect;
}
void Material::Apply(crossplatform::DeviceContext &, crossplatform::PhysicalLightRenderData &)
{
}
