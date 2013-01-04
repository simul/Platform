#include <GL/glew.h>
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Sky/Float4.h"
#include <windows.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <iostream>
#include "Simul/Base/Timer.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include <math.h>

int Utilities::instance_count=0;
int Utilities::screen_width=0;
int Utilities::screen_height=0;
GLuint Utilities::linedraw_program=0;
GLuint Utilities::simple_program=0;

Utilities::Utilities()
{
	instance_count++;
}

void Utilities::RestoreDeviceObjects(void *)
{
	const char *vert="varying vec4 colr;"
						"void main(void)"
						"{"
						"    gl_Position		= ftransform();"
						"    colr=gl_Color;"
						"}";
	const char *frag="varying vec4 colr;"
						"void main(void)"
						"{"
						"	gl_FragColor=colr;"
						"}";
	linedraw_program=SetShaders(vert,frag);
	const char *simple_vert="varying vec2 texc;"
						"void main(void)"
						"{"
						"    gl_Position		= ftransform();"
						"    texc=gl_MultiTexCoord0.xy;"
						"}";
	const char *simple_frag="uniform sampler2D image_texture;"
						"varying vec2 texc;"
						"void main(void)"
						"{"
						"	vec4 c = texture2D(image_texture,texc);"
						"	gl_FragColor=c;"
						"}";
	simple_program=SetShaders(simple_vert,simple_frag);
}

void Utilities::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void Utilities::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(linedraw_program);
	SAFE_DELETE_PROGRAM(simple_program);
}

Utilities::~Utilities()
{
	if(!instance_count)
		Utilities::InvalidateDeviceObjects();
}

void RenderTexture(int x,int y,int w,int h)
{
	ERROR_CHECK		
	int prog=0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	if(prog==0)
		glUseProgram(Utilities::simple_program);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0.f,1.f);
	glVertex2f((float)x,(float)(y+h));
	glTexCoord2f(1.f,1.f);
	glVertex2f((float)(x+w),(float)(y+h));
	glTexCoord2f(1.f,0.f);
	glVertex2f((float)(x+w),(float)y);
	glTexCoord2f(0.f,0.f);
	glVertex2f((float)x,(float)y);
	glEnd();
	ERROR_CHECK
}

bool IsExtensionSupported(const char *name)
{
	GLint n=0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (GLint i=0; i<n; i++)
	{
		const char* extension=(const char*)glGetStringi(GL_EXTENSIONS, i);
		if (!strcmp(name,extension))
		{
			std::cout<<"GL Extension supported: "<<extension<<std::endl;
			return true;
		}
	}
	return false;
}


bool CheckExtension(const char *txt)
{
	if(!glewIsSupported(txt)&&!IsExtensionSupported(txt))
	{
		std::cerr<<"Error - required OpenGL extension is not supported: "<<txt<<std::endl;
		return false;
	}
	return true;
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

void Ortho()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0,1.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void SetTopDownOrthoProjection(int w,int h)
{
	win_h=h;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,w,h,0,-1.0,1.0);
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
void DrawQuad(int x,int y,int w,int h)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0.0,1.0);
	glVertex2f(x,y+h);
	glTexCoord2f(1.0,1.0);
	glVertex2f(x+w,y+h);
	glTexCoord2f(1.0,0.0);
	glVertex2f(x+w,y);
	glTexCoord2f(0.0,0.0);
	glVertex2f(x,y);
	glEnd();
}


float GetFramerate()
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

void CheckGLError(const char *filename,int line_number)
{
	int err=glGetError();
	if(err!=0)
	{
		CheckGLError(filename,line_number,err);
	}
}

void CheckGLError(const char *filename,int line_number,int err)
{
	if(err)
	{
		std::cerr<<filename<<" ("<<line_number<<"): ";
		const char *c=(const char*)gluErrorString(err);
		if(c)
			std::cerr<<std::endl<<c<<std::endl;
		const char *d=(const char*)glewGetErrorString(err);
		if(d)
			std::cerr<<std::endl<<d<<std::endl;
		if(!c&&!d)
			std::cerr<<std::endl<<"unknown error: "<<err<<std::endl;
		DebugBreak();
		//assert(0);
	}
}

void CalcCameraPosition(float *cam_pos,float *cam_dir)
{
	simul::math::Matrix4x4 modelview;
	glGetFloatv(GL_MODELVIEW_MATRIX,modelview.RowPointer(0));
	ERROR_CHECK
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
bool RenderAngledQuad(const float *dir,float half_angle_radians)
{
		ERROR_CHECK
	float cam_dir[3],cam_pos[3];
	CalcCameraPosition(cam_pos,cam_dir);
	float Yaw=atan2(dir[0],dir[1]);
	float Pitch=asin(dir[2]);
    glMatrixMode(GL_MODELVIEW);
		ERROR_CHECK
    glPushMatrix();
		ERROR_CHECK
	//glLoadIdentity();
	simul::math::Matrix4x4 modelview;
		ERROR_CHECK
	glTranslatef(cam_pos[0],cam_pos[1],cam_pos[2]);
		ERROR_CHECK
	glRotatef(180.f*Yaw/pi,0.0f,0.0f,-1.0f);
		ERROR_CHECK
	glRotatef(180.f*Pitch/pi,1.0f,0.0f,0.0f);
		ERROR_CHECK
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
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
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
		ERROR_CHECK
	return true;
}

void PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
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
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*s);
		s++;
	}
	glPopAttrib();
}

void DrawLines(VertexXyzRgba *lines,int vertex_count,bool strip)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glUseProgram(Utilities::linedraw_program);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glBegin(strip?GL_LINE_STRIP:GL_LINES);
	for(int i=0;i<vertex_count;i++)
	{
		VertexXyzRgba &V=lines[i];
		glColor4f(V.r,V.g,V.b,V.a);
		glVertex3f(V.x,V.y,V.z);
	}
	glEnd();
	glUseProgram(0);
	glPopAttrib();
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

void FixGlProjectionMatrix(float required_distance)
{
	simul::math::Matrix4x4 proj;
	glGetMatrix(proj.RowPointer(0),GL_PROJECTION_MATRIX);

	float zFar=proj(3,2)/(1.f+proj(2,2));
	float zNear=proj(3,2)/(proj(2,2)-1.f);
	zFar=required_distance;
	proj(2,2)=-(zFar+zNear)/(zFar-zNear);
	proj(3,2)=-2.f*(zNear*zFar)/(zFar-zNear);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(proj.RowPointer(0));
}

void OrthoMatrices()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0,1.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}


void setParameter(GLuint program,const char *name,float value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform1f(loc,value);
	ERROR_CHECK
}

void setParameter(GLuint program,const char *name,float value1,float value2)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform2f(loc,value1,value2);
	ERROR_CHECK
}

void setParameter(GLuint program,const char *name,float value1,float value2,float value3)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform3f(loc,value1,value2,value3);
	ERROR_CHECK
}

void setParameter(GLuint program,const char *name,int value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform1i(loc,value);
	ERROR_CHECK
}

void setParameter(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform4f(loc,value.x,value.y,value.z,value.w);
	ERROR_CHECK
}

void setParameter2(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform2f(loc,value.x,value.y);
	ERROR_CHECK
}

void setParameter3(GLuint program,const char *name,const simul::sky::float4 &value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	glUniform3f(loc,value.x,value.y,value.z);
	ERROR_CHECK
}

void setMatrix(GLuint program,const char *name,const float *value)
{
	GLint loc=glGetUniformLocation(program,name);
	if(loc<0)
		std::cout<<"Warning: parameter "<<name<<" was not found in GLSL program "<<program<<std::endl;
	static bool tr=1;
	glUniformMatrix4fv(loc,1,tr,value);
	ERROR_CHECK
}


void setParameter(GLint loc,int value)
{
	glUniform1i(loc,value);
	ERROR_CHECK
}

void setParameter(GLint loc,float value)
{
	glUniform1f(loc,value);
	ERROR_CHECK
}

void setParameter2(GLint loc,const simul::sky::float4 &value)
{
	glUniform2f(loc,value.x,value.y);
	ERROR_CHECK
}
void setParameter3(GLint loc,const simul::sky::float4 &value)
{
	glUniform3f(loc,value.x,value.y,value.z);
	ERROR_CHECK
}

void setMatrix(GLint loc,const float *value)
{
	static bool tr=1;
	glUniformMatrix4fv(loc,1,tr,value);
	ERROR_CHECK
}