#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Sky/Float4.h"
#include <stdlib.h>
#include <GL/gl.h>
#include <iostream>
#include "Simul/Base/Timer.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include <math.h>
#include <string.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "Simul/Math/Pi.h"
#ifndef _MSC_VER
#define DebugBreak()
#endif
using namespace simul;
using namespace opengl;

#ifndef _MSC_VER
#define DebugBreak()
#endif
using namespace simul;
using namespace opengl;

int Utilities::instance_count=0;
int Utilities::screen_width=0;
int Utilities::screen_height=0;
Utilities *Utilities::ut=NULL;
static int num_warnings=0;
#define MAX_GL_WARNINGS (20)
struct UtKiller
{
	~UtKiller()
	{
		Utilities::Kill();
	}
};
UtKiller utk;

Utilities &Utilities::GetSingleton()
{
	if(!ut)
		ut=new Utilities();
	return *ut;
}
void Utilities::Kill()
{
	delete ut;
	ut=NULL;
}

Utilities::Utilities()
{
	instance_count++;
	if(instance_count==1)
		RestoreDeviceObjects(NULL);
}

void Utilities::RestoreDeviceObjects(void *)
{
}

void Utilities::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void Utilities::InvalidateDeviceObjects()
{
}

Utilities::~Utilities()
{
	if(!instance_count)
		Utilities::InvalidateDeviceObjects();
}

void simul::opengl::RenderTexture(int x,int y,int w,int h)
{
	GL_ERROR_CHECK		
	int prog=0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0.f,0.f);
	glVertex2f((float)x,(float)y);
	glTexCoord2f(1.f,0.f);
	glVertex2f((float)(x+w),(float)y);
	glTexCoord2f(0.f,1.f);
	glVertex2f((float)x,(float)(y+h));
	glTexCoord2f(1.f,1.f);
	glVertex2f((float)(x+w),(float)(y+h));
	glEnd();
	GL_ERROR_CHECK
}

bool simul::opengl::IsExtensionSupported(const char *name)
{
	GLint n=0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (GLint i=0; i<n; i++)
	{
		const char* extension=(const char*)glGetStringi(GL_EXTENSIONS, i);
		if (!strcmp(name,extension))
		{
			//std::cout<<"GL Extension supported: "<<extension<<std::endl;
			return true;
		}
	}
	return false;
}

bool simul::opengl::CheckExtension(const char *txt)
{
GL_ERROR_CHECK
	if(!glewIsSupported(txt)&&!IsExtensionSupported(txt))
	{
		std::cerr<<"Error - required OpenGL extension is not supported: "<<txt<<std::endl;
GL_ERROR_CHECK
		return false;
	}
GL_ERROR_CHECK
	return true;
}

static int win_h=0;

void simul::opengl::SetVSync(int vsync)
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

void simul::opengl::DrawQuad(int x,int y,int w,int h)
{
	DrawQuad((float)x,(float)y,(float)w,(float)h);
}

// draw a quad with texture coordinate for texture rectangle
void simul::opengl::DrawQuad(float x,float y,float w,float h)
{
GL_ERROR_CHECK
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0.0,1.0);
	glVertex2f(x,y+h);
	glTexCoord2f(1.0,1.0);
	glVertex2f(x+w,y+h);
	glTexCoord2f(0.0,0.0);
	glVertex2f(x,y);
	glTexCoord2f(1.0,0.0);
	glVertex2f(x+w,y);
	glEnd();
GL_ERROR_CHECK
}
void simul::opengl::DrawFullScreenQuad()
{
	DrawQuad(0.f,0.f,1.f,1.f);
}

float simul::opengl::GetFramerate()
{
	static float framerate=0.f;
#if 1
	static simul::base::Timer timer;
	timer.FinishTime();
	framerate*=.99f;
	if(framerate)
		framerate+=0.01f*(1000.f/timer.Time);
	else
		framerate=60.f;
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

void simul::opengl::ResetGLError()
{
	glGetError();
}

void simul::opengl::CheckGLError(const char *filename,int line_number)
{
	int err=glGetError();
	if(err!=0)
	{
		CheckGLError(filename,line_number,err);
	}
}

void simul::opengl::CalcCameraPosition(float *cam_pos,float *cam_dir)
{
	simul::math::Matrix4x4 modelview;
	glGetFloatv(GL_MODELVIEW_MATRIX,modelview.RowPointer(0));
	GL_ERROR_CHECK
	simul::math::Matrix4x4 inv;
	modelview.Inverse(inv);
	cam_pos[0]=inv(3,0);
	cam_pos[1]=inv(3,1);
	cam_pos[2]=inv(3,2);
	if(cam_dir)
	{
		cam_dir[0]=-inv(2,0);
		cam_dir[1]=-inv(2,1);
		cam_dir[2]=-inv(2,2);
	}
}

bool simul::opengl::RenderAngledQuad(const float *dir,float half_angle_radians)
{
		GL_ERROR_CHECK
	float cam_dir[3],cam_pos[3];
	CalcCameraPosition(cam_pos,cam_dir);
	float Yaw=atan2(dir[0],dir[1]);
	float Pitch=asin(dir[2]);


	simul::math::Matrix4x4 modelview;
		GL_ERROR_CHECK
	glTranslatef(cam_pos[0],cam_pos[1],cam_pos[2]);
		
	glRotatef(180.f*Yaw/pi,0.0f,0.0f,-1.0f);
		
	glRotatef(180.f*Pitch/pi,1.0f,0.0f,0.0f);
		
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
#if 1
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	static float relative_distance=1000.f;
	simul::math::Matrix4x4 proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	//float zFar=proj(3,2)/(1.f+proj(2,2));
	float zNear=proj(3,2)/(proj(2,2)-1.f);
	float w=relative_distance*zNear;
	float d=w/tan(half_angle_radians);
	struct Vertext
	{
		float x,y,z;
		float tx,ty;
	};
	Vertext vertices[4]=
	{
		{ w,d,-w,	 1.f	,1.f},
		{ w,d, w,	 1.f	,0.f},
		{-w,d, w,	 0.f	,0.f},
		{-w,d,-w,	 0.f	,1.f},
	};
	glBegin(GL_QUADS);
	for(int i=0;i<4;i++)
	{
		Vertext &V=vertices[i];
		glMultiTexCoord2f(GL_TEXTURE0,V.tx,V.ty);
		glVertex3f(V.x,V.y,V.z);
	}
	glEnd();
#endif
	return true;
}

void simul::opengl::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety,bool centred)
{
	
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glColor4f(colr[0],colr[1],colr[2],colr[3]);

	static float ff=1000.f;
	//float sz=sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
	float x=p[0]+(float)offsetx/ff;
	float y=p[1]-(float)offsety/ff;
	float z=p[2]-(float)offsety/ff;

	glRasterPos3f(x,y,z);
	const char *s=text;
	while(*s)
	{
#ifndef WIN64
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*s);
#endif
		s++;
	}
	
}

void simul::opengl::setParameter(GLuint program,const char *name,float value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
	{
		//std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	}
	else
		glUniform1f(loc,value);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLuint program,const char *name,float value1,float value2)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
	{
		//std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	}
	else
		glUniform2f(loc,value1,value2);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLuint program,const char *name,float value1,float value2,float value3)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
	{
		//std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	}
	else
		glUniform3f(loc,value1,value2,value3);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLuint program,const char *name,float value1,float value2,float value3,float value4)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
		glUniform4f(loc,value1,value2,value3,value4);
	GL_ERROR_CHECK
}
void simul::opengl::setVector4(GLuint program,const char *name,const float *value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
		glUniform4f(loc,value[0],value[1],value[2],value[3]);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLuint program,const char *name,int value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
	{
		if(num_warnings<MAX_GL_WARNINGS)
			std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
		num_warnings++;
	}
	else
		glUniform1i(loc,value);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GL_ERROR_CHECK
	GLint loc=glGetUniformLocation(program,name);
	GL_ERROR_CHECK
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
		glUniform4f(loc,value.x,value.y,value.z,value.w);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter2(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform2f(loc,value.x,value.y);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter3(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
	{
		glUniform3f(loc,value.x,value.y,value.z);
		GL_ERROR_CHECK
	}
}

void simul::opengl::setMatrix(GLuint program,const char *name,const float *value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
	{
		static bool tr=0;
		glUniformMatrix4fv(loc,1,tr,value);
		GL_ERROR_CHECK
	}
}

void simul::opengl::setMatrixTranspose(GLuint program,const char *name,const float *value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	static bool tr=1;
	glUniformMatrix4fv(loc,1,tr,value);
	GL_ERROR_CHECK
}

void simul::opengl::setTexture(GLuint program,const char *name,int texture_number,GLuint texture)
{
    glActiveTexture(GL_TEXTURE0+texture_number);
	glBindTexture(GL_TEXTURE_2D,texture);
GL_ERROR_CHECK
	GLint loc	=glGetUniformLocation(program,name);
GL_ERROR_CHECK
	if(loc<0)
	{
		if(num_warnings<MAX_GL_WARNINGS)
			std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: texture "<<name<<" was not found in GLSL program "<<program<<std::endl;
		num_warnings++;
	}
	else
		glUniform1i(loc,texture_number);
GL_ERROR_CHECK
}

void simul::opengl::set3DTexture(GLuint program,const char *name,int texture_number,GLuint texture)
{
GL_ERROR_CHECK
    glActiveTexture(GL_TEXTURE0+texture_number);
	glBindTexture(GL_TEXTURE_3D,texture);
GL_ERROR_CHECK
	GLint loc	=glGetUniformLocation(program,name);
GL_ERROR_CHECK
	if(loc<0)
	{
		if(num_warnings<MAX_GL_WARNINGS)
			std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: 3D texture "<<name<<" was not found in GLSL program "<<program<<std::endl;
		num_warnings++;
	}
	else
		glUniform1i(loc,texture_number);
GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLint loc,int value)
{
	glUniform1i(loc,value);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter(GLint loc,float value)
{
	glUniform1f(loc,value);
	GL_ERROR_CHECK
}

void simul::opengl::setParameter2(GLint loc,const simul::sky::float4 &value)
{
	glUniform2f(loc,value.x,value.y);
	GL_ERROR_CHECK
}
void simul::opengl::setParameter3(GLint loc,const simul::sky::float4 &value)
{
	glUniform3f(loc,value.x,value.y,value.z);
	GL_ERROR_CHECK
}

void simul::opengl::linkToConstantBuffer(GLuint program,const char *name,GLuint bindingIndex)
{
	GLint indexInShader	=glGetUniformBlockIndex(program,name);
	if(indexInShader>=0)
		glUniformBlockBinding(program,indexInShader,bindingIndex);
}

GLuint simul::opengl::make2DTexture(int w,int l,const float *src)
{
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,w,l,0,GL_RGBA,GL_FLOAT,src);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D,0);
	return tex;
}

#undef pi
#ifdef _MSC_VER
#include <windows.h>
#endif
void simul::opengl::CheckGLError(const char *filename,int line_number,int err)
{
	if(err)
	{
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"gl error ";
		while(err!=GL_NO_ERROR)
		{
			const char *error=NULL;
			switch(err)
			{
				case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
				case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
				case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
				case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
				case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
				default:
				error="UNKNOWN";
				break;
			}
			if(error)
				std::cerr <<" "<<err<<" GL_" << error;
			err=glGetError();
        }
		std::cerr<<std::endl;
		BREAK_IF_DEBUGGING;
	}
}
