#include "Overlay.h"
#include "Simul/Base/StringFunctions.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <string>

static void PrintText(const char * pText)
{
    // Push OpenGL attributes.
    glPushAttrib(GL_ENABLE_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glPushAttrib(GL_TEXTURE_BIT);

    glEnable(GL_TEXTURE_2D);

    // Blend with background color
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Visible for double side
    glDisable(GL_CULL_FACE);

    // Blend with foreground color
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
//	glRasterPos3f(x,y,z);
    const char * pCharacter = pText;
    while (*pCharacter != '\0')
    {
       glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*pCharacter);
        ++pCharacter;
    }
    glPopMatrix();

    // Pop OpenGL attributes.
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
}


Overlay::Overlay()
{
}


Overlay::~Overlay()
{
}

void Overlay::RenderGrid(const float *pTransform)
{
	glPushMatrix();
	glMultMatrixf(pTransform);

	// Draw a grid 500*500
	glColor3f(0.3f, 0.3f, 0.3f);
	glLineWidth(1.0);
	const int hw = 500;
	const int step = 20;
	const int bigstep = 100;
	int       i;

	// Draw Grid
	for (i = -hw; i <= hw; i+=step)
	{

		if (i % bigstep == 0)
		{
			glLineWidth(2.0);
		} else {
			glLineWidth(1.0);
		}
		glBegin(GL_LINES);
		glVertex3i(i,-hw,0);
		glVertex3i(i,hw ,0);
		glEnd();
		glBegin(GL_LINES);
		glVertex3i(-hw,i,0);
		glVertex3i(hw ,i,0);
		glEnd();

	}

	// Write some grid info
	const GLfloat yoffset = -2.f;
	const GLfloat xoffset = 1.f;
	for (i = -hw; i <= hw; i+=bigstep)
	{
		std::string scoord;
		int lCount;

		// Don't display origin
		//if (i == 0) continue;
		if (i == 0)
		{
			scoord = "0";
			lCount = (int)scoord.length();
			glPushMatrix();
			//glVertex3f(i+xoffset,yoffset,0);
	glRasterPos3f(i+xoffset,yoffset,0);
			//glRotatef(-90,1,0,0);

			PrintText(scoord.c_str());

			glPopMatrix();
			continue;
		}

		// X coordinates
		scoord = "X: ";
		scoord += simul::base::stringFormat("%g",i);
		lCount = (int)scoord.length();

		glPushMatrix();
	glRasterPos3f(i+xoffset,yoffset,0);
	//	glTranslatef(i+xoffset,yoffset,0);
		//glRotatef(-90,1,0,0);
		PrintText(scoord.c_str());
		glPopMatrix();

		// Z coordinates
		scoord = "Y: ";
		scoord += simul::base::stringFormat("%g",i);
		lCount = (int)scoord.length();

		glPushMatrix();
	//	glTranslatef(xoffset,i+yoffset,0);
	glRasterPos3f(xoffset,i+yoffset,0);
	//	glRotatef(-90,1,0,0);
		PrintText(scoord.c_str());
		glPopMatrix();

	}
	glPopMatrix();
}


