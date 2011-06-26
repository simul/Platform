#include "Simul/Platform/OpenGL/SimulGLUtilities.h"

#include <windows.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include "Simul/Base/Timer.h"

void CheckExtension(const char *txt)
{
   if(!glewIsSupported(txt))
    {
		std::cerr<<"Error - required OpenGL extension is not supported: "<<txt<<std::endl;
        exit(-1);
    }
}

static int win_h=0;
void SetOrthoProjection(int w,int h)
{
	win_h=h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,w,0,h,-1.0,1.0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0,0,w,h);
}

void SetPerspectiveProjection(int w,int h,float field_of_view)
{
	win_h=h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(field_of_view,(float)w/(float)h,1.0,325000.0);
	glViewport(0,0,w,h);
}
void RenderString(float x, float y, void *font, const char* string)
{
	glColor4f(1.f,1.f,1.f,1.f); 
	glRasterPos2f(x,win_h-y);

	const char *s=string;
	while(*s)
	{
		if(*s=='\n')
		{
			y+=12;
			glRasterPos2f(x,win_h-y);
		}
		else
			glutBitmapCharacter(font,*s);
		s++;
	}
}

float GetFramerate()
{
	static float framerate=100.f;
#if 1
	static simul::base::Timer timer;
	timer.FinishTime();
	framerate*=.99f;
	framerate+=0.01f*(1000.f/timer.Time);
	timer.StartTime();
#else
	static int count=0;
	static int timeprev=0;
	int time_ms=glutGet(GLUT_ELAPSED_TIME);
	int timediff=0;
	if(timeprev)
		timediff=time_ms - timeprev;
	timeprev=time_ms;
	count++;
	if(timediff)
	{
		framerate+=0.01f*(1000.f/(float)timediff*(float)count);
		framerate*=.99f;
		count=0;
	}
	else
		count++;
#endif
	return framerate;
}

void SetVSync(int vsync)
{
#ifdef WIN32
	// Enable or Disable vsync:
	typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
	typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
	wglSwapIntervalEXT=(PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
	wglSwapIntervalEXT(vsync);
#endif
}

// draw a quad with texture coordinate for texture rectangle
void DrawQuad(int w,int h)
{
    glBegin(GL_QUADS);
    glTexCoord2f(0.0,1.0);
    glVertex2f(0.0,h);
    glTexCoord2f(1.0,1.0);
    glVertex2f(w,h);
    glTexCoord2f(1.0,0.0);
    glVertex2f(w,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex2f(0.0,0.0);
    glEnd();
}

void CheckGLError()
{
	if(int err=glGetError()!=0)
	{
		const char *c=(const char*)gluErrorString(err);
		if(c)
			std::cerr<<std::endl<<c<<std::endl;
		else
			std::cerr<<std::endl<<"unknown error: "<<err<<std::endl;
		DebugBreak();
		assert(0);
	}
}