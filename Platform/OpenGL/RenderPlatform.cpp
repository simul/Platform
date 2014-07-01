#include "GL/glew.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Material.h"
#include "Simul/Platform/OpenGL/Mesh.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/Light.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Platform/OpenGL/Buffer.h"
#include "Simul/Platform/OpenGL/Layout.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#include "Simul/Camera/Camera.h"
using namespace simul;
using namespace opengl;
RenderPlatform::RenderPlatform()
	:solid_program(0)
	,reverseDepth(false)
	,effect(NULL)
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
	delete effect;
	effect=NULL;
}

void RenderPlatform::RecompileShaders()
{
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]="0";
	SAFE_DELETE_PROGRAM(solid_program);
	solid_program	=MakeProgram("solid",defines);
	solidConstants.LinkToProgram(solid_program,"SolidConstants",1);

	effect=CreateEffect("debug",defines);
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

void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &,int w,int l,int d)
{
	glDispatchCompute(w,l,d);
	GL_ERROR_CHECK
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int pass)
{
	deviceContext;
	effect;
	tech;
	pass;
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

void RenderPlatform::DrawTexture	(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,GLuint tex,float /*mult*/)
{
GL_ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,tex);
GL_ERROR_CHECK
glDisable(GL_BLEND);
glDisable(GL_CULL_FACE);
	DrawQuad(deviceContext,x1,y1,dx,dy,effect,effect->GetTechniqueByIndex(0));
GL_ERROR_CHECK
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult)
{
	DrawTexture(deviceContext,x1,y1,dx,dy,tex->AsGLuint(),mult);
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex)
{
GL_ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
GL_ERROR_CHECK
glDisable(GL_BLEND);
glDisable(GL_CULL_FACE);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	static float cc=300000.f;
	vec4 depthToLinFadeDistParams=camera::GetDepthToDistanceParameters(deviceContext.viewStruct,cc);//(deviceContext.viewStruct.proj[3*4+2],cc,deviceContext.viewStruct.proj[2*4+2]*cc);
	struct Viewport
	{
		int X,Y,Width,Height;
	};
	GL_ERROR_CHECK
	Viewport viewport;
	glGetIntegerv(GL_VIEWPORT,(int*)(&viewport));
	GL_ERROR_CHECK
	effect->Apply(deviceContext,effect->GetTechniqueByName("show_depth"),0);
	GL_ERROR_CHECK
	effect->SetParameter("tanHalfFov",vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov));
	effect->SetParameter("depthToLinFadeDistParams",depthToLinFadeDistParams);
	effect->SetTexture(deviceContext,"image_texture",tex);
	vec4 r(2.f*(float)x1/(float)viewport.Width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
		,2.f*(float)dx/(float)viewport.Width
		,2.f*(float)dy/(float)viewport.Height);
	GL_ERROR_CHECK
	effect->SetParameter("rect",r);
	GL_ERROR_CHECK
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GL_ERROR_CHECK
	effect->Unapply(deviceContext);
GL_ERROR_CHECK
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)
{
	struct Viewport
	{
		int X,Y,Width,Height;
	};
	if(!effect||!technique)
		return;
	GL_ERROR_CHECK
	Viewport viewport;
	glGetIntegerv(GL_VIEWPORT,(int*)(&viewport));
	GL_ERROR_CHECK
	effect->Apply(deviceContext,technique,0);
	GL_ERROR_CHECK
	vec4 r(2.f*(float)x1/(float)viewport.Width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
		,2.f*(float)dx/(float)viewport.Width
		,2.f*(float)dy/(float)viewport.Height);
	GL_ERROR_CHECK
	effect->SetParameter("rect",r);
	GL_ERROR_CHECK
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GL_ERROR_CHECK
	effect->Unapply(deviceContext);
	GL_ERROR_CHECK
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
	GL_ERROR_CHECK
}


#ifndef GLUT_BITMAP_HELVETICA_12
#define GLUT_BITMAP_HELVETICA_12	((void*)7)
#endif

void RenderPlatform::Print(crossplatform::DeviceContext &,int x,int y	,const char *string)
{
	if(!string)
		return;
	void *font=GLUT_BITMAP_HELVETICA_12;
	glColor4f(1.f,1.f,1.f,1.f);
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	int win_h=viewport[3];
	glRasterPos2f((float)x,(float)(win_h-y));
	glDisable(GL_LIGHTING);
	glBindTexture(GL_TEXTURE_2D,0);
	const char *s=string;
	while(*s)
	{
		if(*s=='\n')
		{
			y+=12;
			glRasterPos2f((float)x,(float)(win_h-y));
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

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *m)
{
	simul::math::Matrix4x4 proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	simul::math::Matrix4x4 view;
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	solidConstants.worldViewProj=modelviewproj;
	solidConstants.Apply();
}

crossplatform::Material *RenderPlatform::CreateMaterial()
{
	opengl::Material *mat=new opengl::Material;
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new opengl::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new opengl::Light();
}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	crossplatform::Texture * tex=new opengl::Texture;
	if(fileNameUtf8)
		tex->LoadFromFile(this,fileNameUtf8);
	return tex;
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	std::string fn(filename_utf8);
	if(fn.find(".")>=fn.length())
		fn+=".glfx";
	return new opengl::Effect(this,fn.c_str(),defines);
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	return new opengl::PlatformConstantBuffer();
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	return new opengl::Buffer();
}

GLuint RenderPlatform::ToGLFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case RGBA_16_FLOAT:
		return GL_RGBA16F;
	case RGBA_32_FLOAT:
		return GL_RGBA32F;
	case RGB_32_FLOAT:
		return GL_RGB32F;
	case RG_32_FLOAT:
		return GL_RG32F;
	case R_32_FLOAT:
		return GL_R32F;
	case LUM_32_FLOAT:
		return GL_LUMINANCE32F_ARB;
	case RGBA_8_UNORM:
		return GL_RGBA;
	case RGBA_8_SNORM:
		return GL_RGBA;
	default:
		return 0;
	};
}

int RenderPlatform::FormatCount(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case RGBA_16_FLOAT:
		return 4;
	case RGBA_32_FLOAT:
		return 4;
	case RGB_32_FLOAT:
		return 3;
	case RG_32_FLOAT:
		return 2;
	case R_32_FLOAT:
		return 1;
	case LUM_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
		return 4;
	case RGBA_8_SNORM:
		return 4;
	default:
		return 0;
	};
}
GLenum DataType(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case RGBA_16_FLOAT:
		return GL_FLOAT;
	case RGBA_32_FLOAT:
		return GL_FLOAT;
	case RGB_32_FLOAT:
		return GL_FLOAT;
	case RG_32_FLOAT:
		return GL_FLOAT;
	case R_32_FLOAT:
		return GL_FLOAT;
	case LUM_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	default:
		return 0;
	};
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,crossplatform::LayoutDesc *desc,crossplatform::Buffer *buffer)
{
	opengl::Layout *l=new opengl::Layout();
GL_ERROR_CHECK
	SAFE_DELETE_VAO(l->vao);
	glGenVertexArrays(1,&l->vao );
	glBindVertexArray(l->vao);
	SIMUL_ASSERT(buffer->AsGLuint()!=0);
	glBindBuffer(GL_ARRAY_BUFFER,buffer->AsGLuint());
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
		//d.semanticName;
		//d.semanticIndex;
		//ToGLFormat(d.format);
		//d.alignedByteOffset;
		//d.inputSlot;
		//d.perInstance?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
		//d.instanceDataStepRate;
		//{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "TEXCOORD",	0, DXGI_FORMAT_R32_FLOAT,			0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			glEnableVertexAttribArray( i );
			glVertexAttribPointer( i						// Attribute bind location
									,FormatCount(d.format)	// Data type count
									,DataType(d.format)				// Data type
									,GL_FALSE				// Normalise this data type?
									,buffer->stride			// Stride to the next vertex
									,(GLvoid*)d.alignedByteOffset );	// Vertex Buffer starting offset
	};
	
	glBindVertexArray( 0 ); 
GL_ERROR_CHECK
	return l;
}

void *RenderPlatform::GetDevice()
{
	return NULL;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &,int ,int num_buffers,crossplatform::Buffer **buffers)
{
	for(int i=0;i<num_buffers;i++)
	{
		crossplatform::Buffer *buffer=buffers[i];
GL_ERROR_CHECK
		GLuint buf=buffer->AsGLuint();
		GLint prog;
		glGetIntegerv(GL_CURRENT_PROGRAM,&prog);
		glBindBuffer(GL_ARRAY_BUFFER,buf);
	}
}

void RenderPlatform::Draw(crossplatform::DeviceContext &,int num_verts,int start_vert)
{
	glDrawArrays(GL_POINTS, start_vert, num_verts); 
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &,Vertext *lines,int vertex_count,bool strip)
{
	::DrawLines((VertexXyzRgba*)lines,vertex_count,strip);
}

void RenderPlatform::Draw2dLines	(crossplatform::DeviceContext &,Vertext *lines,int vertex_count,bool strip)
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

void RenderPlatform::DrawCircle		(crossplatform::DeviceContext &,const float *,float ,const float *,bool)
{
}