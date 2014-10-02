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
#include "Simul/Platform/OpenGL/FrameBufferGL.h"
#include "Simul/Platform/OpenGL/Layout.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/CrossPlatform/Macros.h"

#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#include "Simul/Camera/Camera.h"
using namespace simul;
using namespace opengl;
RenderPlatform::RenderPlatform()
	:solid_program(0)
	,reverseDepth(false)
	,effect(NULL)
	,currentTopology(crossplatform::TRIANGLELIST)
{
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::RestoreDeviceObjects(void *unused)
{
	solidConstants.RestoreDeviceObjects();
	crossplatform::RenderPlatform::RestoreDeviceObjects(unused);
	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
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
	crossplatform::RenderPlatform::RecompileShaders();
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
	GL_ERROR_CHECK
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
		
void RenderPlatform::DrawMarker(crossplatform::DeviceContext &,const double *matrix)
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

void RenderPlatform::DrawLine(crossplatform::DeviceContext &,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
    glColor3f(colour[0],colour[1],colour[2]);
    glLineWidth(width);

    glBegin(GL_LINES);

    glVertex3dv((const GLdouble *)pGlobalBasePosition);
    glVertex3dv((const GLdouble *)pGlobalEndPosition);

    glEnd();
}

void RenderPlatform::DrawCrossHair(crossplatform::DeviceContext &,const double *pGlobalPosition)
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

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &,const double *pGlobalPosition, double pRoll)
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

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
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


void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult,bool blend)
{
GL_ERROR_CHECK
	effect->SetTexture(deviceContext,"image_texture",tex);
	if(blend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
GL_ERROR_CHECK
glDisable(GL_BLEND);
glDisable(GL_CULL_FACE);
	DrawQuad(deviceContext,x1,y1,dx,dy,effect,effect->GetTechniqueByName("show_texture"));
GL_ERROR_CHECK
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v)
{
GL_ERROR_CHECK
	
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
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
	GL_ERROR_CHECK
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
	GL_ERROR_CHECK
}
/*

#ifndef GLUT_BITMAP_HELVETICA_12
#define GLUT_BITMAP_HELVETICA_12	((void*)7)
#endif

void RenderPlatform::Print(crossplatform::DeviceContext &,int x,int y,const char *string,const float* colr,const float* bkg)
{
	if(!string)
		return;
	void *font=GLUT_BITMAP_HELVETICA_12;
	if(colr)
		glColor4fv(colr);
	else
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
}*/

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

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *m,const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
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

crossplatform::BaseFramebuffer	*RenderPlatform::CreateFramebuffer()
{
	opengl::FramebufferGL * b=new opengl::FramebufferGL;
	return b;
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *)
{
	return new opengl::SamplerState();
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	std::string fn(filename_utf8);
	if(fn.find(".")>=fn.length())
		fn+=".glfx";
	opengl::Effect *e=new opengl::Effect();
	e->Load(this,fn.c_str(),defines);
	e->SetName(fn.c_str());
	if(e->platform_effect==(void*)0xFFFFFFFF)
	{
		SAFE_DELETE(e);
	}
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	return new opengl::PlatformConstantBuffer();
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	return NULL;
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
	case INT_32_FLOAT:
		return GL_INTENSITY32F_ARB;
	case RGBA_8_UNORM:
		return GL_RGBA;
	case RGBA_8_SNORM:
		return GL_RGBA8_SNORM;
	case R_8_UNORM:
		return GL_R8;// not GL_R...!
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT32F;
	default:
		return 0;
	};
}

GLuint RenderPlatform::ToGLExternalFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case RGBA_16_FLOAT:
		return GL_RGBA;
	case RGBA_32_FLOAT:
		return GL_RGBA;
	case RGB_32_FLOAT:
		return GL_RGB;
	case RG_32_FLOAT:
		return GL_RG;
	case R_32_FLOAT:
		return GL_RED;
	case LUM_32_FLOAT:
		return GL_LUMINANCE;
	case INT_32_FLOAT:
		return GL_INTENSITY;
	case RGBA_8_UNORM:
		return GL_RGBA;
	case RGBA_8_SNORM:
		return GL_RGBA;
	case R_8_UNORM:
		return GL_RED;// not GL_R...!
	case D_32_FLOAT:
		return GL_RED;
	default:
		return GL_RGBA;
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
	case INT_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
		return 4;
	case RGBA_8_SNORM:
		return 4;
	case R_8_UNORM:
		return 1;
	case R_8_SNORM:
		return 1;
	case R_32_UINT:
		return 1;
	case RG_32_UINT:
		return 2;
	case RGB_32_UINT:
		return 3;
	case RGBA_32_UINT:
		return 4;
	case D_32_FLOAT:
		return 1;
	default:
		return 0;
	};
}
GLenum RenderPlatform::DataType(crossplatform::PixelFormat p)
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
	case INT_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	case R_8_UNORM:
		return GL_UNSIGNED_BYTE;
	case R_8_SNORM:
		return GL_UNSIGNED_BYTE;
	case R_32_UINT:
		return GL_UNSIGNED_INT;
	case RG_32_UINT:
		return GL_UNSIGNED_INT;
	case RGB_32_UINT:
		return GL_UNSIGNED_INT;
	case RGBA_32_UINT:
		return GL_UNSIGNED_INT;
	case D_32_FLOAT:
		return GL_FLOAT;
	default:
		return 0;
	};
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,crossplatform::LayoutDesc *desc,crossplatform::Buffer *buffer)
{
	opengl::Layout *l=new opengl::Layout();
	l->SetDesc(desc,num_elements);
GL_ERROR_CHECK
	SAFE_DELETE_VAO(l->vao);
	glGenVertexArrays(1,&l->vao );
	glBindVertexArray(l->vao);
	SIMUL_ASSERT(buffer->AsGLuint()!=0);
	glBindBuffer(GL_ARRAY_BUFFER,buffer->AsGLuint());
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
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

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	opengl::RenderState *s=new opengl::RenderState;
	s->desc=desc;
	s->type=desc.type;
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	opengl::Query *q=new opengl::Query(type);
	return q;
}

static GLenum toGlComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_LESS:
		return GL_LESS;
	case crossplatform::DEPTH_EQUAL:
		return GL_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return GL_LEQUAL;
	case crossplatform::DEPTH_GREATER:
		return GL_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return GL_NOTEQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return GL_GEQUAL;
	default:
		break;
	};
	return GL_LESS;
}
static GLenum toGlBlendOp(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return GL_ZERO;
	case crossplatform::BLEND_ONE:
		return GL_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return GL_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return GL_DST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return GL_DST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return 0;
	case crossplatform::BLEND_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_SRC1_COLOR:
		return GL_SRC1_COLOR;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return GL_ONE_MINUS_SRC1_COLOR;
	case crossplatform::BLEND_SRC1_ALPHA:
		return GL_SRC1_ALPHA;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return GL_ONE_MINUS_SRC1_ALPHA;
	default:
		break;
	};
	return GL_ONE;
}
void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	opengl::RenderState *S=(opengl::RenderState*)s;
	if(S->desc.type==crossplatform::BLEND)
	{
		if(S->desc.blend.RenderTarget[0].BlendEnable)
		{
			glEnable(GL_BLEND);
			for(int i=0;i<S->desc.blend.numRTs;i++)
			{
				const crossplatform::RTBlendDesc &d=S->desc.blend.RenderTarget[i];
				//glBlendEquationi((unsigned)i, GL_FUNC_ADD);
				glBlendEquationSeparatei((unsigned)i, GL_FUNC_ADD,GL_FUNC_ADD);

				//glBlendFunci((unsigned)i, toGlBlendOp(d.SrcBlend), toGlBlendOp(d.DestBlend));

				glBlendFuncSeparatei((unsigned)i, toGlBlendOp(d.SrcBlend), toGlBlendOp(d.DestBlend),
									   toGlBlendOp(d.SrcBlendAlpha), toGlBlendOp(d.DestBlendAlpha));
			}
		}
		else
			glDisable(GL_BLEND);
	}
}

void *RenderPlatform::GetDevice()
{
	return NULL;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &,int ,int num_buffers,crossplatform::Buffer **buffers,const crossplatform::Layout *layout)
{
	GL_ERROR_CHECK
	/*
	for(int i=0;i<num_buffers;i++)
	{
		crossplatform::Buffer *buffer=buffers[i];
GL_ERROR_CHECK
		GLuint buf=buffer->AsGLuint();
		//GLint prog;
		//glGetIntegerv(GL_CURRENT_PROGRAM,&prog);
		glBindBuffer(GL_ARRAY_BUFFER,buf);
	GL_ERROR_CHECK
		glEnableVertexAttribArray(i);
	GL_ERROR_CHECK
		// We pass:
		// 	GLuint index, 	GLint size, 	GLenum type, 	GLboolean normalized, 	GLsizei stride, 	const GLvoid * pointer
		// size 
		glVertexAttribPointer(i,buffer->stride,GL_FLOAT,false,0,0);
	GL_ERROR_CHECK
	}*/
}

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	crossplatform::Viewport viewport;
	GL_ERROR_CHECK
	glGetIntegerv(GL_VIEWPORT,(int*)(&viewport));
	GL_ERROR_CHECK
	return viewport;
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Viewport *vps)
{
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	GL_ERROR_CHECK
	GLuint buf=buffer->AsGLuint();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
	GL_ERROR_CHECK
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t)
{
	currentTopology=t;
	GL_ERROR_CHECK
}

void RenderPlatform::EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options)
{
	/// We will not do this for GL, because there's NO BINARY SHADER FORMAT!
#if 0
	crossplatform::RenderPlatform::EnsureEffectIsBuilt(filename_utf8,options);
#endif
}

void RenderPlatform::StoreRenderState(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
//	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GL_ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	GL_ERROR_CHECK
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
	GL_ERROR_CHECK
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	GL_ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	GL_ERROR_CHECK
//	glPopAttrib();
	GL_ERROR_CHECK
}

GLenum toGLTopology(crossplatform::Topology t)
{
	switch(t)
	{
	case crossplatform::POINTLIST:
		return GL_POINTS;
	case crossplatform::LINELIST:
		return GL_LINES;
	case crossplatform::LINESTRIP:
		return GL_LINE_STRIP;	
	case crossplatform::TRIANGLELIST:
		return GL_TRIANGLES;	
	case crossplatform::TRIANGLESTRIP	:
		return GL_TRIANGLE_STRIP;	
	case crossplatform::LINELIST_ADJ	:
		return GL_LINES;	
	case crossplatform::LINESTRIP_ADJ	:
		return GL_LINE_STRIP;	
	case crossplatform::TRIANGLELIST_ADJ:
		return GL_TRIANGLES;	
	case crossplatform::TRIANGLESTRIP_ADJ:
		return GL_TRIANGLE_STRIP;
	default:
		break;
	};
	return GL_LINE_LOOP;
}
void RenderPlatform::Draw(crossplatform::DeviceContext &,int num_verts,int start_vert)
{
	glDrawArrays(toGLTopology(currentTopology), start_vert, num_verts); 
}

void RenderPlatform::DrawIndexed		(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
	GL_ERROR_CHECK
	glDrawElements(toGLTopology(currentTopology),num_indices,GL_UNSIGNED_SHORT,(void*)base_vertex);
	GL_ERROR_CHECK
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &,Vertext *lines,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glUseProgram(Utilities::GetSingleton().linedraw_program);
	glDisable(GL_ALPHA_TEST);
    test_depth?glEnable(GL_DEPTH_TEST):glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glBegin(strip?GL_LINE_STRIP:GL_LINES);
	for(int i=0;i<vertex_count;i++)
	{
		const Vertext &V=lines[i];
		glColor4f(V.colour.x,V.colour.y,V.colour.z,V.colour.w);
		glVertex3f(V.pos.x,V.pos.y,V.pos.z);
	}
	glEnd();
	glUseProgram(0);
	glPopAttrib();
}

void RenderPlatform::Draw2dLines	(crossplatform::DeviceContext &,Vertext *lines,int vertex_count,bool strip)
{
	//::Draw2DLines((VertexXyzRgba*)lines,vertex_count,strip);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glUseProgram(Utilities::GetSingleton().linedraw_2d_program);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    
    
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

void RenderPlatform::PrintAt3dPos(crossplatform::DeviceContext &,const float *p,const char *text,const float* colr,int offsetx,int offsety,bool centred)
{
	::PrintAt3dPos(p,text,colr,offsetx,offsety);
}

void RenderPlatform::DrawCircle		(crossplatform::DeviceContext &,const float *,float ,const float *,bool)
{
}

static vec4 lerp(float s,vec4 x1,vec4 x2)
{
	vec4 r;
	r.x=s*x2.x+(1.0f-s)*x1.x;
	r.y=s*x2.y+(1.0f-s)*x1.y;
	r.z=s*x2.z+(1.0f-s)*x1.z;
	r.w=s*x2.w+(1.0f-s)*x1.w;
	return r;
}

static vec4 Lookup(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *tex,float distance_texcoord,float elevation_texcoord)
{
	distance_texcoord*=(float)tex->GetWidth();
	int x=(int)(distance_texcoord);
	if(x<0)
		x=0;
	if(x>tex->GetWidth()-2)
		x=tex->GetWidth()-2;
	float x_interp=distance_texcoord-x;
	elevation_texcoord*=(float)tex->GetLength();
	int  	y=(int)(elevation_texcoord);
	if(y<0)
		y=0;
	if(y>tex->GetWidth()-2)
		y=tex->GetWidth()-2;
	float y_interp=elevation_texcoord-y;
	// four floats per texel, four texels.
	vec4 data[4];
	tex->activateRenderTarget(deviceContext);
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	tex->deactivateRenderTarget();
	vec4 bottom		=lerp(x_interp,data[0],data[1]);
	vec4 top		=lerp(x_interp,data[2],data[3]);
	vec4 ret		=lerp(y_interp,bottom,top);
	return ret;
}
