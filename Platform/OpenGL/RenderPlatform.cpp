#include "GL/glew.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Material.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Scene/Light.h"
#include "Simul/Scene/Texture.h"

using namespace simul;
using namespace opengl;
RenderPlatform::RenderPlatform()
{
}

RenderPlatform::~RenderPlatform()
{
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const GLfloat BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

scene::MaterialCache *RenderPlatform::CreateMaterial()
{
	return new opengl::Material;
}

scene::Mesh *RenderPlatform::CreateMesh()
{
	return new scene::Mesh;
}

scene::LightCache *RenderPlatform::CreateLight()
{
	return new scene::LightCache();
}

scene::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	scene::Texture * tex=new opengl::Texture;
	tex->LoadFromFile(fileNameUtf8);
	return tex;
}