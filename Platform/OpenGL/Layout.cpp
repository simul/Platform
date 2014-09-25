#include <GL/glew.h>
#include "Layout.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
using namespace simul;
using namespace opengl;

Layout::Layout():vao(0)
{
}


Layout::~Layout()
{
	InvalidateDeviceObjects();
}

void Layout::InvalidateDeviceObjects()
{
}
			
void Layout::Apply(crossplatform::DeviceContext &)
{
	//glEnableClientState(GL_VERTEX_ARRAY);
	//glClientActiveTexture(GL_TEXTURE0);
//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	GL_ERROR_CHECK
	glBindVertexArray( vao );
	GL_ERROR_CHECK
}
void Layout::Unapply(crossplatform::DeviceContext &)
{
	//glDisableClientState( GL_VERTEX_ARRAY );                // Disable Vertex Arrays
	//glClientActiveTexture(GL_TEXTURE0);
   // glDisableClientState( GL_TEXTURE_COORD_ARRAY );  
	GL_ERROR_CHECK
	glBindVertexArray( 0 ); 
	GL_ERROR_CHECK
}