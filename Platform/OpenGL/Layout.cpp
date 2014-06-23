#include <GL/glew.h>
#include "Layout.h"
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
	glBindVertexArray( vao );
}
void Layout::Unapply(crossplatform::DeviceContext &)
{
	//glDisableClientState( GL_VERTEX_ARRAY );                // Disable Vertex Arrays
	//glClientActiveTexture(GL_TEXTURE0);
   // glDisableClientState( GL_TEXTURE_COORD_ARRAY );  
	glBindVertexArray( 0 ); 
}