#include "GL/glew.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Material.h"
#include "Simul/Platform/OpenGL/Mesh.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/OpenGL/Light.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>

using namespace simul;
using namespace opengl;
RenderPlatform::RenderPlatform()
	:solid_program(0)
	,reverseDepth(false)
{
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::RestoreDeviceObjects(void*)
{
	solidConstants.RestoreDeviceObjects();
	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	solidConstants.Release();
	SAFE_DELETE_PROGRAM(solid_program);
}

void RenderPlatform::RecompileShaders()
{
	std::map<std::string,std::string> defines;
	SAFE_DELETE_PROGRAM(solid_program);
	solid_program	=MakeProgram("solid",defines);
	solidConstants.LinkToProgram(solid_program,"SolidConstants",1);
}

void RenderPlatform::PushTexturePath(const char *pathUtf8)
{
	simul::opengl::PushTexturePath(pathUtf8);
}

void RenderPlatform::PopTexturePath()
{
	simul::opengl::PopTexturePath();
}

void RenderPlatform::StartRender()
{
	glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_LIGHTING_BIT);
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glUseProgram(solid_program);
}

void RenderPlatform::EndRender()
{
	glUseProgram(0);
	glPopAttrib();
	glPopAttrib();
}

void RenderPlatform::SetReverseDepth(bool r)
{
	reverseDepth=r;
}

namespace
{
    const float DEFAULT_LIGHT_POSITION[]				={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]					={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF				=180.0f;
}

void RenderPlatform::IntializeLightingEnvironment(const float pAmbientLight[3])
{
    glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_DIRECTION_LIGHT_POSITION);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, DEFAULT_LIGHT_COLOR);
    glLightfv(GL_LIGHT0, GL_SPECULAR, DEFAULT_LIGHT_COLOR);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, DEFAULT_LIGHT_SPOT_CUTOFF);
    glEnable(GL_LIGHT0);
    // Set ambient light.
    GLfloat lAmbientLight[] = {static_cast<GLfloat>(pAmbientLight[0]), static_cast<GLfloat>(pAmbientLight[1]),static_cast<GLfloat>(pAmbientLight[2]), 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lAmbientLight);
}

void RenderPlatform::DrawMarker(void *,const double *matrix)
{
    glColor3f(0.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) matrix);

    glBegin(GL_LINE_LOOP);
        glVertex3f(+1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
    glEnd();
    glPopMatrix();
}

void RenderPlatform::DrawLine(void *,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
    glColor3f(colour[0],colour[1],colour[2]);
    glLineWidth(width);

    glBegin(GL_LINES);

    glVertex3dv((const GLdouble *)pGlobalBasePosition);
    glVertex3dv((const GLdouble *)pGlobalEndPosition);

    glEnd();
}

void RenderPlatform::DrawCrossHair(void *,const double *pGlobalPosition)
{
    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((double*) pGlobalPosition);

    double lCrossHair[6][3] = { { -3, 0, 0 }, { 3, 0, 0 },
    { 0, -3, 0 }, { 0, 3, 0 },
    { 0, 0, -3 }, { 0, 0, 3 } };

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[0]);
    glVertex3dv(lCrossHair[1]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[2]);
    glVertex3dv(lCrossHair[3]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[4]);
    glVertex3dv(lCrossHair[5]);

    glEnd();
	
	glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void RenderPlatform::DrawCamera(void *,const double *pGlobalPosition, double pRoll)
{
    glColor3d(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) pGlobalPosition);
    glRotated(pRoll, 1.0, 0.0, 0.0);

    int i;
    float lCamera[10][2] = {{ 0, 5.5 }, { -3, 4.5 },
    { -3, 7.5 }, { -6, 10.5 }, { -23, 10.5 },
    { -23, -4.5 }, { -20, -7.5 }, { -3, -7.5 },
    { -3, -4.5 }, { 0, -5.5 }   };

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
    }
    glEnd();

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
        }
    }
    glEnd();

    for (i = 0; i < 10; i++)
    {
        glBegin( GL_LINES );
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
        glEnd();
    }
    glPopMatrix();
}

void RenderPlatform::DrawLineLoop(void *,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
    glPushMatrix();
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    glPopMatrix();
}

void RenderPlatform::DrawTexture	(void *context,int x1,int y1,int dx,int dy,void *tex,float mult)
{
	glBindTexture(GL_TEXTURE_2D,(GLuint)tex);
	glUseProgram(Utilities::GetSingleton().simple_program);
	::DrawQuad(x1,y1,dx,dy);
	glUseProgram(0);	
}

void RenderPlatform::DrawQuad		(void *context,int x1,int y1,int dx,int dy,void *effect,void *technique)
{
	struct Viewport
	{
		int X,Y,Width,Height;
	};
	Viewport viewport;
	glGetIntegerv(GL_VIEWPORT,(int*)(&viewport));
	GLint program=(GLint)technique;
	float r[]={2.f*(float)x1/(float)viewport.Width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
		,2.f*(float)dx/(float)viewport.Width
		,2.f*(float)dy/(float)viewport.Height};
	opengl::setVector4(program,"rect",r);
	glBegin(GL_QUADS);
	glVertex2f(0.0,1.0);
	glVertex2f(1.0,1.0);
	glVertex2f(1.0,0.0);
	glVertex2f(0.0,0.0);
	glEnd();
}

#ifndef GLUT_BITMAP_HELVETICA_12
#define GLUT_BITMAP_HELVETICA_12	((void*)7)
#endif

void RenderPlatform::Print			(void *context,int x,int y	,const char *string)
{
	void *font=GLUT_BITMAP_HELVETICA_12;
	glColor4f(1.f,1.f,1.f,1.f);
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	int win_h=viewport[3];
	glRasterPos2f(x,win_h-y);
	glDisable(GL_LIGHTING);
	glBindTexture(GL_TEXTURE_2D,0);
	const char *s=string;
	while(*s)
	{
		if(*s=='\n')
		{
			y+=12;
			glRasterPos2f(x,win_h-y);
		}
#ifndef WIN64
		else
			glutBitmapCharacter(font,*s);
#else
		font;
#endif
		s++;
	}
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
#include "Simul/Math/Matrix4x4.h"
void MakeWorldViewProjMatrix(float *wvp,const double *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,view(v),proj(p),model(w);
	simul::math::Multiply4x4(tmp1,model,view);
	simul::math::Multiply4x4(*(simul::math::Matrix4x4*)wvp,tmp1,proj);
}

void RenderPlatform::SetModelMatrix(void *,const crossplatform::ViewStruct &viewStruct,const double *m)
{
	simul::math::Matrix4x4 proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	simul::math::Matrix4x4 view;
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,viewStruct.view,viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	solidConstants.worldViewProj=modelviewproj;
	solidConstants.Apply();
}

scene::Material *RenderPlatform::CreateMaterial()
{
	opengl::Material *mat=new opengl::Material;
	materials.insert(mat);
	return mat;
}

scene::Mesh *RenderPlatform::CreateMesh()
{
	return new opengl::Mesh;
}

scene::Light *RenderPlatform::CreateLight()
{
	return new opengl::Light();
}

scene::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	scene::Texture * tex=new opengl::Texture;
	tex->LoadFromFile(fileNameUtf8);
	return tex;
}

void *RenderPlatform::GetDevice()
{
	return NULL;
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	::DrawLines((VertexXyzRgba*)lines,vertex_count,strip);
}

void RenderPlatform::Draw2dLines	(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	//::Draw2DLines((VertexXyzRgba*)lines,vertex_count,strip);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glUseProgram(Utilities::GetSingleton().linedraw_2d_program);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_3D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glBegin(strip?GL_LINE_STRIP:GL_LINES);
	for(int i=0;i<vertex_count;i++)
	{
		Vertext &V=lines[i];
		glColor4fv(V.colour);
		glVertex3fv(V.pos);
	}
	glEnd();
	glUseProgram(0);
	glPopAttrib();
}

void RenderPlatform::PrintAt3dPos(void *,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	::PrintAt3dPos(p,text,colr,offsetx,offsety);
}

void RenderPlatform::DrawCircle		(crossplatform::DeviceContext &context,const float *dir,float rads,const float *colr,bool)
{
}