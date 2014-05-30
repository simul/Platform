#define NOMINMAX
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/Material.h"
#include "Simul/Platform/DirectX11/Mesh.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/Light.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/TextRenderer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Camera/Camera.h"

using namespace simul;
using namespace dx11;
namespace simul
{
	namespace dx11
	{
		TextRenderer textRenderer;
	}
}


RenderPlatform::RenderPlatform()
	:effect(NULL)
	,reverseDepth(false)
	,mirrorY(false)
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
	textRenderer.RestoreDeviceObjects(device);
	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	solidConstants.InvalidateDeviceObjects();
	textRenderer.InvalidateDeviceObjects();
	SAFE_RELEASE(effect);
	for(std::set<scene::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		mat->effect=effect;
		delete mat;
	}
	materials.clear();
}

void RenderPlatform::RecompileShaders()
{
	std::map<std::string,std::string> defines;
	if(reverseDepth)
		defines["REVERSE_DEPTH"]="1";
	SAFE_RELEASE(effect);
	if(!device)
		return;
	CreateEffect(device,&effect,"solid.fx",defines);
	solidConstants.LinkToEffect(effect,"SolidConstants");
	//solidConstants.LinkToProgram(solid_program,"SolidConstants",1);
	for(std::set<scene::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		mat->effect=effect;
	}
	textRenderer.RecompileShaders();
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

void MakeWorldViewProjMatrix(float *wvp,const double *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,view(v),proj(p),model(w);
	simul::math::Multiply4x4(tmp1,model,view);
	simul::math::Multiply4x4(*(simul::math::Matrix4x4*)wvp,tmp1,proj);
}

void RenderPlatform::SetModelMatrix(void *context,const crossplatform::ViewStruct &viewStruct,const double *m)
{
//	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
//	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,viewStruct.view,viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	solidConstants.worldViewProj=modelviewproj;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	solidConstants.Apply(pContext);

	effect->GetTechniqueByName("solid")->GetPassByIndex(0)->Apply(0,pContext);
}

scene::Material *RenderPlatform::CreateMaterial()
{
	dx11::Material *mat=new dx11::Material;
	mat->effect=effect;
	materials.insert(mat);
	return mat;
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

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::DrawTexture(void *context,int x1,int y1,int dx,int dy,void *t,float mult)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3DX11Effect		*m_pDebugEffect=UtilityRenderer::GetDebugEffect();
	ID3D11ShaderResourceView *srv=(ID3D11ShaderResourceView*)t;
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",srv);
	simul::dx11::setParameter(m_pDebugEffect,"multiplier",mult);
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	if(srv)
		srv->GetDesc(&desc);
	ID3DX11EffectTechnique *tech=m_pDebugEffect->GetTechniqueByName("textured");
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	if(msaa)
	{
		tech=m_pDebugEffect->GetTechniqueByName("textured");
		simul::dx11::setTexture(m_pDebugEffect,"imageTextureMS",srv);
	}
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
		y1=(int)viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(pContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,m_pDebugEffect,tech);
	}
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",NULL);
}

void RenderPlatform::DrawQuad		(void *context,int x1,int y1,int dx,int dy,void *effect,void *technique)
{
	ID3D11DeviceContext		*pContext	=(ID3D11DeviceContext *)context;
	ID3DX11Effect			*eff		=(ID3DX11Effect	*)effect;
	ID3DX11EffectTechnique	*tech		=(ID3DX11EffectTechnique*)technique;
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
		y1=viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(pContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,eff,tech);
	}
}

void RenderPlatform::Print(void *context,int x,int y	,const char *text)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	float clr[]={1.f,1.f,0.f,1.f};
	float black[]={0.f,0.f,0.f,0.5f};
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	int h=viewport.Height;
	textRenderer.Render(pContext,(float)x,(float)y,(float)viewport.Width,(float)h,text,clr,black,mirrorY);
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	simul::dx11::UtilityRenderer::DrawLines(deviceContext,(VertexXyzRgba*)lines,vertex_count,strip);
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	simul::dx11::UtilityRenderer::Draw2dLines(deviceContext,(VertexXyzRgba*)lines,vertex_count,strip);
}

void RenderPlatform::DrawCircle(crossplatform::DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill)
{
	ID3D11DeviceContext *pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(fill?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	pContext->IASetInputLayout(NULL);
	{
		ID3DX11EffectGroup *g=dx11::UtilityRenderer::GetDebugEffect()->GetGroupByName("circle");
		ID3DX11EffectTechnique *tech=fill?g->GetTechniqueByName("filled"):g->GetTechniqueByName("outline");
		
		simul::math::Vector3 d(dir);
		d.Normalize();
		float Yaw=atan2(d.x,d.y);
		float Pitch=-asin(d.z);
		simul::math::Matrix4x4 world, tmp1, tmp2;
	
		simul::geometry::SimulOrientation ori;
		ori.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		ori.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world	=ori.T4;
		//set up matrices
		math::Matrix4x4 view=deviceContext.viewStruct.view;
		view._41=0.f;
		view._42=0.f;
		view._43=0.f;
		simul::math::Multiply4x4(tmp1,world,view);
		simul::math::Multiply4x4(tmp2,tmp1,deviceContext.viewStruct.proj);
		camera::MakeWorldViewProjMatrix(tmp2,world,view,deviceContext.viewStruct.proj);
		dx11::setMatrix(UtilityRenderer::GetDebugEffect(),"worldViewProj",&tmp2._11);
		dx11::setParameter(UtilityRenderer::GetDebugEffect(),"radius",rads);
		dx11::setParameter(UtilityRenderer::GetDebugEffect(),"colour",colr);
		ApplyPass(pContext,tech->GetPassByIndex(0));
	}
	pContext->Draw(fill?64:32,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::PrintAt3dPos(void *context,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	//renderPlatform->PrintAt3dPos((ID3D11DeviceContext *)context,p,text,colr,offsetx,offsety);
}
