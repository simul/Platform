#include "GL/glew.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Material.h"
#include "Simul/Platform/OpenGL/Mesh.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/Light.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Platform/OpenGL/Buffer.h"
#include "Simul/Platform/OpenGL/FrameBufferGL.h"
#include "Simul/Platform/OpenGL/Layout.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Platform/CrossPlatform/Macros.h"

#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#include <ios>					// For std::cout and cerr .
#include <iomanip>				// For std::cerr formatting.
#include "Simul/Platform/CrossPlatform/Camera.h"
using namespace simul;
using namespace opengl;


//typedef void (GLAPIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
/*typedef void (APIENTRY *DEBUGPROC)(GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *message,
            void *userParam);*/

// http://blog.nobel-joergensen.com/2013/02/17/debugging-opengl-part-2-using-gldebugmessagecallback/
void GLAPIENTRY openglDebugCallbackFunction(GLenum source,
                                           GLenum type,
                                           GLuint id,
                                           GLenum severity,
                                           GLsizei /*length*/,
                                           const GLchar* message,
                                          const void* /*userParam*/)
{
	using namespace std;
	cerr<<__FILE__<<"("<<__LINE__<<"): ";
    switch (type)
	{
    case GL_DEBUG_TYPE_ERROR:
		cerr<<"error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        cerr << "DEPRECATED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        cerr << "UNDEFINED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        cerr << "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        cerr << "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        cerr << "OTHER";
        break;
    }
    
    cerr << " G" <<setfill('0') << setw(4)<< id << ": "<<resetiosflags(std::ios_base::_Fmtmask);
    cerr << message << " ";
 
    cerr << "severity: ";
    switch (severity){
    case GL_DEBUG_SEVERITY_LOW:
        cerr << "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        cerr << "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        cerr << "HIGH";
        break;
    }
	cerr << endl;
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
		BREAK_ONCE_IF_DEBUGGING;
	default:
		break;
	};
}
#include "GL/glfx.h"
void RenderPlatform::T1()
{
	glBlendEquationSeparatei(0, GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparatei(0,  GL_ONE, GL_ZERO,   GL_ONE, GL_ZERO);
}

RenderPlatform::RenderPlatform()
	:reverseDepth(false)
	,currentTopology(crossplatform::TRIANGLELIST)
	,empty_vao(0)
{

}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::RestoreDeviceObjects(void *unused)
{
	GL_ERROR_CHECK
	ERRNO_CHECK
	glewExperimental= GL_TRUE;
	GLenum glewError = glewInit();
	// Glew creates an error on init, due to using deprecated interfaces.
	glGetError();
	if (glewError!=GLEW_OK)
	{
		std::cerr << "Error initializing GLEW! " << glewGetErrorString(glewError) << "\n";
		return;
	}
	if(glewIsSupported("GL_ARB_debug_output"))
	{
	GL_ERROR_CHECK
		std::cout << "Register OpenGL debug callback " << std::endl;
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(openglDebugCallbackFunction, nullptr);
		glDebugMessageControl( GL_DONT_CARE ,
			GL_DEBUG_TYPE_OTHER ,
			GL_DONT_CARE ,
			0, NULL , GL_FALSE );
		GLuint unusedIds = 0;
// Enabling only two par
	/*	glDebugMessageControl(GL_DONT_CARE,
			GL_DEBUG_TYPE_ERROR,
			GL_DONT_CARE,
			0,
			&unusedIds,
			GL_TRUE);
		glDebugMessageInsert(	GL_DEBUG_SOURCE_APPLICATION ,
			GL_DEBUG_TYPE_ERROR,
			1,
			GL_DEBUG_SEVERITY_HIGH ,
			-1,
			"Testing gl callback output");*/
	}
	else
		std::cout << "glDebugMessageCallback not available" <<std::endl;
	
	GL_ERROR_CHECK
	const GLubyte* pVersion = glGetString(GL_VERSION); 
	std::cout<<"GL_VERSION: "<<pVersion<<std::endl;
	GL_ERROR_CHECK
	ERRNO_BREAK

	rescaleVertexShaderConstants.RestoreDeviceObjects(this);
	ERRNO_BREAK
	crossplatform::RenderPlatform::RestoreDeviceObjects(unused);
	ERRNO_BREAK
	// GL Insists on having a bound vertex array object, even if we're not using it in the vertex shader.
	glGenVertexArrays(1,&empty_vao);
	ERRNO_BREAK
	RecompileShaders();
	ERRNO_BREAK
}

void RenderPlatform::InvalidateDeviceObjects()
{
	// GL Insists on having a bound vertex array object, even if we're not using it in the vertex shader.
	if (empty_vao)
		glDeleteVertexArrays(1, &empty_vao);
	empty_vao=0;
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	rescaleVertexShaderConstants.InvalidateDeviceObjects();
}

void RenderPlatform::StartRender(crossplatform::DeviceContext &deviceContext)
{
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	//glFrontFace(GL_CCW);
	SetStandardRenderState(deviceContext,crossplatform::STANDARD_OPAQUE_BLENDING);
	SetStandardRenderState(deviceContext,crossplatform::STANDARD_DEPTH_LESS_EQUAL);
	solidEffect->Apply(deviceContext,solidEffect->GetTechniqueByIndex(0),0);
}

void RenderPlatform::EndRender(crossplatform::DeviceContext &deviceContext)
{
	solidEffect->Unapply(deviceContext);
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

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	if(!deviceContext.activeTechnique)
		return;
	GL_ERROR_CHECK
	glDispatchCompute(w,l,d);
	GL_ERROR_CHECK
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *debugEffect,crossplatform::EffectTechnique *tech,int pass)
{
	deviceContext;
	debugEffect;
	tech;
	pass;
}
		
void RenderPlatform::DrawMarker(crossplatform::DeviceContext &deviceContext,const double *matrix)
{
    glColor3f(0.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	
    
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
	
	
    
   // glMultMatrixd((double*) pGlobalPosition);

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
	
	
    
}

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &,const double *pGlobalPosition, double pRoll)
{
    glColor3d(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	
    
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
    
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
    
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend)
{
GL_ERROR_CHECK
	debugConstants.multiplier=mult;
	if(blend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	GL_ERROR_CHECK
	glDisable(GL_CULL_FACE);
	const char *techname="textured";
	if(tex&&tex->GetDimension()==3)
	{
		techname="show_volume";
		debugEffect->SetTexture(deviceContext,"volumeTexture",tex);
	}
	else
	{
		debugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	DrawQuad(deviceContext,x1,y1,dx,dy,debugEffect,debugEffect->GetTechniqueByName(techname),"noblend");
GL_ERROR_CHECK
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect
	,crossplatform::EffectTechnique *technique,const char *pass)
{
	if(false)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
GL_ERROR_CHECK
glDisable(GL_CULL_FACE);
glDisable(GL_DEPTH_TEST);

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
	effect->Apply(deviceContext,technique,pass);
	GL_ERROR_CHECK
	vec4 r(2.f*(float)x1/(float)viewport.Width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
		,2.f*(float)dx/(float)viewport.Width
		,2.f*(float)dy/(float)viewport.Height);
	GL_ERROR_CHECK
	debugConstants.LinkToEffect(effect,"DebugConstants");
	debugConstants.rect=r;
	debugConstants.Apply(deviceContext);
	DrawQuad(deviceContext);
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
	GL_ERROR_CHECK
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	if(!deviceContext.activeTechnique)
		return;
	GL_ERROR_CHECK
	rescaleVertexShaderConstants.rescaleVertexShaderY=opengl::FramebufferGL::IsTargetTexture()?-1.0f:1.0f;
	rescaleVertexShaderConstants.Apply(deviceContext);
	// GL Insists on having a bound vertex array object, even if we're not using it in the vertex shader.
	glBindVertexArray(empty_vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
	GL_ERROR_CHECK
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

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *m,const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 model(m);
	crossplatform::MakeWorldViewProjMatrix(wvp,model,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	solidConstants.worldViewProj=wvp;
	solidConstants.worldViewProj.transpose();
	solidConstants.world=model;
	solidConstants.world.transpose();
	
	solidConstants.lightIrradiance	=physicalLightRenderData.lightColour;
	solidConstants.lightDir			=physicalLightRenderData.dirToLight;
	solidConstants.Apply(deviceContext);
	
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
	if(fileNameUtf8&&strcmp(fileNameUtf8,"ESRAM")!=0)
		tex->LoadFromFile(this,fileNameUtf8);
	return tex;
}

crossplatform::BaseFramebuffer	*RenderPlatform::CreateFramebuffer()
{
	opengl::FramebufferGL * b=new opengl::FramebufferGL;
	return b;
}
static GLenum toGLWrapping(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch(w)
	{
	case crossplatform::SamplerStateDesc::WRAP:
		return GL_REPEAT;
		break;
	case crossplatform::SamplerStateDesc::CLAMP:
		return GL_CLAMP_TO_EDGE;
		break;
	case crossplatform::SamplerStateDesc::MIRROR:
		return GL_MIRRORED_REPEAT;
		break;
	default:
		break;
	}
	return GL_REPEAT;
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *desc)
{
	opengl::SamplerState *s=new opengl::SamplerState();
	glGenSamplers(1,&s->sampler_state);
	switch(desc->filtering)
	{
	case crossplatform::SamplerStateDesc::POINT:
		glSamplerParameteri(s->sampler_state,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glSamplerParameteri(s->sampler_state,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		break;
	case crossplatform::SamplerStateDesc::LINEAR:
		glSamplerParameteri(s->sampler_state,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glSamplerParameteri(s->sampler_state,GL_TEXTURE_MIN_FILTER,GL_LINEAR);// GL_NEAREST_MIPMAP_LINEAR etc causes problems here.
		break;
	default:
		break;
	}
	glSamplerParameteri(s->sampler_state,GL_TEXTURE_WRAP_S,toGLWrapping(desc->x));
	glSamplerParameteri(s->sampler_state,GL_TEXTURE_WRAP_T,toGLWrapping(desc->y));
	glSamplerParameteri(s->sampler_state,GL_TEXTURE_WRAP_R,toGLWrapping(desc->z));
	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
GL_ERROR_CHECK
	ERRNO_BREAK
	opengl::Effect *e=new opengl::Effect();
	e->Load(this,filename_utf8,defines);
GL_ERROR_CHECK
	e->SetName(filename_utf8);
	if(e->platform_effect==(void*)0xFFFFFFFF)
	{
		// We're still going to return a valid object - it just won't have any valid techniques.
		return e;
	}
	rescaleVertexShaderConstants.LinkToEffect(e,"RescaleVertexShaderConstants");
GL_ERROR_CHECK
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	return new opengl::PlatformConstantBuffer();
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new opengl::PlatformStructuredBuffer();
	return b;
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
	case RGBA_32_UINT:
		return GL_RGBA32UI;
	case RGB_32_FLOAT:
		return GL_RGB32F;
	case RG_32_FLOAT:
		return GL_RG32F;
	case R_32_FLOAT:
		return GL_R32F;
	case R_16_FLOAT:
		return GL_R16F;
	case LUM_32_FLOAT:
		return GL_LUMINANCE32F_ARB;
	case INT_32_FLOAT:
		return GL_INTENSITY32F_ARB;
	case RGBA_8_UNORM:
		return GL_RGBA8;
	case RGBA_8_SNORM:
		return GL_RGBA8_SNORM;
	case RGB_8_UNORM:
		return GL_RGB8;
	case RGB_8_SNORM:
		return GL_RGB8_SNORM;
	case R_8_UNORM:
		return GL_R8;// not GL_R...!
	case R_32_UINT:
		return GL_R32UI;
	case RG_32_UINT:
		return GL_RG32UI;
	case RGB_32_UINT:
		return GL_RGB32UI;
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT32F;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT16;
	default:
		return 0;
	};
}

crossplatform::PixelFormat RenderPlatform::FromGLFormat(GLuint p)
{
	using namespace crossplatform;
	switch(p)
	{
		case GL_RGBA16F:
			return RGBA_16_FLOAT;
		case GL_RGBA32F:
			return RGBA_32_FLOAT;
		case GL_RGBA32UI:
			return RGBA_32_UINT;
		case GL_RGB32F:
			return RGB_32_FLOAT;
		case GL_RG32F:
			return RG_32_FLOAT;
		case GL_R32F:
			return R_32_FLOAT;
		case GL_R16F:
			return R_16_FLOAT;
		case GL_LUMINANCE32F_ARB:
			return LUM_32_FLOAT;
		case GL_INTENSITY32F_ARB:
			return INT_32_FLOAT;
		case GL_RGBA8:
			return RGBA_8_UNORM;
		case GL_RGBA8_SNORM:
			return RGBA_8_SNORM;
		case GL_RGB8:
			return RGB_8_UNORM;
		case GL_RGB8_SNORM:
			return RGB_8_SNORM;
		case GL_R8:// not GL_R...!
			return R_8_UNORM;
		case GL_R32UI:
			return R_32_UINT;
		case GL_RG32UI:
			return RG_32_UINT;
		case GL_RGB32UI:
			return RGB_32_UINT;
		case GL_DEPTH_COMPONENT32F:
			return D_32_FLOAT;
		case GL_DEPTH_COMPONENT16:
			return D_16_UNORM;
	default:
		return UNKNOWN;
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
	case RGBA_32_UINT:
		return GL_RGBA_INTEGER;
	case R_32_UINT:
		return GL_RED_INTEGER;
	case RG_32_UINT:
		return GL_RG_INTEGER;
	case RGB_32_UINT:
		return GL_RGB_INTEGER;
	case RG_32_FLOAT:
		return GL_RG;
	case R_16_FLOAT:
		return GL_RED;
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
	case RGB_8_UNORM:
		return GL_RGB;
	case RGB_8_SNORM:
		return GL_RGB;
	case R_8_UNORM:
		return GL_RED;// not GL_R...!
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT;
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
	case R_16_FLOAT:
		return 1;
	case LUM_32_FLOAT:
		return 1;
	case INT_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
		return 4;
	case RGBA_8_SNORM:
		return 4;
	case RGB_8_UNORM:
		return 3;
	case RGB_8_SNORM:
		return 3;
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
	case D_16_UNORM:
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
	case R_16_FLOAT:
		return GL_FLOAT;
	case LUM_32_FLOAT:
		return GL_FLOAT;
	case INT_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_SNORM:
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
	case D_16_UNORM:
		return GL_UNSIGNED_SHORT;
	default:
		return 0;
	};
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc)
{
	opengl::Layout *l=new opengl::Layout();
	l->SetDesc(desc,num_elements);
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
void RenderPlatform::SetRenderState(crossplatform::DeviceContext &,const crossplatform::RenderState *s)
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

				d.RenderTargetWriteMask;
				glColorMask(d.RenderTargetWriteMask&1
							,d.RenderTargetWriteMask&2
							,d.RenderTargetWriteMask&4
							,d.RenderTargetWriteMask&8);
			}
		}
		else
			glDisable(GL_BLEND);
	}
	else if(S->desc.type==crossplatform::DEPTH)
	{
		if(S->desc.depth.test)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		if(S->desc.depth.write)
			glDepthMask(GL_TRUE);
		else
			glDepthMask(GL_FALSE);
		glDisable(GL_STENCIL_TEST);
		glDepthFunc(toGlComparison(S->desc.depth.comparison));
	}
}
void RenderPlatform::Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	GL_ERROR_CHECK
	//glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, ((opengl::Texture*)source)->FramebufferAsGLuint() );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER,  ((opengl::Texture*)destination)->FramebufferAsGLuint() );
	/*if (offscreen.depth&&ms_offscreen.depth)
	{
	glBlitFramebuffer( 0, 0, vpw, vph, 0, 0, vpw, vph, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	glError();
	}*/
	glBlitFramebuffer( 0, 0, source->width, source->length, 0, 0, destination->width, destination->length, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	GL_ERROR_CHECK
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	SaveGLImage(lFileNameUtf8,texture->AsGLuint(),true);
}

void *RenderPlatform::GetDevice()
{
	return NULL;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &,int ,int num_buffers,crossplatform::Buffer **buffers,const crossplatform::Layout *layout,const int *vertexStep)
{
	GL_ERROR_CHECK
	glBindVertexArray(((opengl::Buffer*)buffers[0])->vao );
	GL_ERROR_CHECK
}

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &,crossplatform::Buffer *buffer,int start_index)
{
	if (buffer)
	{
		opengl::Buffer *glbuffer=static_cast<opengl::Buffer *>(buffer);
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,  glbuffer->TransformFeedbackAsGLuint());
	}
	else
	{
		glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,0);
	}
}

static GLuint m_fb=0;
void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	PushRenderTargets(deviceContext);
	// We don't know what textures we will be passed here, so we must CREATE a new framebuffer:
	{
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		glGenFramebuffers(1, &m_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		for(int i=0;i<num;i++)
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i, GL_TEXTURE_2D, targs[i]?targs[i]->AsGLuint():0, 0);
		if(depth)
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth?depth->AsGLuint():0, 0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
		{
			FramebufferGL::CheckFramebufferStatus();
			SIMUL_BREAK("Framebuffer incomplete for RenderPlatform::ActivateRenderTargets");
		}
	}
	GL_ERROR_CHECK
	FramebufferGL::CheckFramebufferStatus();
	GL_ERROR_CHECK
	if(targs&&targs[0])
	{
		int w=targs[0]->width;
		int h=targs[1]->length;
		glViewport(0,0,w,h);
	}
	GL_ERROR_CHECK
	FramebufferGL::fb_stack.push(m_fb);
	GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(num, buffers);
	GL_ERROR_CHECK
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	GL_ERROR_CHECK
	GLuint popped_fb=FramebufferGL::fb_stack.top();
	FramebufferGL::fb_stack.pop();
	SIMUL_ASSERT(m_fb==popped_fb);
	PopRenderTargets(deviceContext);
	SAFE_DELETE_FRAMEBUFFER(m_fb);
	GL_ERROR_CHECK
}
crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &,int index)
{
	crossplatform::Viewport viewport;
	GL_ERROR_CHECK
	glGetIntegerv(GL_VIEWPORT,(int*)(&viewport));
	GL_ERROR_CHECK
	return viewport;
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &,int num,crossplatform::Viewport *vps)
{
	GL_ERROR_CHECK
	glViewport(vps->x,vps->y,vps->w,vps->h);
	GL_ERROR_CHECK
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &,crossplatform::Buffer *buffer)
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
//	
	GL_ERROR_CHECK
    
    
	GL_ERROR_CHECK
    
    
	GL_ERROR_CHECK
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
    
    
	GL_ERROR_CHECK
    
    
	GL_ERROR_CHECK
//	
	GL_ERROR_CHECK
}

void RenderPlatform::PushRenderTargets(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	GLint current_fb=0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING,&current_fb);
	crossplatform::Viewport viewport;
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT,vp);
	viewport.x=vp[0];
	viewport.y=vp[1];
	viewport.w=vp[2];
	viewport.h=vp[3];
	FramebufferGL::fb_stack.push(current_fb);
	viewport_stack.push_back(viewport);
	GL_ERROR_CHECK
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	GLuint last_fb=FramebufferGL::fb_stack.top();
    glBindFramebuffer(GL_FRAMEBUFFER,last_fb);
	crossplatform::Viewport viewport=viewport_stack.back();
	GLint vp[]={viewport.x,viewport.y,viewport.w,viewport.h};
	glViewport(vp[0],vp[1],vp[2],vp[3]);
	FramebufferGL::fb_stack.pop();
	viewport_stack.pop_back();
	GL_ERROR_CHECK
	//TODO: Better implementation of glDrawBuffers
	//GLuint fb=FramebufferGL::fb_stack.size()?FramebufferGL::fb_stack.top():0;
	//glBindFramebuffer(0,fb);
	GL_ERROR_CHECK
	if(last_fb)
	{
		const GLenum buffers[9] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
		glDrawBuffers(1, buffers);
	GL_ERROR_CHECK
	}
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

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	if(!deviceContext.activeTechnique)
		return;
	rescaleVertexShaderConstants.rescaleVertexShaderY=opengl::FramebufferGL::IsTargetTexture()?-1.0f:1.0f;
	rescaleVertexShaderConstants.Apply(deviceContext);
	
	GLint current_vao;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&current_vao);
	// GL Insists on having a bound vertex array object, even if we're not using it in the vertex shader.
	if(current_vao==0)
		glBindVertexArray(empty_vao);
	glDrawArrays(toGLTopology(currentTopology), start_vert, num_verts); 
}

void RenderPlatform::DrawIndexed		(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
	if(!deviceContext.activeTechnique)
		return;
	GL_ERROR_CHECK
	rescaleVertexShaderConstants.rescaleVertexShaderY=opengl::FramebufferGL::IsTargetTexture()?-1.0f:1.0f;
	rescaleVertexShaderConstants.Apply(deviceContext);
	//glDrawElements(toGLTopology(currentTopology),num_indices,GL_UNSIGNED_SHORT,(void*)base_vertex);
	GL_ERROR_CHECK
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &,crossplatform::PosColourVertex *lines,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
	
	glDisable(GL_ALPHA_TEST);
    test_depth?glEnable(GL_DEPTH_TEST):glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glBegin(strip?GL_LINE_STRIP:GL_LINES);
	for(int i=0;i<vertex_count;i++)
	{
		const crossplatform::PosColourVertex &V=lines[i];
		glColor4f(V.colour.x,V.colour.y,V.colour.z,V.colour.w);
		glVertex3f(V.pos.x,V.pos.y,V.pos.z);
	}
	glEnd();
	glUseProgram(0);
	
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip)
{
	debugConstants.rect=vec4(0,0,1.f,1.f);//-1.0,-1.0,2.0f/viewport.Width,2.0f/viewport.Height);
	debugConstants.Apply(deviceContext);
	debugEffect->Apply(deviceContext,debugEffect->GetTechniqueByName("lines_2d"),0);
	glBegin(strip?GL_LINE_STRIP:GL_LINES);
	for(int i=0;i<vertex_count;i++)
	{
		crossplatform::PosColourVertex &V=lines[i];
		glColor4fv(V.colour);
		glTexCoord4fv(V.colour);
		glVertex3fv(V.pos);
	}
	glEnd();
	debugEffect->Unapply(deviceContext);
	
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
