#include "Material.h"
#include "Texture.h"
#include "Utilities.h"

using namespace simul;
using namespace dx12;

Material::Material()
{
}


Material::~Material()
{
}

void Material::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
/*  EMISSION	mEmissive.mColor);
    AMBIENT		mAmbient.mColor);
    DIFFUSE		mDiffuse.mColor);
    SPECULAR	mSpecular.mColor);
    SHININESS	mShininess);*/
	if(effect)
	{
		if(mDiffuse.mTextureName)
			effect->SetTexture(deviceContext,"diffuseTexture",mDiffuse.mTextureName);
		effect->SetTexture(deviceContext,"diffuseCubemap",physicalLightRenderData.diffuseCubemap);
	}
}