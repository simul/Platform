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

void Material::SetEffect(const std::shared_ptr<crossplatform::Effect> &e)
{
	effect=e;
}
std::shared_ptr<crossplatform::Effect> Material::GetEffect()
{
	return effect;
}
void Material::Apply(crossplatform::DeviceContext &, crossplatform::PhysicalLightRenderData &)
{
}
