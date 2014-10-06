

#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h> // for uintptr_t

#include <GL/glew.h>

#include <fstream>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

#include "SimulGLSkyRenderer.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Camera/Camera.h"
using namespace simul;
using namespace opengl;

#if 0//def _MSC_VER
GLenum sky_tex_format=GL_HALF_FLOAT_NV;
GLenum internal_format=GL_RGBA16F_ARB;
#else
GLenum sky_tex_format=GL_FLOAT;
GLenum internal_format=GL_RGBA32F_ARB;
#endif


SimulGLSkyRenderer::SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m)
	:BaseSkyRenderer(sk,m)
{

}
// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades(crossplatform::DeviceContext &deviceContext)
{
GL_ERROR_CHECK
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	renderPlatform->SetStandardRenderState(deviceContext,crossplatform::STANDARD_OPAQUE_BLENDING);
GL_ERROR_CHECK
	BaseSkyRenderer::Render2DFades(deviceContext);
GL_ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	return true;
}

void SimulGLSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	BaseSkyRenderer::RestoreDeviceObjects(r);
}
