#include "Material.h"
#include "Texture.h"
#include "Utilities.h"

using namespace simul;
using namespace dx11;

Material::Material()
	:effect(NULL)
{
}


Material::~Material()
{
}

void Material::Apply(crossplatform::DeviceContext &deviceContext) const
{
/*	glActiveTexture(GL_TEXTURE0);
	float zero[]	={0,0,0,0};
    glMaterialfv(GL_FRONT_AND_BACK	,GL_EMISSION	,mEmissive.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_AMBIENT		,mAmbient.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_DIFFUSE		,mDiffuse.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_SPECULAR	,mSpecular.mColor);
    glMaterialf	(GL_FRONT_AND_BACK	,GL_SHININESS	,mShininess);
	if(mDiffuse.mTextureName)
		glBindTexture(GL_TEXTURE_2D	,((dx11::Texture *)mDiffuse.mTextureName)->shaderResourceView);
	else
		glBindTexture(GL_TEXTURE_2D	,0);
	for(int i=1;i<2;i++)
	{
		glActiveTexture(GL_TEXTURE0+i);
		glBindTexture(GL_TEXTURE_2D	,((dx11::Texture *)mDiffuse.mTextureName)->shaderResourceView);
	}
	glActiveTexture(GL_TEXTURE0);*/
	if(mDiffuse.mTextureName)
		dx11::setTexture(effect,"diffuseTexture",((dx11::Texture *)mDiffuse.mTextureName)->shaderResourceView);
}