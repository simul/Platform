// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include <GL/glew.h>
#include "SimulGLLightningRenderer.h"
#include <math.h>

#include "Simul/Base/SmartPtr.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Sky/Float4.h"
#include "LoadGLImage.h"
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

using namespace simul::clouds;
extern void printProgramInfoLog(GLuint obj);

SimulGLLightningRenderer::SimulGLLightningRenderer(simul::clouds::LightningRenderInterface *lightningRenderInterface) :
	simul::clouds::BaseLightningRenderer(lightningRenderInterface)
	,lightning_texture(0)
{
}

SimulGLLightningRenderer::~SimulGLLightningRenderer()
{
}

void SimulGLLightningRenderer::RestoreDeviceObjects()
{
	RecompileShaders();
	CreateLightningTexture();
}

void SimulGLLightningRenderer::RecompileShaders()
{
	lightning_program			=MakeProgramWithGS("simul_lightning");//
	lightningTexture_param		=glGetUniformLocation(lightning_program,"lightningTexture");
	
	printProgramInfoLog(lightning_program);
	glUseProgram(NULL);
}

void SimulGLLightningRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(lightning_program);
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

void GetCameraPosVector(simul::math::Vector3 &cam_pos,simul::math::Vector3 &cam_dir)
{
	simul::math::Matrix4x4 modelview;
	glGetMatrix(modelview.RowPointer(0),GL_MODELVIEW_MATRIX);
	simul::math::Matrix4x4 inv;
	modelview.Inverse(inv);
	cam_pos.x=inv(3,0);
	cam_pos.y=inv(3,1);
	cam_pos.z=inv(3,2);

	
	cam_dir.x=inv(2,0);
	cam_dir.y=inv(2,1);
	cam_dir.z=inv(2,2);
}

void SimulGLLightningRenderer::Render()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(0,"SimulGLLightningRenderer::Render");
ERROR_CHECK
/*	m_pd3dDevice->SetTexture(0,lightning_texture);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
		*/
	//set up matrices
/*	D3DXMATRIX wvp;
	D3DXMatrixIdentity(&world);
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_WORLD,&world);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);

	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	D3DXVECTOR4 cam_pos=GetCameraPosVector(view);
*/
ERROR_CHECK
	simul::math::Vector3 view_dir,cam_pos;
	GetCameraPosVector(cam_pos,view_dir);
ERROR_CHECK
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_1D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D,lightning_texture);
	glUseProgram(lightning_program);
	glUniform1i(lightningTexture_param,0);
	simul::math::Matrix4x4 view,proj,viewproj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Multiply4x4(viewproj,view,proj);
setMatrix(lightning_program,"worldViewProj",viewproj.RowPointer(0));
	int main_viewport[4];
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	float vpx=main_viewport[2];
	float vpy=main_viewport[3];
	setParameter(lightning_program,"viewportPixels",vpx,vpy);
	int texcoord_index=glGetAttribLocation(lightning_program,"in_coord");
ERROR_CHECK
	simul::math::Vector3 pos;
	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;

	for(int i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->IsSourceStarted(i))
			continue;
		bool quads=false;
		simul::math::Vector3 x1,x2;
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(int j=0;j<lightningRenderInterface->GetNumLevels(i);j++)
		{
			for(int jj=0;jj<lightningRenderInterface->GetNumBranches(i,j);jj++)
			{
				const simul::clouds::LightningRenderInterface::Branch &branch=lightningRenderInterface->GetBranch(i,j,jj);
				if(branch.width<.1f)
				{
					quads=false;
				}
				simul::math::Vector3 last_transverse;
				vert_start=vert_num;
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE,GL_ONE);
				/*if(quads)
				{
					glBegin(GL_TRIANGLE_STRIP);
				}
				else*/
					glBegin(GL_LINE_STRIP_ADJACENCY);
				for(int k=0;k<branch.numVertices;k++)
				{
					x1=(const float *)branch.vertices[k];
					if(k+1<branch.numVertices)
						x2=(const float *)branch.vertices[k+1];
					else
					{
						x2=2.f*x1-simul::math::Vector3((const float *)branch.vertices[k-1]);
					}
					bool start=(k==0);
					bool end=(k==branch.numVertices-1);
					simul::math::Vector3 dir=x2-x1;
					dir.Normalize();
					static float ww=100.f;
					float width=branch.brightness*branch.width;
					width*=ww;
					simul::math::Vector3 transverse;
					view_dir=x1-cam_pos;
					CrossProduct(transverse,view_dir,dir);
					transverse.Unit();
					transverse*=width;
					float brightness=branch.brightness;
					if(end)
						brightness=0.f;
					glVertexAttrib2f(texcoord_index,width,brightness);
					glVertex3f(x1.x,x1.y,x1.z+vertical_shift);
					last_transverse=transverse;
				}
				glEnd();
				vert_num=0;
				int err=glGetError();
				if(err!=0)
				{
					glDisable(GL_TEXTURE_1D);
					glPopAttrib();
					return;
				}
			}
		}
	}
	glDisable(GL_TEXTURE_1D);
	ERROR_CHECK
	glPopAttrib();
}

bool SimulGLLightningRenderer::CreateLightningTexture()
{
	unsigned size=64;
	unsigned char *lightning_tex_data=new unsigned char[size*4];
	glGenTextures(1,&lightning_texture);
	glEnable(GL_TEXTURE_1D);
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D,lightning_texture);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_T,GL_REPEAT);


	const float *lightning_colour=lightningRenderInterface->GetLightningColour();
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float s=linear*2.f;
		if(s>1.f)
			s=2.f-s;
		float u=s/0.2f;
		if(u>1.f)
			u=1.f;
		float level=u;
		float r=lightning_colour[0]*level;
		float g=lightning_colour[1]*level;
		float b=lightning_colour[2]*level;
		float a=level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		if(a>1.f)
			a=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*r);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*a);
	}
	glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA8,size,0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,0);
	glTexSubImage1D(
		GL_TEXTURE_1D,
		0,0,
		size,
		GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
		lightning_tex_data);
	ERROR_CHECK
	delete [] lightning_tex_data;
	glDisable(GL_TEXTURE_1D);
	return true;
}
