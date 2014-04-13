#include "GL/glew.h"
#include "Material.h"
#include "Texture.h"
#include <fbxsdk.h>

using namespace simul;
using namespace opengl;


Material::Material()
{
}


Material::~Material()
{
}

void Material::Apply() const
{
	glActiveTexture(GL_TEXTURE0);
//	float zero[]	={0,0,0,0};
    glMaterialfv(GL_FRONT_AND_BACK	,GL_EMISSION	,mEmissive.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_AMBIENT		,mAmbient.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_DIFFUSE		,mDiffuse.mColor);
    glMaterialfv(GL_FRONT_AND_BACK	,GL_SPECULAR	,mSpecular.mColor);
    glMaterialf	(GL_FRONT_AND_BACK	,GL_SHININESS	,mShininess);
	if(mDiffuse.mTextureName)
		glBindTexture(GL_TEXTURE_2D	,((opengl::Texture *)mDiffuse.mTextureName)->pTextureObject);
	else
		glBindTexture(GL_TEXTURE_2D	,0);
	for(int i=1;i<2;i++)
	{
		glActiveTexture(GL_TEXTURE0+i);
		glBindTexture(GL_TEXTURE_2D	,((opengl::Texture *)mDiffuse.mTextureName)->pTextureObject);
	}
	glActiveTexture(GL_TEXTURE0);
}