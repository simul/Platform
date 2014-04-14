#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/Material.h"
#include "Simul/Platform/DirectX11/Mesh.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/Light.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"

using namespace simul;
using namespace dx11;

RenderPlatform::RenderPlatform()
	:effect(NULL)
{
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::RestoreDeviceObjects(void *d)
{
	device=(ID3D11Device*)d;
	solidConstants.RestoreDeviceObjects(device);
	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	solidConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(effect);
}

void RenderPlatform::RecompileShaders()
{
	std::map<std::string,std::string> defines;
	SAFE_RELEASE(effect);
	CreateEffect(device,&effect,"solid.fx",defines);
	solidConstants.LinkToEffect(effect,"SolidConstants");
	//solidConstants.LinkToProgram(solid_program,"SolidConstants",1);
}

void RenderPlatform::PushTexturePath(const char *pathUtf8)
{
	simul::dx11::PushTexturePath(pathUtf8);
}

void RenderPlatform::PopTexturePath()
{
	simul::dx11::PopTexturePath();
}

void RenderPlatform::StartRender()
{
	/*glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_LIGHTING_BIT);
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glUseProgram(solid_program);*/
}

void RenderPlatform::EndRender()
{
	/*glUseProgram(0);
	glPopAttrib();
	glPopAttrib();*/
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
 /*   glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_DIRECTION_LIGHT_POSITION);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, DEFAULT_LIGHT_COLOR);
    glLightfv(GL_LIGHT0, GL_SPECULAR, DEFAULT_LIGHT_COLOR);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, DEFAULT_LIGHT_SPOT_CUTOFF);
    glEnable(GL_LIGHT0);
    // Set ambient light.
    GLfloat lAmbientLight[] = {static_cast<GLfloat>(pAmbientLight[0]), static_cast<GLfloat>(pAmbientLight[1]),static_cast<GLfloat>(pAmbientLight[2]), 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lAmbientLight);*/
}

void RenderPlatform::DrawMarker(void *context,const double *matrix)
{
 /*   glColor3f(0.0, 1.0, 1.0);
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
    glPopMatrix();*/
}

void RenderPlatform::DrawLine(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
/*    glColor3f(colour[0],colour[1],colour[2]);
    glLineWidth(width);

    glBegin(GL_LINES);

    glVertex3dv((const GLdouble *)pGlobalBasePosition);
    glVertex3dv((const GLdouble *)pGlobalEndPosition);

    glEnd();*/
}

void RenderPlatform::DrawCrossHair(void *context,const double *pGlobalPosition)
{
/*    glColor3f(1.0, 1.0, 1.0);
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
    glPopMatrix();*/
}

void RenderPlatform::DrawCamera(void *context,const double *pGlobalPosition, double pRoll)
{
 /*   glColor3d(1.0, 1.0, 1.0);
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
    glPopMatrix();*/
}

void RenderPlatform::DrawLineLoop(void *context,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
/*    glPushMatrix();
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    glPopMatrix();*/
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const float BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
/*    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);*/
}
#include "Simul/Math/Matrix4x4.h"
void MakeWorldViewProjMatrix(float *wvp,const double *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,view(v),proj(p),model(w);
	simul::math::Multiply4x4(tmp1,model,view);
	simul::math::Multiply4x4(*(simul::math::Matrix4x4*)wvp,tmp1,proj);
}

void RenderPlatform::SetModelMatrix(void *context,const double *m)
{
	simul::math::Matrix4x4 proj;
//	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	simul::math::Matrix4x4 view;
//	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,view,proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	solidConstants.worldViewProj=modelviewproj;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	solidConstants.Apply(pContext);
}

scene::Material *RenderPlatform::CreateMaterial()
{
	return new dx11::Material;
}

scene::Mesh *RenderPlatform::CreateMesh()
{
	return new dx11::Mesh;
}

scene::Light *RenderPlatform::CreateLight()
{
	return new dx11::Light();
}

scene::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	scene::Texture * tex=new dx11::Texture(device);
	tex->LoadFromFile(fileNameUtf8);
	return tex;
}

void RenderPlatform::DrawTexture(void *context,int x1,int y1,int dx,int dy,void *t,float mult)
{
	if(!t)
		return;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3DX11Effect		*m_pDebugEffect=UtilityRenderer::GetDebugEffect();
	ID3D11ShaderResourceView *srv=(ID3D11ShaderResourceView*)t;
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",srv);
	simul::dx11::setParameter(m_pDebugEffect,"multiplier",mult);
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	srv->GetDesc(&desc);
	ID3DX11EffectTechnique *tech=m_pDebugEffect->GetTechniqueByName("textured");
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	if(msaa)
	{
		tech=m_pDebugEffect->GetTechniqueByName("textured");
		simul::dx11::setTexture(m_pDebugEffect,"imageTextureMS",srv);
	}
	UtilityRenderer::DrawQuad2(pContext,x1,y1,dx,dy,m_pDebugEffect,tech);
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",NULL);
}
