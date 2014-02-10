#include <GL/glew.h>
#include "SimulGLUnderwaterRenderer.h"
using namespace simul;
using namespace opengl;

SimulGLUnderwaterRenderer::SimulGLUnderwaterRenderer()
{
}

SimulGLUnderwaterRenderer::~SimulGLUnderwaterRenderer()
{
}

void SimulGLUnderwaterRenderer::Render(bool blend)
{
	EnsureTexturesAreUpToDate();
}

void SimulGLUnderwaterRenderer::RecompileShaders()
{

}

void SimulGLUnderwaterRenderer::EnsureCorrectTextureSizes()
{
}

void SimulGLUnderwaterRenderer::EnsureTexturesAreUpToDate()
{
}

void SimulGLUnderwaterRenderer::EnsureTextureCycle()
{
}

bool SimulGLUnderwaterRenderer::IsYVertical() const
{
	return false;
}
