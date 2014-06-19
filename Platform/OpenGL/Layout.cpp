#include <GL/glew.h>
#include "Layout.h"
using namespace simul;
using namespace opengl;

Layout::Layout()
{
}


Layout::~Layout()
{
	InvalidateDeviceObjects();
}

void Layout::InvalidateDeviceObjects()
{
}
			
void Layout::Apply(crossplatform::DeviceContext &deviceContext)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glClientActiveTexture(GL_TEXTURE0);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
void Layout::Unapply(crossplatform::DeviceContext &deviceContext)
{
	glDisableClientState( GL_VERTEX_ARRAY );                // Disable Vertex Arrays
	glClientActiveTexture(GL_TEXTURE0);
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );   
}